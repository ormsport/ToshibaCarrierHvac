/*
*   If you are not expert user, please edit in config section only.
*/

/*----------BEGIN OF CONFIG----------*/
// Hostname
const char hostname[] = "beyond-hvac";
//use chip id for mqtt client id.
#define use_chip_id

// WiFi Client
const char* ssid      = "";
const char* password  = "";

// MQTT Settings
const char* mqtt_host = "";
const char* mqtt_port = "1883";
uint8_t     qossub    = 0;
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_settings_in_topic = "hvac/settings/set";
const char* mqtt_settings_out_topic = "hvac/settings";
const char* mqtt_status_out_topic = "hvac/status";
#define MQTT_MAX_RECONNECT_TRIES    5   // Max connection retries before give up
#define MQTT_CONNECT_GIVEUP_DELAY   1   // Stop reconnect to mqtt host for x mintue(s) then try to connect again

// HTTP Update Server Settings
const char* update_path = "/update";    // Web OTA update path
const char* update_username = "admin";  // Web OTA update username
const char* update_password = "admin";  // Web OTA update password

// ToshibaCarrierHvac Settings
//ToshibaCarrierHvac hvac(D5, D6);     // To use SoftwareSerial with ESP8266 and AVR please uncommemt this line
//ToshibaCarrierHvac hvac(&Serial2);   // To use HardwareSerial with ESP8266, ESP32 and AVR please uncomment this line

/*----------END OF CONFIG----------*/



//If using chipid
#ifdef use_chip_id
char mqtt_clientid[64];
#else
const char* mqtt_clientid = nickname;
#endif

uint32_t mqtt_conn_abort_time = 0;
uint8_t mqtt_reconnect_retries = 0;
boolean mqtt_conn_abort = false;