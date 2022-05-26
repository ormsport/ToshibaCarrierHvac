#ifndef ToshibaCarrierHvac_H
#define ToshibaCarrierHvac_H

#if defined(ARDUINO) && ARDUINO >= 100
    #include "Arduino.h"
#else
    #include "WProgram.h"
#endif

// hardware serial or software serial
// #define HVAC_USE_HW_SERIAL
#if !defined(HVAC_USE_HW_SERIAL) && (defined(__AVR__) || defined(ESP8266))
    #define HVAC_USE_SW_SERIAL
#endif

// #define HVAC_USE_SW_SERIAL
// software serial for AVR
#if defined(HVAC_USE_SW_SERIAL) && defined(__AVR__)
    #include <CustomSoftwareSerial.h>
    #define HVAC_DEBUG
#endif

// software serial for ESP8266
#if defined(HVAC_USE_SW_SERIAL) && (defined(ESP8266))
    #include <SoftwareSerial.h>
    #define HVAC_DEBUG
#endif

// show error about debug port
#if defined(HVAC_DEBUG) && !defined(ESP32) && !defined(HVAC_USE_SW_SERIAL)
    #error "Debug enabled but hardware serial also used"
#else
    #define DEBUG_PORT Serial
#endif


// callback
#if defined(ESP8266) || defined(ESP32)
    #include <functional>
    #define STATUS_UPDATED_CALLBACK_SIGNATURE std::function<void(hvacStatus newStatus)> statusUpdatedCallback
    #define SETTINGS_UPDATED_CALLBACK_SIGNATURE std::function<void(hvacSettings newSettings)> settingsUpdatedCallback
    #define UPDATE_CALLBACK_SIGNATURE std::function<void(void)> updateCallback
    #define WHICH_FUNCTION_UPDATED_CALLBACK_SIGNATURE std::function<void(const char* function)> whichFunctionUpdatedCallback
#else
    #define STATUS_UPDATED_CALLBACK_SIGNATURE void (*statusUpdatedCallback)(hvacStatus newStatus)
    #define SETTINGS_UPDATED_CALLBACK_SIGNATURE void (*settingsUpdatedCallback)(hvacSettings newSettings)
    #define UPDATE_CALLBACK_SIGNATURE void (*updateCallback)(void)
    #define WHICH_FUNCTION_UPDATED_CALLBACK_SIGNATURE void (*whichFunctionUpdatedCallback)(const char* function)
#endif

// hvac settings structure
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

// hvac status structure
struct hvacStatus {
    int8_t roomTemperature;
    int8_t outsideTemperature;
    const char* offTimer;
    const char* onTimer;
    bool running;
};

class ToshibaCarrierHvac {
    private:
        Stream* _serial;
        bool _swSerial;
        bool _firstRun = true;
        bool _handshake = false; // waiting
        bool _ready = false;     // waiting
        bool _connected = false;
        bool _init = false;
        bool _sendWake = false;
        uint32_t _lastReceive = 0;
        uint32_t _lastSendWake = 0;
        uint32_t _idleTimeout = 0;
        uint32_t _connectionTimeout = 0;
        uint32_t _queryallDelay = 0;
        uint32_t _lastSyncSettings = 0;
        uint32_t _lastSettingsCallback = 0;
        uint32_t _lastStatusCallback = 0;
        uint32_t _lastUpdateCallback = 0;
        uint8_t _settingsCallbackBucket = 0;
        uint8_t _statusCallbackBucket = 0;
        uint8_t _updateCallbackBucket = 0;

        hvacSettings currentSettings {};
        hvacSettings wantedSettings {"UNKNOWN", 0, "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN"}; // set data to prevent strcasecmp crash
        hvacSettings userSettings {"UNKNOWN", 0, "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN"};   // set data to prevent strcasecmp crash
        hvacStatus currentStatus {};

        // callback
        STATUS_UPDATED_CALLBACK_SIGNATURE {nullptr};
        SETTINGS_UPDATED_CALLBACK_SIGNATURE {nullptr};
        UPDATE_CALLBACK_SIGNATURE {nullptr};
        WHICH_FUNCTION_UPDATED_CALLBACK_SIGNATURE {nullptr};

        // handshake SYN packet
        const byte HANDSHAKE_SYN_PACKET_1[8]  = {2, 255, 255, 0, 0, 0, 0, 2};
        const byte HANDSHAKE_SYN_PACKET_2[9]  = {2, 255, 255, 1, 0, 0, 1, 2, 254};
        const byte HANDSHAKE_SYN_PACKET_3[10] = {2, 0, 0, 0, 0, 0, 2, 2, 2, 250};
        const byte HANDSHAKE_SYN_PACKET_4[10] = {2, 0, 1, 129, 1, 0, 2, 0, 0, 123};
        const byte HANDSHAKE_SYN_PACKET_5[10] = {2, 0, 1, 2, 0, 0, 2, 0, 0, 254};
        const byte HANDSHAKE_SYN_PACKET_6[8]  = {2, 0, 2, 0, 0, 0, 0, 254};

        // handshake ACK packet
        const byte HANDSHAKE_ACK_PACKET_1[10] = {2, 0, 2, 1, 0, 0, 2, 0, 0, 251};
        const byte HANDSHAKE_ACK_PACKET_2[10] = {2, 0, 2, 2, 0, 0, 2, 0, 0, 250};

