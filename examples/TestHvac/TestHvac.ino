/*
*   This sketch will turn on your HVAC with preset shown below.
*/

#include <ToshibaCarrierHvac.h>

ToshibaCarrierHvac hvac(&Serial);

hvacSettings myPreset {
    "on",    // State ["off", "on"]
    25,      // Setpoint [16-30c]
    "cool",  // Mode ["auto", "cool", "heat", "dry", "fan_only"]
    "on",    // Swing ["off", "on"]
    "lvl_3", // Fan mode ["quiet", "lvl_1", "lvl_2", "lvl_3", "lvl_4", "lvlL_5", "auto"]
    "off",   // Pure ["off", "on"]
    "100%",  // Power select ["50%", "75%", "100%"]
    "normal" // Operation ["normal", "high_power", "silent_1", "eco", "silent_2"]
};

void setup() {
    // put your setup code here, to run once:
    
}

void loop() {
    // put your main code here, to run repeatedly:
    hvac.handleHvac();
    if ((hvac.isConnected() == true) && (hvac.getState() == "off")) {
        hvac.applyPreset(myPreset);
    }
}