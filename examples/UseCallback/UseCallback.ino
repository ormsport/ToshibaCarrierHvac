/*
*   This sketch show how to use callback to a custom function.
*   *: You can use only one callback has this mark.
*/

#include <ToshibaCarrierHvac.h>

ToshibaCarrierHvac hvac(D5, D6);    // Rx, Tx

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);

    // set a callback function
    hvac.setStatusUpdatedCallback(whenStatusUpdated);             // *callback when any value in status updated and return data struct of status
    hvac.setSettingsUpdatedCallback(whenSettingsUpdated);         // *same as above but settings
    hvac.setUpdateCallback(whenUpdateCallback);                   // *callback when any value in both struct updated
    hvac.setWhichFunctionUpdatedCallback(whichFunctionUpdated);   // callback when any value in both struct updated and return name of the function that updated
}

void loop() {
    // put your main code here, to run repeatedly:
    hvac.handleHvac();
}

void whenStatusUpdated(hvacStatus newStatus) {    // print data in status
    Serial.println("#-------HVAC Status-------#");
    Serial.print("room temperature: ");
    Serial.println(newStatus.roomTemperature);
    Serial.print("outside temperature: ");
    Serial.println(newStatus.outsideTemperature);
    Serial.print("CDU running: ");
    Serial.println(newStatus.running);
    Serial.print("off timer: ");
    Serial.println(newStatus.offTimer);
    Serial.print("on timer: ");
    Serial.println(newStatus.onTimer);
    Serial.println("#------END HVAC Status------#");
}

void whenSettingsUpdated(hvacSettings newSetting) {   // print data in settings
    Serial.println("#-------HVAC Settings-------#");
    Serial.print("state: ");
    Serial.println(newSetting.state);
    Serial.print("setpoint: ");
    Serial.println(newSetting.setpoint);
    Serial.print("mode: ");
    Serial.println(newSetting.mode);
    Serial.print("fan mode: ");
    Serial.println(newSetting.fanMode);
    Serial.print("swing: ");
    Serial.println(newSetting.swing);
    Serial.print("pure: ");
    Serial.println(newSetting.pure);
    Serial.print("psel: ");
    Serial.println(newSetting.powerSelect);
    Serial.print("operation: ");
    Serial.println(newSetting.operation);
    Serial.println("#------END HVAC Settings------#");
}

void whenUpdateCallback() {
    hvacSettings newSettings = hvac.getSettings();
    hvacStatus newStatus = hvac.getStatus();
    // and do something...
}

void whichFunctionUpdated(const char* function) {
    if (function == "STATE") {
        Serial.print("State updated: ");
        Serial.println(hvac.getState());
    } else if (function == "SETPOINT") {
        Serial.print("Setpoint updated: ");
        Serial.println(hvac.getSetpoint());
    } else if (function == "MODE") {
        Serial.print("Mode updated: ");
        Serial.println(hvac.getMode());
    } else if (function == "FANMODE") {
        Serial.print("Fan mode updated: ");
        Serial.println(hvac.getFanMode());
    } else if (function == "SWING") {
        Serial.print("Swing updated: ");
        Serial.println(hvac.getSwing());
    } else if (function == "PURE") {
        Serial.print("Pure updated: ");
        Serial.println(hvac.getPure());
    } else if (function == "PSEL") {
        Serial.print("Power select updated: ");
        Serial.println(hvac.getPowerSelect());
    } else if (function == "COMPMODE") {
        Serial.print("Operation updated: ");
        Serial.println(hvac.getOperation());
    } else if (function == "OFFTIMER") {
        Serial.print("Off timer updated: ");
        Serial.println(hvac.getOffTimer());
    } else if (function == "ONTIMER") {
        Serial.print("On timer updated: ");
        Serial.println(hvac.getOnTimer());
    } else if (function == "ROOMTEMP") {
        Serial.print("Room temperature updated: ");
        Serial.println(hvac.getRoomTemperature());
    } else if (function == "OUTSIDETEMP") {
        Serial.print("Outside temperature updated: ");
        Serial.println(hvac.getOutsideTemperature());
    }
}