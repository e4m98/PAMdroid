/*
 * Android–Linux 근접 인증 PAM 우회 데몬
 * Build: g++ -std=c++20
 * Requires: libsdbus-c++-dev, libglib2.0-dev, libsystemd-dev
 */

#include <sdbus-c++/sdbus-c++.h>
#include <glib.h>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

static constexpr int16_t RSSI_MIN = -76;                // 인증 허용 임계값
static const char* BLUEZ_SERVICE  = "org.bluez";
static const char* OBJ_MGR_IFACE  = "org.freedesktop.DBus.ObjectManager";
static const char* DEV_IFACE      = "org.bluez.Device1";

/* ---------- 구조체 ---------- */
struct AndroidDeviceInfo
{
    std::string mac;   // XX:XX:XX:XX:XX:XX
    std::string path;  // /org/bluez/hci0/dev_...
};

/* ---------- 시스템블루투스 활성 확인 ---------- */
static bool isBluetoothServiceActive()
{
    return system("systemctl is-active --quiet bluetooth") == 0;
}

/* ---------- Android 폰 검색 ---------- */
static AndroidDeviceInfo findConnectedAndroidPhone()
{
    AndroidDeviceInfo info;

    /* 시스템 버스 연결 */
    auto conn       = sdbus::createSystemBusConnection();
    auto objManager = sdbus::createProxy(*conn, BLUEZ_SERVICE, "/");

    /* GetManagedObjects 호출 */
    std::map<sdbus::ObjectPath,
             std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;

    objManager->callMethod("GetManagedObjects")
              .onInterface(OBJ_MGR_IFACE)
              .storeResultsTo(objects);

    for (const auto& [objPath, ifaces] : objects)
    {
        auto devIt = ifaces.find(DEV_IFACE);
        if (devIt == ifaces.end()) continue;

        const auto& props = devIt->second;

        /* 연결 여부 */
        if (!props.at("Connected").get<bool>()) continue;

        /* 안드로이드 폰 판별 (Icon, Class) */
        bool isAndroid = false;

        auto iconIt = props.find("Icon");
        if (iconIt != props.end() &&
            iconIt->second.get<std::string>() == "phone")
            isAndroid = true;

        auto classIt = props.find("Class");
        if (classIt != props.end() &&
            classIt->second.get<uint32_t>() == 0x5e020c)
            isAndroid = true;

        if (!isAndroid) continue;

        /* MAC 추출 */
        std::string mac = objPath;
        mac = mac.substr(mac.find("dev_") + 4);
        for (char& c : mac) if (c == '_') c = ':';

        info.mac  = mac;
        info.path = objPath;

        /* 하나만 사용하고 종료 */
        break;
    }
    return info;
}

/* ---------- 데몬 메인 ---------- */
static GMainLoop* gLoop = nullptr;

static void signalHandler(int)
{
    if (gLoop) g_main_loop_quit(gLoop);
}

int main()
{
    /* 1) 블루투스 서비스 확인 */
    if (!isBluetoothServiceActive())
    {
        std::cerr << "bluetooth.service inactive. Aborting.\n";
        return EXIT_FAILURE;
    }

    /* 2) 안드로이드 기기 검색 */
    AndroidDeviceInfo dev = findConnectedAndroidPhone();
    if (dev.mac.empty())
    {
        std::cerr << "No connected Android phone found.\n";
        return EXIT_FAILURE;
    }

    /* 3) 장치 프록시 생성 */
    try
    {
        auto conn = sdbus::createSystemBusConnection();
        auto proxy = sdbus::createProxy(*conn, BLUEZ_SERVICE, dev.path);
        proxy->finishRegistration();

        /* 3-1) RSSI 주기적 확인: signal or poll */
        int16_t rssi = proxy->getProperty("RSSI").onInterface(DEV_IFACE).get<int16_t>();

        std::cout << "Android MAC: " << dev.mac
                  << "  RSSI: " << rssi << " dBm\n";

        if (rssi < RSSI_MIN)
        {
            std::cerr << "RSSI below threshold.\n";
            return EXIT_FAILURE;
        }
    }
    catch (const sdbus::Error& e)
    {
        std::cerr << "DBus error: " << e.getName()
                  << " – " << e.getMessage() << '\n';
        return EXIT_FAILURE;
    }

    /* 4) 신호 처리 및 짧은 대기
     * PAM 모듈이 exec 형식으로 호출하면 즉시 종료해도 좋지만,
     * 데몬형으로 구동 시 연결 끊김 감시를 위해 GLib 루프 사용.
     */
    gLoop = g_main_loop_new(nullptr, FALSE);
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    /* 인증 성공 상태 → 0 반환, GLib 루프는 선택 */
    std::cout << "Proximity condition satisfied. sudo may proceed.\n";
    g_timeout_add_seconds(1, [](gpointer) -> gboolean {
        /* 짧은 루프 후 정상 종료 */
        g_main_loop_quit(gLoop);
        return G_SOURCE_REMOVE;
    }, nullptr);

    g_main_loop_run(gLoop);
    g_main_loop_unref(gLoop);

    return EXIT_SUCCESS;
}
