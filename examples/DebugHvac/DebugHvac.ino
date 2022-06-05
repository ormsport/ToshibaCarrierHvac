/*
*   This sketch will print all feedback and raw data received from HVAC
*   ***Not recommend to debug with ESP-01***
*   To debug your HVAC please uncomment "#define HVAC_DEBUG" in "ToshibaCarrierHvac.h".
*   for ESP8266 and Arduino Uno please use software serial
*   for ESP32 changes tx and rx pin to &Serial2
*/

#include <ToshibaCarrierHvac.h>

ToshibaCarrierHvac hvac(D5, D6); // Rx, Tx

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    delay(5000);  // delay 5s before handshake start
}

void loop() {
    // put your main code here, to run repeatedly:
    hvac.handleHvac();
}