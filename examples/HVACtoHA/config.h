/*
*   If you are not expert user, please edit in config section only.
*/

/*----------BEGIN OF CONFIG----------*/
// Hostname
const char hostname[] = "beyond-hvac";

// WiFi Client
const char* ssid      = "";
const char* password  = "";

// MQTT Settings
const char* mqtt_host = "";
const char* mqtt_port = "1883";
uint8_t     qossub    = 0;
const char* mqtt_user = "";
const char* mqtt_password = "";
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


uint32_t mqtt_conn_abort_time = 0;
uint8_t mqtt_reconnect_retries = 0;
boolean mqtt_conn_abort = false;
boolean availability = true;

char unique_id[9];
char obj_id[64];
char mqtt_ha_discovery_topic[64];
char mqtt_power_command_topic[64];
char mqtt_preset_mode_command_topic[64];
char mqtt_mode_command_topic[64];
char mqtt_temperature_command_topic[64];
char mqtt_fan_mode_command_topic[64];
char mqtt_swing_mode_command_topic[64];
char mqtt_pure_command_topic[64];
char mqtt_psel_command_topic[64];
char mqtt_current_temperature_topic[64];
char mqtt_temperature_state_topic[64];
char mqtt_preset_mode_state_topic[64];
char mqtt_mode_state_topic[64];
char mqtt_fan_mode_state_topic[64];
char mqtt_swing_mode_state_topic[64];
char mqtt_outside_temperature_topic[64];
char mqtt_pure_state_topic[64];
char mqtt_psel_state_topic[64];
char mqtt_on_timer_state_topic[64];
char mqtt_off_timer_state_topic[64];
char mqtt_action_topic[64];
char mqtt_init_topic[64];
char mqtt_availability_topic[64];