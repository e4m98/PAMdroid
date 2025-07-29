#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <systemd/sd-bus.h>
#include <glib.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define DEFAULT_RSSI_MIN -80
static const char* BLUEZ_SERVICE = "org.bluez";
static const char* OBJECT_MANAGER_IFACE = "org.freedesktop.DBus.ObjectManager";
static const char* DEVICE_IFACE = "org.bluez.Device1";

struct AndroidDeviceInfo {
    char mac[20];
    char path[256];
    int16_t rssi;
    bool connected;
};

static bool isBluetoothServiceActive(void) {
    return system("systemctl is-active --quiet bluetooth") == 0;
}

// Android 기기 검색
static struct AndroidDeviceInfo findConnectedAndroidPhone(void) {
    struct AndroidDeviceInfo result = {0};
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;
    
    // 시스템 버스 연결
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to connect to system bus: %s", 
               strerror(-r));
        goto cleanup;
    }
    
    // Call GetManagedObjects
    r = sd_bus_call_method(bus,
                          BLUEZ_SERVICE,
                          "/",
                          OBJECT_MANAGER_IFACE,
                          "GetManagedObjects",
                          &error,
                          &reply,
                          "");
    if (r < 0) {
        syslog(LOG_ERR, "Failed to call GetManagedObjects: %s", 
               error.message);
        goto cleanup;
    }
    
    // D-Bus Message Parsing
    
cleanup:
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return result;
}

static int16_t parseRssiThreshold(int argc, const char **argv) {
    int16_t threshold = DEFAULT_RSSI_MIN;
    
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "rssi_min=", 9) == 0) {
            threshold = (int16_t)atoi(argv[i] + 9);
            break;
        }
    }
    
    return threshold;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                                  int argc, const char **argv) {
    openlog("Pamdroid", LOG_PID, LOG_AUTHPRIV);
    
    if (!isBluetoothServiceActive()) {
        pam_syslog(pamh, LOG_ERR, "Bluetooth service is not active");
        closelog();
        return PAM_AUTHINFO_UNAVAIL;
    }
    
    int16_t rssiMin = parseRssiThreshold(argc, argv);
    struct AndroidDeviceInfo device = findConnectedAndroidPhone();
    
    if (strlen(device.mac) == 0) {
        pam_syslog(pamh, LOG_INFO, "No connected Android device found");
        closelog();
        return PAM_AUTH_ERR;
    }
    
    if (device.rssi < rssiMin) {
        pam_syslog(pamh, LOG_INFO, 
                   "Android device %s RSSI %d dBm below threshold %d dBm",
                   device.mac, device.rssi, rssiMin);
        closelog();
        return PAM_AUTH_ERR;
    }
    
    pam_syslog(pamh, LOG_INFO, 
               "Android device %s authenticated (RSSI: %d dBm)",
               device.mac, device.rssi);
    
    closelog();
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags,
                             int argc, const char **argv) {
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
                               int argc, const char **argv) {
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags,
                                  int argc, const char **argv) {
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv) {
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
                               int argc, const char **argv) {
    return PAM_IGNORE;
}
