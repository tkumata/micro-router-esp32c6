#include "Config.h"

const char* AP_SSID = "micro-router-esp32c6";
const uint8_t AP_CHANNEL = 6;
const uint8_t AP_MAX_CONNECTIONS = 3;

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const unsigned long STA_RECONNECT_INTERVAL = 5000;
const unsigned long STATUS_PRINT_INTERVAL = 10000;
const unsigned long STA_CONNECTION_TIMEOUT = 30000;
const unsigned long CONFIG_SAVE_DELAY = 5000;
const unsigned long NAT_ENABLE_DELAY = 1000;

const uint32_t MIN_FREE_HEAP_WARNING = 50000;

const char* PREF_NAMESPACE = "wifi-config";
const char* PREF_KEY_STA_SSID = "sta_ssid";
const char* PREF_KEY_STA_PASSWORD = "sta_password";
const char* PREF_KEY_AP_PASSWORD = "ap_password";
const char* PREF_KEY_STA_SSID_ENC = "sta_ssid_enc";
const char* PREF_KEY_STA_PASSWORD_ENC = "sta_pwd_enc";
const char* PREF_KEY_AP_PASSWORD_ENC = "ap_pwd_enc";
const char* PREF_KEY_CONFIGURED = "configured";
const char* PREF_KEY_AP_PASSWORD_SET = "ap_pw_set";
const char* PREF_KEY_DNS_FILTER_ENABLED = "dns_filter_en";

const char* DEFAULT_AP_PASSWORD = "esp32c6router";

const uint16_t WEB_SERVER_PORT = 80;

const unsigned long SERIAL_STABILIZE_DELAY = 1000;

const size_t WIFI_SSID_MAX_LENGTH = 32;
const size_t WIFI_PASSWORD_MIN_LENGTH = 8;
const size_t WIFI_PASSWORD_MAX_LENGTH = 64;
const size_t WIFI_SSID_BUFFER_SIZE = 33;
const size_t WIFI_PASSWORD_BUFFER_SIZE = 65;

const uint8_t WIFI_SSID_HIDDEN = 0;
const unsigned long WIFI_CONNECTION_CHECK_DELAY = 500;

const uint16_t HTTP_STATUS_OK = 200;
const uint16_t HTTP_STATUS_SEE_OTHER = 303;
const uint16_t HTTP_STATUS_BAD_REQUEST = 400;
const uint16_t HTTP_STATUS_FORBIDDEN = 403;
const uint16_t HTTP_STATUS_NOT_FOUND = 404;
const uint16_t HTTP_STATUS_INTERNAL_ERROR = 500;

const uint32_t BYTES_TO_KB_DIVISOR = 1024;
const uint32_t MILLISECONDS_TO_SECONDS_DIVISOR = 1000;
const uint8_t PERCENTAGE_MULTIPLIER = 100;

const IPAddress DEFAULT_UPSTREAM_DNS(8, 8, 8, 8);
const uint16_t DNS_FORWARD_TIMEOUT = 2000;
const unsigned long DNS_POLLING_INTERVAL = 10;

const size_t DNS_HEADER_SIZE = 12;
const uint8_t DNS_COMPRESSION_POINTER_MASK = 0xC0;
const uint8_t DNS_LABEL_MAX_LENGTH = 63;
const uint8_t DNS_RESPONSE_FLAGS_BYTE2 = 0x81;
const uint8_t DNS_RESPONSE_FLAGS_BYTE3 = 0x80;
const uint16_t DNS_COMPRESSION_POINTER_QUERY = 0xC00C;
const uint16_t DNS_TYPE_A = 0x0001;
const uint16_t DNS_CLASS_IN = 0x0001;
const uint32_t DNS_TTL_SECONDS = 300;
const uint8_t DNS_IPV4_ADDRESS_LENGTH = 4;
const IPAddress DNS_BLOCKED_IP(0, 0, 0, 0);

const size_t DOMAIN_NAME_MIN_LENGTH = 3;
const size_t DOMAIN_NAME_MAX_LENGTH = 253;

const size_t BLOCKLIST_BUFFER_SIZE = 131072;
