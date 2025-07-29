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

static bool isAndroidDevice(const char* name, uint32_t device_class) {
    if (!name) return false;
    
    const char* android_keywords[] = {
        "android", "Android", "ANDROID",
        "안드로이드", "SAMSUNG", "Samsung",
        "samsung",
        NULL
    };
    
    for (int i = 0; android_keywords[i]; i++) {
        if (strstr(name, android_keywords[i])) {
            return true;
        }
    }
    
    uint32_t major_class = (device_class >> 8) & 0x1F;
    if (major_class == 0x02) {
        return true;
    }
    
    return false;
}

// D-Bus variant Parsing
static int parse_variant_value(sd_bus_message *reply, const char* expected_type, void* value) {
    const char* variant_type;
    int r;
    
    r = sd_bus_message_peek_type(reply, NULL, &variant_type);
    if (r < 0) return r;
    
    r = sd_bus_message_enter_container(reply, 'v', variant_type);
    if (r < 0) return r;
    
    if (strcmp(expected_type, "s") == 0 && strcmp(variant_type, "s") == 0) {
        r = sd_bus_message_read_basic(reply, 's', (const char**)value);
    } else if (strcmp(expected_type, "b") == 0 && strcmp(variant_type, "b") == 0) {
        r = sd_bus_message_read_basic(reply, 'b', (int*)value);
    } else if (strcmp(expected_type, "n") == 0 && strcmp(variant_type, "n") == 0) {
        r = sd_bus_message_read_basic(reply, 'n', (int16_t*)value);
    } else if (strcmp(expected_type, "u") == 0 && strcmp(variant_type, "u") == 0) {
        r = sd_bus_message_read_basic(reply, 'u', (uint32_t*)value);
    } else {
        r = sd_bus_message_skip(reply, variant_type);
    }
    
    if (r >= 0) {
        r = sd_bus_message_exit_container(reply);
    }
    
    return r;
}

static struct AndroidDeviceInfo findConnectedAndroidPhone(void) {
    struct AndroidDeviceInfo result = {0};
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;
    
    // connect system bus
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        syslog(LOG_ERR, "Failed to connect to system bus: %s", strerror(-r));
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
        syslog(LOG_ERR, "Failed to call GetManagedObjects: %s", error.message);
        goto cleanup;
    }
    
    // D-Bus Message Parsing - a{oa{sa{sv}}}
    r = sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}");
    if (r < 0) {
        syslog(LOG_ERR, "Failed to enter main array container: %s", strerror(-r));
        goto cleanup;
    }
    
    while ((r = sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}")) > 0) {
        const char *object_path;
        
        r = sd_bus_message_read_basic(reply, 'o', &object_path);
        if (r < 0) {
            syslog(LOG_ERR, "Failed to read object path: %s", strerror(-r));
            break;
        }
        
        // (/org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX)
        if (!strstr(object_path, "/dev_")) {
            sd_bus_message_skip(reply, "a{sa{sv}}");
            sd_bus_message_exit_container(reply);
            continue;
        }
        
        r = sd_bus_message_enter_container(reply, 'a', "{sa{sv}}");
        if (r < 0) break;
        
        bool is_device1_interface = false;
        const char *device_name = NULL;
        int connected = 0;
        int16_t rssi = -127;
        uint32_t device_class = 0;
        
        while ((r = sd_bus_message_enter_container(reply, 'e', "sa{sv}")) > 0) {
            const char *interface_name;
            
            r = sd_bus_message_read_basic(reply, 's', &interface_name);
            if (r < 0) break;
            
            if (strcmp(interface_name, DEVICE_IFACE) == 0) {
                is_device1_interface = true;
                
                r = sd_bus_message_enter_container(reply, 'a', "{sv}");
                if (r < 0) break;
                
                // Properties
                while ((r = sd_bus_message_enter_container(reply, 'e', "sv")) > 0) {
                    const char *property_name;
                    
                    r = sd_bus_message_read_basic(reply, 's', &property_name);
                    if (r < 0) break;
                    
                    if (strcmp(property_name, "Name") == 0) {
                        parse_variant_value(reply, "s", &device_name);
                    } else if (strcmp(property_name, "Connected") == 0) {
                        parse_variant_value(reply, "b", &connected);
                    } else if (strcmp(property_name, "RSSI") == 0) {
                        parse_variant_value(reply, "n", &rssi);
                    } else if (strcmp(property_name, "Class") == 0) {
                        parse_variant_value(reply, "u", &device_class);
                    } else {
                        sd_bus_message_skip(reply, "v");
                    }
                    
                    sd_bus_message_exit_container(reply);
                }
                
                sd_bus_message_exit_container(reply);
            } else {
                sd_bus_message_skip(reply, "a{sv}");
            }
            
            sd_bus_message_exit_container(reply);
        }
        
        sd_bus_message_exit_container(reply);
        
        // Android 기기이고 연결된 경우 결과 저장
        if (is_device1_interface && connected && 
            isAndroidDevice(device_name, device_class)) {
            
            // MAC
            const char *dev_prefix = strstr(object_path, "/dev_");
            if (dev_prefix) {
                const char *mac_part = dev_prefix + 5;
                strncpy(result.mac, mac_part, sizeof(result.mac) - 1);
                result.mac[sizeof(result.mac) - 1] = '\0';
                
                for (int i = 0; result.mac[i]; i++) {
                    if (result.mac[i] == '_') {
                        result.mac[i] = ':';
                    }
                }
            }
            
            strncpy(result.path, object_path, sizeof(result.path) - 1);
            result.path[sizeof(result.path) - 1] = '\0';
            result.rssi = rssi;
            result.connected = true;
            
            syslog(LOG_INFO, "Found connected Android device: %s (MAC: %s, RSSI: %d dBm)", 
                   device_name ? device_name : "Unknown", result.mac, result.rssi);
            break; // 첫 번째 연결된 Android 기기만 사용
        }
        
        sd_bus_message_exit_container(reply);
    }
    
    sd_bus_message_exit_container(reply);
    
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
