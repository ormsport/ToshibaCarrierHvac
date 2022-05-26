/*
*   This sketch will send value received from HVAC over MQTT in JSON format, there is 3 mqtt topics used to send settings and status and used set a settings.
*   You need to config wifi and mqtt in "config.h"
*   Set settings with JSON format too. example '{"state":"on"}'
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
    *************************/
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
    Serial.printf("HTTPUpdateServer ready!");

    /************************
    * Setup: MQTT(PubSupClient)
    ************************/
    #if defined(use_chip_id)
    uint32_t chip_id = 0;
    #if defined(ESP32)
    for(int i=0; i<17; i=i+8) {
        chip_id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    #else
    chip_id = ESP.getChipId();
    #endif
    snprintf(mqtt_clientid, 64, "%s-%08X", hostname, chip_id);
    #endif
    Serial.printf("MQTT Client ID: %s\n", mqtt_clientid);

    if (mqtt_host != "" && atoi(mqtt_port) > 0 && atoi(mqtt_port) < 65535) {
        Serial.printf("MQTT active: %s:%d\n", mqtt_host, String(mqtt_port).toInt());
        mqttClient.setBufferSize(256);
        mqttClient.setServer(mqtt_host, atoi(mqtt_port));
        mqttClient.setCallback(mqtt_callback);
        while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT Server...");
        if (mqttClient.connect(mqtt_clientid, mqtt_user, mqtt_password )) {
            Serial.println("Connected");
            mqttClient.subscribe(mqtt_settings_in_topic, qossub);
            mqttClient.subscribe(mqtt_settings_out_topic, qossub);
            mqttClient.subscribe(mqtt_status_out_topic, qossub);
            Serial.print("MQTT status in topic: ");
            Serial.println(mqtt_settings_in_topic);
            Serial.print("MQTT settings out topic: ");
            Serial.println(mqtt_settings_out_topic);
            Serial.print("MQTT status out topic: ");
            Serial.println(mqtt_status_out_topic);
            
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
    * Setup: ToshibaCarrierHvac
    ************************/
    Serial.print("Initializing ToshibaCarrierHvac...");
    hvac.setStatusUpdatedCallback(hvacStatusCallback);
    hvac.setSettingsUpdatedCallback(hvacSettingsCallback);
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
}

/************************
* HVAC settings update callback
************************/
void hvacSettingsCallback(hvacSettings newSettings) {
    Serial.println(">>>HVAC settings update callback<<<");
    StaticJsonDocument<256> root;
    root["state"] = newSettings.state;
    root["setpoint"] = newSettings.setpoint;
    root["mode"] = newSettings.mode;
    root["fan_mode"] = newSettings.fanMode;
    root["swing"] = newSettings.swing;
    root["pure"] = newSettings.pure;
    root["psel"] = newSettings.powerSelect;
    root["op"] = newSettings.operation;
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_settings_out_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
    }
}

/************************
* HVAC status update callback
************************/
void hvacStatusCallback(hvacStatus newStatus) {
    Serial.println(">>>HVAC status update callback<<<");
    StaticJsonDocument<256> root;
    root["room_temp"] = newStatus.roomTemperature;
    root["outside_temp"] = newStatus.outsideTemperature;
    root["on_timer"] = newStatus.onTimer;
    root["off_timer"] = newStatus.offTimer;
    root["cdu_run"] = newStatus.running;
    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    if ( mqttClient.publish(mqtt_status_out_topic, buffer, false)) {
        Serial.printf("MQTT: Message has been sent.\n");
        Serial.printf("Data: %s\n", buffer);
    } else {
        Serial.printf("MQTT: Message sending error.\n");
        Serial.printf("Error Data: %s\n", buffer);
    }
}

/************************
* MQTT data process
************************/
bool processJson(char* message) {
    const size_t bufferSize = JSON_OBJECT_SIZE(8) + 50;
    DynamicJsonDocument jsonBuffer(bufferSize);
    DeserializationError error = deserializeJson(jsonBuffer, message);
    if (error) {
        Serial.print("parseObject() failed: ");
        Serial.println(error.c_str());
        return false;
    }
    JsonObject root = jsonBuffer.as<JsonObject>();
    if (root.containsKey("state")) {
        hvac.setState(root["state"]);
    }
    if (root.containsKey("setpoint")) {
        hvac.setSetpoint(root["setpoint"]);
    }
    if (root.containsKey("mode")) {
        hvac.setMode(root["mode"]);
    }
    if (root.containsKey("fan_mode")) {
        hvac.setFanMode(root["fan_mode"]);
    }
    if (root.containsKey("swing")) {
        hvac.setSwing(root["swing"]);
    }
    if (root.containsKey("pure")) {
        hvac.setPure(root["pure"]);
    }
    if (root.containsKey("psel")) {
        hvac.setPowerSelect(root["psel"]);
    }
    if (root.containsKey("preset")) {
        hvac.setOperation(root["op"]);
    }
    jsonBuffer.clear();
    return true;
}

/************************
* MQTT callback
************************/
void checkpayload(uint8_t * payload, bool mqtt = false, uint8_t num = 0) {
    if (payload[0] == '#') {
        mqttClient.publish(mqtt_settings_in_topic, String(String("OK ") + String((char *)payload)).c_str(), false);
    }
}

void mqtt_callback(char* topic, byte* payload_in, unsigned int length) {
    uint8_t * payload = (uint8_t *)malloc(length + 1);
    memcpy(payload, payload_in, length);
    payload[length] = NULL;
    Serial.printf("MQTT: Message arrived [%s]  from topic: [%s]\n", payload, topic);
    if (strcmp(topic, mqtt_settings_in_topic) == 0) {
        if (!processJson((char*)payload)) {
        return;
        }
    } else if (strcmp(topic, (char *)mqtt_settings_in_topic) == 0) {
        checkpayload(payload, true);
        free(payload);
    }
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
        if (mqttClient.connect(mqtt_clientid, mqtt_user, mqtt_password)) {
            Serial.println("MQTT connected!");
            // re-subscribe
            mqttClient.subscribe(mqtt_settings_in_topic, qossub);
            mqttClient.subscribe(mqtt_settings_out_topic, qossub);
            mqttClient.subscribe(mqtt_status_out_topic, qossub);
            Serial.print("MQTT status in topic: ");
            Serial.println(mqtt_settings_in_topic);
            Serial.print("MQTT settings out topic: ");
            Serial.println(mqtt_settings_out_topic);
            Serial.print("MQTT status out topic: ");
            Serial.println(mqtt_status_out_topic);
            
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