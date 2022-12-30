/*
*   This sketch will send climate and other features config to HA discovery topic, so you can control/monitor your HVAC from HA dashboard.
*   You need to config wifi and mqtt in "config.h" and enable discovery in HA MQTT options.
*/

#if defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
WebServer httpServer(80);
HTTPUpdateServer httpUpdater;
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#endif

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ToshibaCarrierHvac.h>

#include "config.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    /************************
    * Setup: WiFiClient
    ************************/
    Serial.printf("\n\nConnecting to %s", ssid);
    WiFi.hostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected...OK\nIP address: ");
    Serial.println(WiFi.localIP());

    /************************
    * Setup: mDNS & WebUpdate
    ************************/
    if (MDNS.begin(hostname)) Serial.printf("mDNS responder started\nWeb update url: http://%s.local/update\n", hostname);
    #if defined(ESP32)
    httpUpdater.setup(&httpServer, String(update_path), String(update_username), String(update_password));
    #else
    httpUpdater.setup(&httpServer, update_path, update_username, update_password);
    #endif
    httpServer.begin();
    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTPUpdateServer ready!\n");

    /************************
    * Setup: MQTT(PubSupClient)
    ************************/
    // get chip id
    uint32_t chip_id = 0;
    #if defined(ESP32)
    for(int i=0; i<17; i=i+8) {
        chip_id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    #else
    chip_id = ESP.getChipId();
    #endif
    // prepare mqtt topics
    snprintf(unique_id, 9, "%08X", chip_id);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/climate/%s/config", hostname);
    snprintf(mqtt_power_command_topic, 64, "%s/state/set", hostname);
    snprintf(mqtt_preset_mode_state_topic, 64, "%s/preset", hostname);
    snprintf(mqtt_preset_mode_command_topic, 64, "%s/preset/set", hostname);
    snprintf(mqtt_mode_state_topic, 64, "%s/mode", hostname);
    snprintf(mqtt_mode_command_topic, 64, "%s/mode/set", hostname);
    snprintf(mqtt_temperature_state_topic, 64, "%s/setpoint", hostname);
    snprintf(mqtt_temperature_command_topic, 64, "%s/setpoint/set", hostname);
    snprintf(mqtt_fan_mode_state_topic, 64, "%s/fan", hostname);
    snprintf(mqtt_fan_mode_command_topic, 64, "%s/fan/set", hostname);
    snprintf(mqtt_swing_mode_state_topic, 64, "%s/swing", hostname);
    snprintf(mqtt_swing_mode_command_topic, 64, "%s/swing/set", hostname);
    snprintf(mqtt_pure_state_topic, 64, "%s/pure", hostname);
    snprintf(mqtt_pure_command_topic, 64, "%s/pure/set", hostname);
    snprintf(mqtt_psel_state_topic, 64, "%s/psel", hostname);
    snprintf(mqtt_psel_command_topic, 64, "%s/psel/set", hostname);
    snprintf(mqtt_on_timer_state_topic, 64, "%s/on_timer", hostname);
    snprintf(mqtt_off_timer_state_topic, 64, "%s/off_timer", hostname);
    snprintf(mqtt_current_temperature_topic, 64, "%s/room_temperature", hostname);
    snprintf(mqtt_outside_temperature_topic, 64, "%s/outside_temperature", hostname);
    snprintf(mqtt_action_topic, 64, "%s/action", hostname);
    snprintf(mqtt_availability_topic, 64, "%s/availability", hostname);
    snprintf(mqtt_init_topic, 64, "%s/init", hostname);
    // connect to mqtt host and subscribe
    if (mqtt_host != "" && atoi(mqtt_port) > 0 && atoi(mqtt_port) < 65535) {
        Serial.printf("MQTT active: %s:%d\n", mqtt_host, String(mqtt_port).toInt());
        mqttClient.setBufferSize(1500);
        mqttClient.setServer(mqtt_host, atoi(mqtt_port));
        mqttClient.setCallback(mqtt_callback);
        while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT Server...");
        if (mqttClient.connect(hostname, mqtt_user, mqtt_password )) {
            Serial.println("Connected");
            mqttClient.subscribe(mqtt_power_command_topic, qossub);
            Serial.printf("MQTT power command topic: %s\n", mqtt_power_command_topic);
            mqttClient.subscribe(mqtt_preset_mode_command_topic, qossub);
            Serial.printf("MQTT preset command topic: %s\n", mqtt_preset_mode_command_topic);
            mqttClient.subscribe(mqtt_mode_command_topic, qossub);
            Serial.printf("MQTT mode command topic: %s\n", mqtt_mode_command_topic);
            mqttClient.subscribe(mqtt_temperature_command_topic, qossub);
            Serial.printf("MQTT setpoint command topic: %s\n", mqtt_temperature_command_topic);
            mqttClient.subscribe(mqtt_fan_mode_command_topic, qossub);
            Serial.printf("MQTT fan command topic: %s\n", mqtt_fan_mode_command_topic);
            mqttClient.subscribe(mqtt_swing_mode_command_topic, qossub);
            Serial.printf("MQTT swing command topic: %s\n", mqtt_swing_mode_command_topic);
            mqttClient.subscribe(mqtt_pure_command_topic, qossub);
            Serial.printf("MQTT pure command topic: %s\n", mqtt_pure_command_topic);
            mqttClient.subscribe(mqtt_psel_command_topic, qossub);
            Serial.printf("MQTT psel command topic: %s\n", mqtt_psel_command_topic);
            mqttClient.subscribe(mqtt_init_topic, qossub);
            Serial.printf("MQTT init command topic: %s\n", mqtt_init_topic);
        } else {
            Serial.print("failed with state ");
            Serial.println(mqttClient.state());
            Serial.println("Sleep for 5 second then try again.");
            delay(5000);
        }
        }
    } else {
        Serial.println("MQTT config invalid");
        while(1);
    }

    /************************
    * Setup: HA MQTT Discovery
    ************************/
    // climate
    if (ha_mqtt_discovery_hvac()) Serial.println("HVAC discovery sent: OK");
    // switch
    if (ha_mqtt_discovery_pure()) Serial.println("Pure discovery sent: OK");
    // select
    if (ha_mqtt_discovery_psel()) Serial.println("Power select discovery sent: OK");
    // sensor
    if (ha_mqtt_discovery_off_timer()) Serial.println("Off timer discovery sent: OK");
    if (ha_mqtt_discovery_on_timer()) Serial.println("On timer discovery sent: OK");
    if (ha_mqtt_discovery_room_temp()) Serial.println("Room temp discovery sent: OK");
    if (ha_mqtt_discovery_outside_temp()) Serial.println("Outside temp discovery sent: OK");

    /************************
    * Setup: ToshibaCarrierHvac
    ************************/
    // set callback
    Serial.print("Initializing ToshibaCarrierHvac...");
    hvac.setWhichFunctionUpdatedCallback(hvacCallback);
    Serial.println("ok");
}