        // packet config and type map
        const byte HANDSHAKE_HEADER[3] = {2, 0, 0};
        const byte CONFIRM_HEADER[3] = {2, 0, 2};
        const byte PACKET_HEADER[3] = {2, 0, 3};

        const byte PACKET_TYPE[5]      = {128, 130, 16, 17, 144};
        const char* PACKET_TYPE_MAP[6] = {"SYN/ACK", "ACK", "COMMAND", "FEEDBACK", "REPLY", "UNKNOWN"};

        const byte DATA_TYPE[2] = {1, 2};
        const char* DATA_TYPE_MAP[3] = {"QUERY", "SETTING", "UNKNOWN"};

        const byte FUNCTION_BYTE[13] = {128, 135, 136, 144, 148, 160, 163, 176, 179, 187, 190, 199, 247};
        const char* FUNCTION_BYTE_MAP[14] = {"STATE", "PSEL", "STATUS", "ONTIMER","OFFTIMER", "FANMODE", "SWING", "MODE", "SETPOINT", "ROOMTEMP", "OUTSIDETEMP", "PURE", "OP", "UNKNOWN"};

        const byte MODE_BYTE[5] = {65, 66, 67, 68, 69};
        const char* MODE_BYTE_MAP[6] = {"auto", "cool", "heat", "dry", "fan_only", "UNKNOWN"};

        const byte FANMODE_BYTE[7] = {49, 50, 51, 52, 53, 54, 65};
        const char* FANMODE_BYTE_MAP[8] = {"quiet", "lvl_1", "lvl_2", "lvl_3", "lvl_4", "lvl_5", "auto", "UNKNOWN"};

        const byte PSEL_BYTE[3] = {50, 75, 100};
        const char* PSEL_BYTE_MAP[4] = {"50%", "75%", "100%", "UNKNOWN"};

        const byte OP_BYTE[5] = {0, 1, 2, 3, 10};
        const char* OP_BYTE_MAP[6] = {"normal", "high_power", "silent_1", "eco", "silent_2", "UNKNOWN"};

        const byte STATUS_BYTE[1] = {66};
        const char* STATUS_BYTE_MAP[2] = {"READY", "UNKNOWN"};

        const byte SWING_BYTE[2] = {49, 65};
        const byte STATE_BYTE[2] = {49, 48};
        const byte PURE_BYTE[2] = {16, 24};
        const byte TIMER_BYTE[2] = {66, 65};
        const char* OFF_ON_MAP[3] = {"off", "on", "UNKNOWN"};

        void sendCommand(byte data[], size_t dataLen);
        void sendCommand(const byte data[], size_t dataLen);
        byte getByteByName(const byte byteMap[], const char* valMap[], size_t valLen, const char* name);
        const char* getNameByByte(const char* valMap[], const byte byteMap[], size_t valLen, byte byteVal);
        byte checksum(uint16_t key, size_t fnByte, byte fnVal);
        int8_t temperatureCorrection(byte val);
        bool createPacket(const byte header[], size_t headerLen, const byte packetType, const byte dataType, const byte fnCode, const byte fnVal);
        void sendHandshake(void);
        void queryall(void);
        void queryTemperature(void);
        bool syncUserSettings(void);
        bool processData(byte* data, size_t dataLen);
        bool readPacket(byte data[], size_t dataLen);
        bool findHeader(byte data[], size_t dataLen, const byte header[], size_t headerLen);
        uint8_t findHeaderStart(byte data[], size_t dataLen, const byte header[], size_t headerLen);
        bool chunkSplit(byte data[], size_t dataLen);
        bool packetMonitor(void);
        void sendDebug(char* message, uint8_t len);

    public:
        ToshibaCarrierHvac(uint8_t rxPin, uint8_t txPin);
        ToshibaCarrierHvac(HardwareSerial* port);
        ~ToshibaCarrierHvac();

        void setStatusUpdatedCallback(STATUS_UPDATED_CALLBACK_SIGNATURE);
        void setSettingsUpdatedCallback(SETTINGS_UPDATED_CALLBACK_SIGNATURE);
        void setUpdateCallback(UPDATE_CALLBACK_SIGNATURE);
        void setWhichFunctionUpdatedCallback(WHICH_FUNCTION_UPDATED_CALLBACK_SIGNATURE);

        void handleHvac (void);
        void applyPreset(hvacSettings newSettings);
        void setState(const char* newState);
        void setSetpoint(uint8_t newSetpoint);
        void setMode(const char* newMode);
        void setSwing(const char* newSwing);
        void setFanMode(const char* newFanMode);
        void setPure(const char* newPure);
        void setPowerSelect(const char* newPowerSelect);
        void setOperation(const char* newOperation);
        hvacStatus getStatus(void);
        hvacSettings getSettings(void);
        int8_t getRoomTemperature(void);
        int8_t getOutsideTemperature(void);
        const char* getState(void);
        uint8_t getSetpoint(void);
        const char* getMode(void);
        const char* getSwing(void);
        const char* getFanMode(void);
        const char* getPure(void);
        const char* getOffTimer(void);
        const char* getOnTimer(void);
        const char* getPowerSelect(void);
        const char* getOperation(void);
        bool isCduRunning(void);
        bool isConnected(void);
        void forceQueryAllData(void);
};
#endif // ToshibaCarrierHvac_H