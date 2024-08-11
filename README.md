# ToshibaCarrierHvac
This library can make Arduino/NodeMCU communicate with Toshiba/Carrier HVAC System via serial communication through wifi adapter port. You can use this library to implement the smart home system, control your HVAC remoteless via internet or LAN, automation, etc.

## Tested working on:
 - ESP8266
 - ESP32
 - ATmega328P (Arduino Uno R3, Arduino Nano)
 
## Supported HVAC models:
  - Toshiba Seiya
  - Toshiba Shorai
  - Carrier X Inverter (42TVAA)
  - Carrier X Inverter Plus 2024 (42TVAB)

#### And might support (not tested yet):
  - Carrier Color Smart (42TVCA) 

## Supported features:
 - Power ("off", "on")
 - Setpoint (17-30c)
 - Mode ("auto", "cool", "heat", "dry", "fan_only")
 - Fan mode ("quiet", "lvl_1", "lvl_2", "lvl_3", "lvl_4", "lvl_5", "auto")
 - Swing (vertical) ("fix", "v_swing", "h_swing", "vh_swing", "fix_pos_1", "fix_pos_2", "fix_pos_3", "fix_pos_4", "fix_pos_5", )
 - Pure ("off", "on")
 - Power select ("50%", "75%", "100%")
 - Operation ("normal", "high_power", "silent_1", "eco", "eight_deg", "silent_2", "fireplace_1", "fireplace_2")
 - Front panel WiFi LED ("off", "on")
 - Room temperature
 - Outside temperature
 - On/off timer status ("off", "on")
 - CDU status (run, stop)

## Header & Connector pinout
See images [here](/images)
 
## Sample Circuit (ESP01-adapter)
Note: If using ESP8266 or ESP32 please always use with TTL level shifter (5V to 3.3V TTL)
 
![ESP01-adapter Circuit](images/esp01-adapter_wiring.jpg?raw=true "ESP01-adapter wiring diagram")
 
## How to use
 
#### 1) Include library to your sketch
```C++
#include <ToshibaCarrierHvac.h>
```
  
#### 2) Set port you want to use

- Hardware serial
```C++
ToshibaCarrierHvac hvac(&Serial);
```
- Software serial
```C++
ToshibaCarrierHvac hvac(D5, D6); // RX, TX
```

#### 3) Add handleHvac to loop
```C+
void loop() {
    hvac.handleHvac();
}
```
 
## Global data structures

- Settings structure
```C++
struct hvacSettings {
    const char* state;
    uint8_t setpoint;
    const char* mode;
    const char* swing;
    const char* fanMode;
    const char* pure;
    const char* powerSelect;
    const char* operation;
    const char* wifiLed;
};
```
 
- Status structure
```C++
 struct hvacStatus {
    int8_t roomTemperature;
    int8_t outsideTemperature;
    const char* offTimer;
    const char* onTimer;
    bool running;
};
```

## Functions

- Get all current status from structure
```C++
hvacStatus newStatus = hvac.getStatus;
```
 
- Get all current settings from structure
```C++
hvacSettings newSettings = hvac.getSettings;
```
 
- Get only one function
```C++
hvac.getState();
hvac.getSetpoint();
hvac.getMode();
hvac.getFanMode();
hvac.getSwing();
hvac.getPure();
hvac.getOffTimer();
hvac.getOnTimer();
hvac.getPowerSelect();
hvac.getOperation();
```
 
 - Apply a preset
```C++
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

hvac.applyPreset(myPreset);
 ```
 
- Set only one function
```C++
hvac.setState("on");
hvac.setSetpoint(25);
hvac.setMode("cool");
hvac.setfanMode("lvl_3");
hvac.setSwing("off");
hvac.setPure("on");
hvac.setPowerSelect("100%");
hvac.setOperation("normal");
```
 
- Boolean status (true or false)
```C++
hvac.isConnected();
hvac.isCduRunning();
```

## Callback Functions
Set your callback function in setup. See more example in [UseCallback.ino](examples/UseCallback/UseCallback.ino) 
```C++
void hvacCallback(hvacStatus newStatus) {
    if (newStatus.roomTemperature > 30) hvac.setState("on");
}

void setup() {
    hvac.setStatusUpdatedCallback(hvacCallback);
}
```

### Status Updated Callback
This will callback when any status updated and return structure of current status (acStatus).
```C++
hvac.setStatusUpdatedCallback(YourCallbackFunction);
```

### Settings Updated Callback
This will callback when any setting updated and return structure of current settings (acSettings).
```C++
hvac.setSettingsUpdatedCallback(YourCallbackFunction);
```

### Update Callback
This will callback when any status or any settings updated but doesn't return anything.
```C++
hvac.setUpdateCallback(YourCallbackFunction);
```

### Which Function Updated Callback
This will callback when any status or any setting updated and return name of updated function.
```C++
hvac.setWhichFunctionUpdatedCallback(YourCallbackFunction);
```

## Send custom packet
The custom packet size must be 8 to 17 bytes. This function just send your packet without checking anything so please carefully use.
```C++
byte myPacket[] = {2, 0, 3, 144, 0, 0, 9, 1, 48, 1, 0, 0, 0, 2, 163, 65, 76};
hvac.sendCustomPacket(myPacket, sizeof(myPacket));
```
## Making a prototype board
Use KiCad to design a prototype board. The cost of components and PCB is around $2.5/pices.
- Schematics.
![prototype schematics](images/prototype_sch.jpg?raw=true "prototype schematics")

- PCB
![prototype pcb](images/prototype_pcb.png?raw=true "prototype pcb")

- Wiring
![prototype wiring](images/prototype_wiring.jpg?raw=true "prototype wiring")

- Final
![prototype](images/prototype.jpg?raw=true "prototype")

## TO DO:
- [x] Send custom packet
- [x] Prototype board
- [ ] Callback function for debug