void loop(void) {
    // put your main code here, to run repeatedly:
    #if !defined(ESP32)
    MDNS.update();
    #endif

    httpServer.handleClient();
    hvac.handleHvac();

    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected() && mqtt_reconnect_retries < MQTT_MAX_RECONNECT_TRIES && !mqtt_conn_abort) {
        Serial.println("MQTT disconnected, reconnecting!");
        mqttReconnect();
        } else if (!mqtt_conn_abort) {
        mqttClient.loop();
        } else if (mqtt_conn_abort) {
            if ( (millis() - mqtt_conn_abort_time) > (MQTT_CONNECT_GIVEUP_DELAY * 60000) ) {
                mqtt_reconnect_retries = 0;
                mqtt_conn_abort = false;
            }
        }
    } else {
        Serial.print("WiFi disconnected, reconnecting");
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.hostname(hostname);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        }
    }

    if (hvac.isConnected() != availability) {
        if (hvac.isConnected()) {
            mqttClient.publish(mqtt_availability_topic, "online", false);
        } else if (!hvac.isConnected()) {
            mqttClient.publish(mqtt_availability_topic, "offline", false);
        }
        availability = hvac.isConnected();
    }
}

/************************
* HVAC data update callback
************************/
void hvacCallback(const char* function) {
    Serial.println(">>>HVAC data update callback<<<");
    if (strcmp(function, "ROOMTEMP") == 0) {    // room temperature updated
        char temp[8];
        itoa(hvac.getRoomTemperature(), temp, 10);
        mqttClient.publish(mqtt_current_temperature_topic, temp, false);
        Serial.printf("room temperature changed: %s\n", temp);
    } else if (strcmp(function, "OUTSIDETEMP") == 0) {  // outside temperature updated
        char temp[8];
        itoa(hvac.getOutsideTemperature(), temp, 10);
        mqttClient.publish(mqtt_outside_temperature_topic, temp, false);
        Serial.printf("outside temperature changed: %s\n", temp);
    } else if (strcmp(function, "STATE") == 0) {    // state updated
        if (hvac.getState() == "off") {
            mqttClient.publish(mqtt_mode_state_topic, hvac.getState(), false);
            mqttClient.publish(mqtt_action_topic, hvac.getState(), false);
            Serial.printf("state changed: %s\n", hvac.getState());
        } else if (hvac.getState() == "on") {
            mqttClient.publish(mqtt_mode_state_topic, hvac.getMode(), false);
            Serial.printf("state changed: %s\n", hvac.getState());
            action_send();
        }
    } else if (strcmp(function, "OP") == 0) {  // operation updated
        mqttClient.publish(mqtt_preset_mode_state_topic, hvac.getOperation(), false);
        Serial.printf("preset changed: %s\n", hvac.getOperation());
    } else if (strcmp(function, "MODE") == 0) { // mode updated
        mqttClient.publish(mqtt_mode_state_topic, hvac.getMode(), false);
        action_send();
        Serial.printf("mode changed: %s\n", hvac.getMode());
    } else if (strcmp(function, "SETPOINT") == 0) { // setpoint updated
        char temp[8];
        itoa(hvac.getSetpoint(), temp, 10);
        mqttClient.publish(mqtt_temperature_state_topic, temp, false);
        Serial.printf("setpoint changed: %s\n", temp);
    } else if (strcmp(function, "FANMODE") == 0) { // fan mode updated
        mqttClient.publish(mqtt_fan_mode_state_topic, hvac.getFanMode(), false);
        Serial.printf("fan mode changed: %s\n", hvac.getFanMode());
    } else if (strcmp(function, "SWING") == 0) {    // swing updated
        mqttClient.publish(mqtt_swing_mode_state_topic, hvac.getSwing(), false);
        Serial.printf("swing changed: %s\n", hvac.getSwing());
    } else if (strcmp(function, "PURE") == 0) { // pure updated
        mqttClient.publish(mqtt_pure_state_topic, hvac.getPure(), false);
        Serial.printf("pure state changed: %s\n", hvac.getPure());
    } else if (strcmp(function, "PSEL") == 0) { // power select updated
        mqttClient.publish(mqtt_psel_state_topic, hvac.getPowerSelect(), false);
        Serial.printf("power select changed: %s\n", hvac.getPowerSelect());
    } else if (strcmp(function, "CDU_STATUS") == 0) {   // cdu status updated
        action_send();
    } else if (strcmp(function, "OFFTIMER") == 0) {    // off timer updated
        mqttClient.publish(mqtt_off_timer_state_topic, hvac.getOffTimer(), false);
        Serial.printf("off timer changed: %s\n", hvac.getOffTimer());
    } else if (strcmp(function, "ONTIMER") == 0) { // on timer updated
        mqttClient.publish(mqtt_on_timer_state_topic, hvac.getOnTimer(), false);
        Serial.printf("on timer changed: %s\n", hvac.getOnTimer());
    }
}

