# ToshibaCarrierHvac
สวัสดีครับ ไลบรารี่นี้ใช้การสื่อสารแบบอนุกรม(serial/uart) สำหรับเชื่อมต่อ Arduino หรือ NodeMCU เข้ากับเครื่องปรับอากาศ Toshiba/Carrier ผ่านช่องเชื่อมต่อกล่อง wifi บนบอร์ดโดยตรง
คุณสามารถใช้ไลบรารี่นี้เพื่อต่อยอดและพัฒนาใช้งานร่วมกับระบบ smart home ที่มีอยู่ได้ ควบคุมแอร์ผ่านอินเทอร์เน็ตหรือวงแลน ระบบอัตโนมัติ และอื่นๆอีกมากมาย

## ใช้ได้กับไมโครคอนโทรลเลอร์:
 - ESP8266
 - ESP32
 - ATmega328P (Arduino Uno R3, Arduino Nano)

## รองรับเครื่องปรับอากาศ:
  - Toshiba Seiya
  - Toshiba Shorai
  - Carrier X Inverter (42TVAA)

อาจรองรับ (ยังไม่ได้ทดสอบ):
 - Carrier Color Smart (42TVCA)
 - Carrier X Inverter Plus (42TVAB)

## รองรับการสั่งงาน:
 - เปิด/ปิด
 - ปรับโหมด (auto, cool, dry, fan_only)
 - ปรับระดับความแรงลม
 - เปิด/ปิดการส่ายใบปรับทิศทางลม (แนวตั้ง)
 - เปิด/ปิดระบบฟอกอากาศพลาสมาไอออน (Pure)
 - เลือกระดับการใช้พลังงาน (PSEL)
 - เลือกการทำงาน (High Power, ECO, Silent1, Silent2)
 - ดูอุณหภูมิภายในห้อง
 - ดูอุณหภูมิภายนอก
 - ดูสถานะการตั้งเวลาเปิด/ปิด
 - ดูสถานะ CDU

## Header & Connector pinout
ดูรูปภาพ [ที่นี่](/images)

## วงจรตัวอย่าง
ข้อควรระวัง: กรณีใช้กับ ESP8266 หรือ ESP32 ควรใช้งานร่วมกับ TTL level shifter (5V to 3.3V TTL)

![วงจรตัวอย่าง](images/circuit.png?raw=true "วงจรตัวอย่าง")

## วิธีใช้งาน

#### 1) เรียกใช้งานไลบรารี่ในโปรแกรม
```C++
#include <ToshibaCarrierHvac.h>
```

#### 2) เลือกพอร์ตที่ต้องการใช้งาน

- Hardware serial
```C++
ToshibaCarrierHvac hvac(&Serial);
```
- Software serial
```C++
ToshibaCarrierHvac hvac(D5, D6); // RX, TX
```

#### 3) เพิ่ม handleHvac เข้าไปใน loop
```C+
void loop() {
    hvac.handleHvac();
}
```

#### โครงสร้างข้อมูลที่ใช้งานได้

- โครงสร้างข้อมูลการตั้งค่า
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
};
```

- โครงสร้างข้อมูลสถานะ
```C++
 struct hvacStatus {
    int8_t roomTemperature;
    int8_t outsideTemperature;
    const char* offTimer;
    const char* onTimer;
    bool running;
};
```

## ฟังก์ชั่น
- เรียกใช้โครงสร้างข้อมูลสถานะปัจจุบันทั้งหมด
```C++
hvacStatus newStatus = hvac.getStatus;
```

- เรียกใช้โครงสร้างข้อมูลการตั้งค่าปัจจุบันทั้งหมด
```C++
hvacSettings newSettings = hvac.getSettings;
```

- การเลือกใช้เพียงข้อมูลเดียว
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

- การเซ็ตการตั้งค่าจากพรีเซ็ตที่ตั้งไว้
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
 
- การเลือกเซ็ตเพียงค่าเดียว 
```C++
hvac.setState("on");
hvac.setSetpoint(25);
hvac.setMode("cool");
hvac.setfanMode("lvl_3");
hvac.setSwing("off");
hvac.setPure("on");
hvac.getPowerSelect("100%");
hvac.getOperation("normal");
```

- สถานะแบบบูลีน (true หรือ false)
```C++
hvac.isConnected();
hvac.isCduRunning();
```

## การใช้ฟังก์ชั่นเรียกกลับ
ตั่งค่าฟังก์ชั่นเรียกกลับใน setup สามารถดูตัวอย่างเพิ่มเติมใน [UseCallback.ino](examples/UseCallback/UseCallback.ino)
```C++
void acCallback(hvacStatus newStatus) {
    if (newStatus.roomTemperature > 30) hvac.setState("on");
}

void setup() {
    hvac.setStatusUpdatedCallback(acCallback);
}
```

### ฟังก์ชั่นเรียกกลับสถานะมีการอัพเดต
ฟังก์ชั่นเรียกกลับนี้จะทำงานเมื่อสถานะมีการอัพเดต และส่งกลับข้อมูลโครงสร้างสถานะปัจจุบัน (hvacStatus)
```C++
hvac.setStatusUpdatedCallback(YourCallbackFunction);
```

### ฟังก์ชั่นเรียกกลับการตั้งค่ามีการอัพเดต
ฟังก์ชั่นเรียกกลับนี้จะทำงานเมื่อการตั้งค่ามีการอัพเดต และส่งกลับข้อมูลโครงสร้างการตั้งค่าปัจจุบัน (hvacSettings)
```C++
hvac.setSettingsUpdatedCallback(YourCallbackFunction);
```

### ฟังก์ชั่นเรียกกลับข้อมูลมีการอัพเดต
ฟังก์ชั่นเรียกกลับนี้จะทำงานเมื่อสถานะ หรือการตั้งค่ามีการอัพเดต แต่ไม่ส่งกลับข้อมูลใดๆ
```C++
hvac.setUpdateCallback(YourCallbackFunction);
```

### ฟังก์ชั่นเรียกกลับข้อมูลที่มีการอัพเดต
ฟังก์ชั่นเรียกกลับนี้จะทำงานเมื่อสถานะ หรือการตั้งค่ามีการอัพเดต และส่งกลับเป็นชื่อของข้อมูลที่อัพเดต
```C++
hvac.setWhichFunctionUpdatedCallback(YourCallbackFunction);
```

## จะทำเร็วๆนี้:
- [ ] ส่งแพ็คเก็ตที่สร้างขึ้นเอง
- [ ] ฟังก์ชั่นเรียกกลับสำหรับการ debug