void action_send() {
    if (hvac.isCduRunning() && ((hvac.getMode() == "cool") || (hvac.getMode() == "auto"))) {
        mqttClient.publish(mqtt_action_topic, "cooling", false);
        Serial.printf("action changed: cooling\n");
    } else if (hvac.isCduRunning() && (hvac.getMode() == "heat")) {
        mqttClient.publish(mqtt_action_topic, "heating", false);
        Serial.printf("action changed: heating\n");
    } else if (hvac.isCduRunning() && (hvac.getMode() == "dry")) {
        mqttClient.publish(mqtt_action_topic, "drying", false);
        Serial.printf("action changed: drying\n");
    } else if (!hvac.isCduRunning() && (hvac.getMode() == "fan_only")) {
        mqttClient.publish(mqtt_action_topic, "fan", false);
        Serial.printf("action changed: fan\n");
    } else {
        mqttClient.publish(mqtt_action_topic, "idle", false);
        Serial.printf("action changed: idle\n");
    }
}

/************************
* MQTT callback
************************/
void mqtt_callback(char* topic, byte* payload_in, unsigned int length) {
    uint8_t* payload = (uint8_t*)malloc(length + 1);
    memcpy(payload, payload_in, length);
    payload[length] = '\0';
    char message[length + 1];
    strcpy(message, (char*)payload);
    Serial.printf("MQTT: Message arrived [%s]  from topic: [%s]\n", payload, topic);
    if (strcmp(topic, mqtt_power_command_topic) == 0) {
        if (strcmp(message, "off") == 0) hvac.setState("off");
        else if (strcmp(message, "on") == 0) hvac.setState("on");
    } else if (strcmp(topic, mqtt_preset_mode_command_topic) == 0) {
        if (strcmp(message, "normal") == 0) hvac.setOperation("normal");
        else if (strcmp(message, "high_power") == 0) hvac.setOperation("high_power");
        else if (strcmp(message, "eco") == 0) hvac.setOperation("eco");
        else if (strcmp(message, "silent_1") == 0) hvac.setOperation("silent_1");
        else if (strcmp(message, "silent_2") == 0) hvac.setOperation("silent_2");
    } else if (strcmp(topic, mqtt_mode_command_topic) == 0) {
        if (strcmp(message, "off") == 0) hvac.setState("off");
        else if (strcmp(message, "auto") == 0) hvac.setMode("auto");
        else if (strcmp(message, "cool") == 0) hvac.setMode("cool");
        else if (strcmp(message, "heat") == 0) hvac.setMode("heat");
        else if (strcmp(message, "dry") == 0) hvac.setMode("dry");
        else if (strcmp(message, "fan_only") == 0) hvac.setMode("fan_only");
    } else if (strcmp(topic, mqtt_temperature_command_topic) == 0) {
        hvac.setSetpoint((uint8_t)atoi((char*)payload));
    } else if (strcmp(topic, mqtt_fan_mode_command_topic) == 0) {
        if (strcmp(message, "auto") == 0) hvac.setFanMode("auto");
        else if (strcmp(message, "lvl_1") == 0) hvac.setFanMode("lvl_1");
        else if (strcmp(message, "lvl_2") == 0) hvac.setFanMode("lvl_2");
        else if (strcmp(message, "lvl_3") == 0) hvac.setFanMode("lvl_3");
        else if (strcmp(message, "lvl_4") == 0) hvac.setFanMode("lvl_4");
        else if (strcmp(message, "lvl_5") == 0) hvac.setFanMode("lvl_5");
        else if (strcmp(message, "quiet") == 0) hvac.setFanMode("quiet");
    } else if (strcmp(topic, mqtt_swing_mode_command_topic) == 0) {
        if (strcmp(message, "off") == 0) hvac.setSwing("off");
        else if (strcmp(message, "on") == 0) hvac.setSwing("on");
    } else if (strcmp(topic, mqtt_pure_command_topic) == 0) {
        if (strcmp(message, "off") == 0) hvac.setPure("off");
        else if (strcmp(message, "on") == 0) hvac.setPure("on");
    } else if (strcmp(topic, mqtt_psel_command_topic) == 0) {
        if (strcmp(message, "50%") == 0) hvac.setPowerSelect("50%");
        else if (strcmp(message, "75%") == 0) hvac.setPowerSelect("75%");
        else if (strcmp(message, "100%") == 0) hvac.setPowerSelect("100%");
    } else if (strcmp(topic, mqtt_init_topic) == 0) {
        if (strcmp(message, "ha_start") == 0) {
            hvac.forceQueryAllData();
        } 
    }
    free(payload);
}

/************************
* MQTT reconnect
************************/
void mqttReconnect() {
    // Loop until reconnected
    while (!mqttClient.connected() && mqtt_reconnect_retries < MQTT_MAX_RECONNECT_TRIES) {
        mqtt_reconnect_retries++;
        Serial.printf("Attempting MQTT connection %d / %d ...\n", mqtt_reconnect_retries, MQTT_MAX_RECONNECT_TRIES);
        // Attempt to connect
        if (mqttClient.connect(hostname, mqtt_user, mqtt_password)) {
            Serial.println("MQTT connected!");
            // re-subscribe
            mqttClient.subscribe(mqtt_power_command_topic, qossub);
            Serial.printf("MQTT power command topic: %s\n", mqtt_power_command_topic);
            mqttClient.subscribe(mqtt_preset_mode_command_topic, qossub);
            Serial.printf("MQTT preset command topic: %s\n", mqtt_preset_mode_command_topic);
            mqttClient.subscribe(mqtt_mode_command_topic, qossub);
            Serial.printf("MQTT mode command topic: %s\n", mqtt_mode_command_topic);
            mqttClient.subscribe(mqtt_temperature_command_topic, qossub);
            Serial.printf("MQTT setpoint command topic: %s\n", mqtt_temperature_command_topic);
            mqttClient.subscribe(mqtt_fan_mode_command_topic, qossub);
            Serial.printf("MQTT fan command topic: %s\n", mqtt_fan_mode_command_topic);
            mqttClient.subscribe(mqtt_swing_mode_command_topic, qossub);
            Serial.printf("MQTT swing command topic: %s\n", mqtt_swing_mode_command_topic);
            mqttClient.subscribe(mqtt_pure_command_topic, qossub);
            Serial.printf("MQTT pure command topic: %s\n", mqtt_pure_command_topic);
            mqttClient.subscribe(mqtt_psel_command_topic, qossub);
            Serial.printf("MQTT psel command topic: %s\n", mqtt_psel_command_topic);
            mqttClient.subscribe(mqtt_init_topic, qossub);
            Serial.printf("MQTT init command topic: %s\n", mqtt_init_topic);
            
            mqtt_reconnect_retries = 0;
        } else {
            if (mqtt_reconnect_retries != MQTT_MAX_RECONNECT_TRIES) {
                Serial.print("failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" try again in 5 seconds");
                // Wait 5 seconds before retrying
                delay(5000);
            } else {
                Serial.print("failed, rc=");
                Serial.println(mqttClient.state());
                Serial.printf("MQTT connection aborted, giving up after %d tries, sleep for %d minute(s) then try again.\n", mqtt_reconnect_retries, MQTT_CONNECT_GIVEUP_DELAY);
                mqtt_conn_abort = true;
                mqtt_conn_abort_time = millis();
            }
        }
    }
}

/************************
* HA discovery: hvac
************************/
bool ha_mqtt_discovery_hvac() {
    /*** climate ***/
    const size_t bufferSize = JSON_OBJECT_SIZE(49) + 300;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    // settings
    root["name"] = hostname;
    root["uniq_id"] = unique_id;
    root["init"] = 25;
    root["min_temp"] = 17;
    root["max_temp"] = 30;
    root["temp_unit"] = "C";
    root["temp_step"] = 1;
    root["precision"] = 1;
    root["pl_off"] = "off";
    root["pl_on"] = "on";
    // topics
    root["avty_t"] = mqtt_availability_topic;
    root["pow_cmd_t"] = mqtt_power_command_topic;
    root["pr_mode_stat_t"]= mqtt_preset_mode_state_topic;
    root["pr_mode_cmd_t"] = mqtt_preset_mode_command_topic;
    root["mode_stat_t"]= mqtt_mode_state_topic;
    root["mode_cmd_t"] = mqtt_mode_command_topic;
    root["temp_stat_t"]= mqtt_temperature_state_topic;
    root["temp_cmd_t"] = mqtt_temperature_command_topic;
    root["fan_mode_stat_t"]= mqtt_fan_mode_state_topic;
    root["fan_mode_cmd_t"] = mqtt_fan_mode_command_topic;
    root["swing_mode_stat_t"]= mqtt_swing_mode_state_topic;
    root["swing_mode_cmd_t"] = mqtt_swing_mode_command_topic;
    root["curr_temp_t"] = mqtt_current_temperature_topic;
    root["act_t"]= mqtt_action_topic;
    // templates
    root["temp_cmd_tpl"] = "{{value|int}}";
    // modes
    JsonArray modes = root.createNestedArray("modes");
    modes.add("off");
    modes.add("auto");
    modes.add("cool");
    modes.add("heat");
    modes.add("dry");
    modes.add("fan_only");
    // preset modes
    JsonArray preset_modes = root.createNestedArray("preset_modes");
    preset_modes.add("normal");
    preset_modes.add("high_power");
    preset_modes.add("eco");
    preset_modes.add("silent_1");
    preset_modes.add("silent_2");
    // fan modes
    JsonArray fan_modes = root.createNestedArray("fan_modes");
    fan_modes.add("auto");
    fan_modes.add("lvl_1");
    fan_modes.add("lvl_2");
    fan_modes.add("lvl_3");
    fan_modes.add("lvl_4");
    fan_modes.add("lvl_5");
    fan_modes.add("quiet");
    // swing modes
    JsonArray swing_modes = root.createNestedArray("swing_modes");
    swing_modes.add("off");
    swing_modes.add("on");

    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}

/************************
* HA discovery: pure
************************/
bool ha_mqtt_discovery_pure() {
    /*** switch ***/
    snprintf(obj_id, 64, "%s_pure", hostname);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/switch/%s/config", hostname);
    const size_t bufferSize = JSON_OBJECT_SIZE(9) + 150;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["name"] = "Pure";
    root["obj_id"] = obj_id;
    root["icon"] = "mdi:air-purifier";
    root["uniq_id"] = unique_id;
    root["avty_t"] = mqtt_availability_topic;
    root["cmd_t"] = mqtt_pure_command_topic;
    root["stat_t"] = mqtt_pure_state_topic;
    root["pl_off"] = "off";
    root["pl_on"] = "on";
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}

/************************
* HA discovery: power select
************************/
bool ha_mqtt_discovery_psel() {
    /*** selector ***/
    snprintf(obj_id, 64, "%s_power_select", hostname);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/select/%s/config", hostname);
    const size_t bufferSize = JSON_OBJECT_SIZE(11) + 150;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["name"] = "Power Select";
    root["obj_id"] = obj_id;
    root["icon"] = "mdi:hydro-power";
    root["uniq_id"] = unique_id;
    root["avty_t"] = mqtt_availability_topic;
    root["cmd_t"] = mqtt_psel_command_topic;
    root["stat_t"] = mqtt_psel_state_topic;
    JsonArray options = root.createNestedArray("options");
    options.add("50%");
    options.add("75%");
    options.add("100%");
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}

/************************
* HA discovery: off timer
************************/
bool ha_mqtt_discovery_off_timer() {
    /*** off timer sensor ***/
    char temp[15];
    snprintf(temp, 15, "%s%s", unique_id, "-1");
    snprintf(obj_id, 64, "%s_off_timer", hostname);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/sensor/%s-1/config", hostname);
    const size_t bufferSize = JSON_OBJECT_SIZE(6) + 150;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["name"] = "Off Timer";
    root["icon"] = "mdi:timer-outline";
    root["obj_id"] = obj_id;
    root["uniq_id"] = temp;
    root["avty_t"] = mqtt_availability_topic;
    root["stat_t"] = mqtt_off_timer_state_topic;
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}

/************************
* HA discovery: on timer
************************/
bool ha_mqtt_discovery_on_timer() {
    /*** on timer sensor ***/
    char temp[15];
    snprintf(temp, 15, "%s%s", unique_id, "-2");
    snprintf(obj_id, 64, "%s_on_timer", hostname);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/sensor/%s-2/config", hostname);
    const size_t bufferSize = JSON_OBJECT_SIZE(6) + 150;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["name"] = "On Timer";
    root["icon"] = "mdi:timer-outline";
    root["obj_id"] = obj_id;
    root["uniq_id"] = temp;
    root["avty_t"] = mqtt_availability_topic;
    root["stat_t"] = mqtt_on_timer_state_topic;
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}

/************************
* HA discovery: room temp
************************/
bool ha_mqtt_discovery_room_temp() {
    /*** room temp sensor ***/
    char temp[15];
    snprintf(temp, 15, "%s%s", unique_id, "-3");
    snprintf(obj_id, 64, "%s_room_temperature", hostname);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/sensor/%s-3/config", hostname);
    const size_t bufferSize = JSON_OBJECT_SIZE(7) + 150;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["name"] = "Room Temperature";
    root["icon"] = "mdi:thermometer";
    root["obj_id"] = obj_id;
    root["uniq_id"] = temp;
    root["avty_t"] = mqtt_availability_topic;
    root["stat_t"] = mqtt_current_temperature_topic;
    root["unit_of_meas"] = "C";
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}

/************************
* HA discovery: outside temp
************************/
bool ha_mqtt_discovery_outside_temp() {
    /*** outside temp sensor ***/
    char temp[15];
    snprintf(temp, 15, "%s%s", unique_id, "-4");
    snprintf(obj_id, 64, "%s_outside_temperature", hostname);
    snprintf(mqtt_ha_discovery_topic, 64, "homeassistant/sensor/%s-4/config", hostname);
    const size_t bufferSize = JSON_OBJECT_SIZE(7) + 150;
    DynamicJsonDocument jsonBuffer(bufferSize);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["name"] = "Outside Temperature";
    root["icon"] = "mdi:thermometer";
    root["obj_id"] = obj_id;
    root["uniq_id"] = temp;
    root["avty_t"] = mqtt_availability_topic;
    root["stat_t"] = mqtt_outside_temperature_topic;
    root["unit_of_meas"] = "C";
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_ha_discovery_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
        return true;
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
        return false;
    }
}