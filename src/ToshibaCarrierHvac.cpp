#include "ToshibaCarrierHvac.h"

#define BUADRATE 9600                       // buadrate
#define MAX_RX_BYTE_READ 251                // max bytes read
#define RX_READ_TIMEOUT 250                 // read timeout(ms)
#define IDLE_TIMEOUT 1                      // max timeout(minutes) after received data, and try to sent some query to check the connection (should not exceed than 3 minutes)
#define CONNECTION_TIMEOUT 2                // max timeout(minutes) after sent some query or command but no reply in time, that's mean connection break or disconnected, try to send new handshake
#define START_DELAY 10                      // after connected delay x second before query all data
#define SETTINGS_SEND_DELAY 600             // delay x ms before send next setting (do not decrease too much, your hvac may not parse a setting correctly)
#define SINGLE_QUEUE_TIMEOUT 800            // when timeout(ms) reached and has only one callback in queue just do a callback
#define MULTI_QUEUE_TIMEOUT 1500            // when queue > 1, wait for other data until timeout(ms) then do a callback

extern HardwareSerial Serial;
#if defined(HVAC_USE_SW_SERIAL)
ToshibaCarrierHvac::ToshibaCarrierHvac(uint8_t rxPin, uint8_t txPin) {
    #if defined(__AVR__)
    CustomSoftwareSerial *port = new CustomSoftwareSerial(rxPin, txPin);
    port->begin(BUADRATE, CSERIAL_8E1);
    #endif
    #if defined(ESP8266)
    SoftwareSerial *port = new SoftwareSerial(rxPin, txPin);
    port->begin(BUADRATE, SWSERIAL_8E1);
    #endif
    this->_serial = port;
    this->_swSerial = true;
}
#endif

ToshibaCarrierHvac::ToshibaCarrierHvac(HardwareSerial* port) {
    #if defined(ESP8266) || defined(ESP32)
    port->setRxBufferSize(MAX_RX_BYTE_READ);
    #endif
    port->begin(BUADRATE, SERIAL_8E1);
    #if defined(ESP8266)
    // port->swap();
    #endif
    this->_serial = port;
    this->_swSerial = false;
}

ToshibaCarrierHvac::~ToshibaCarrierHvac() {
    if (_swSerial) delete this->_serial;
}

// callback
void ToshibaCarrierHvac::setStatusUpdatedCallback(STATUS_UPDATED_CALLBACK_SIGNATURE) {
    this->statusUpdatedCallback = statusUpdatedCallback;
}
void ToshibaCarrierHvac::setSettingsUpdatedCallback(SETTINGS_UPDATED_CALLBACK_SIGNATURE) {
    this->settingsUpdatedCallback = settingsUpdatedCallback;
}
void ToshibaCarrierHvac::setUpdateCallback(UPDATE_CALLBACK_SIGNATURE) {
    this->updateCallback = updateCallback;
}
void ToshibaCarrierHvac::setWhichFunctionUpdatedCallback(WHICH_FUNCTION_UPDATED_CALLBACK_SIGNATURE) {
    this->whichFunctionUpdatedCallback = whichFunctionUpdatedCallback;
}

// normal function
void ToshibaCarrierHvac::sendCommand(byte data[], size_t dataLen) {
    _serial->write(data, dataLen);
    _lastSendWake = millis();
    _sendWake = 1;
    #ifdef HVAC_DEBUG
    DEBUG_PORT.print(F("HVAC> Sending data-> "));
    char temp[dataLen];
    for (uint8_t i=0; i<dataLen; i++) {
        DEBUG_PORT.print(data[i]);
        DEBUG_PORT.print(" ");
    }
    DEBUG_PORT.println("");
    #endif
}

void ToshibaCarrierHvac::sendCommand(const byte data[], size_t dataLen) {
    _serial->write(data, dataLen);
    _lastSendWake = millis();
    _sendWake = 1;
    #ifdef HVAC_DEBUG
    DEBUG_PORT.print(F("HVAC> Sending data-> "));
    char temp[dataLen];
    for (uint8_t i=0; i<dataLen; i++) {
        DEBUG_PORT.print(data[i]);
        DEBUG_PORT.print(" ");
    }
    DEBUG_PORT.println("");
    #endif
}

byte ToshibaCarrierHvac::getByteByName(const byte byteMap[], const char* valMap[], size_t byteLen, const char* name) {
    yield();
    for (uint8_t i=0; i<byteLen; i++) {
        if(strcasecmp(valMap[i], name) == 0) {
            return byteMap[i];
        }
    }
    return 255;
}

const char* ToshibaCarrierHvac::getNameByByte(const char* valMap[], const byte byteMap[], size_t byteLen, byte byteVal) {
    yield();
    for (uint8_t i=0; i<byteLen; i++) {
        if (byteMap[i] == byteVal) {
            return valMap[i];
        }
    }
    return valMap[byteLen];
}

byte ToshibaCarrierHvac::checksum(uint16_t key, size_t fnByte, byte fnVal) {
    uint16_t result = key - fnVal - fnByte;
    if (result > 256) return result - 256;
    else return result;
}

int8_t ToshibaCarrierHvac::temperatureCorrection(byte val) {
    if (val > 127) return ((256 - val) * (-1));
    else return val;
}

bool ToshibaCarrierHvac::createPacket(const byte header[], size_t headerLen, byte packetType, byte dataType, byte fnByte, byte fnVal) {
    if (dataType == 2) {    // setting packet
        byte packet[15];    // 15 bytes
        memset(packet, 0, sizeof(packet));  // set all index to 0
        memcpy(packet, header, headerLen);  // add header
        packet[3] = packetType; // add packet type
        // add unknown byte
        packet[6] = 7;
        packet[7] = 1;
        packet[8] = 48;
        packet[9] = 1;
        packet[11] = dataType;  // add data type
        packet[12] = fnByte;    // add function byte
        packet[13] = fnVal;     // add function value byte
        packet[14] = checksum(434, fnVal, fnByte);  // add checksum

        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Settings packet was created"));
        #endif

        // send created packet
        sendCommand(packet, sizeof(packet));
        return true;
    } else if (dataType == 1) {     // query packet
        byte packet[14];    // 14 bytes
        memset(packet, 0, sizeof(packet));  // set all index to 0
        memcpy(packet, header, headerLen);  // add header
        packet[3] = packetType; // add packet type
        // add unknown byte
        packet[6] = 6;
        packet[7] = 1;
        packet[8] = 48;
        packet[9] = 1;
        packet[11] = dataType;  // add data type
        packet[12] = fnByte;    // add function byte
        packet[13] = checksum(436, 0, fnByte);  // add checksum

        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Query packet was created"));
        #endif

        // send created packet
        sendCommand(packet, sizeof(packet));
        return true;
    }
    return false;
}

void ToshibaCarrierHvac::sendHandshake(void) {
    if (_firstRun && !_connected) { // send first handshake
        sendCommand(HANDSHAKE_SYN_PACKET_1, sizeof(HANDSHAKE_SYN_PACKET_1));
        delay(200);
        sendCommand(HANDSHAKE_SYN_PACKET_2, sizeof(HANDSHAKE_SYN_PACKET_2));
        delay(200);
        sendCommand(HANDSHAKE_SYN_PACKET_3, sizeof(HANDSHAKE_SYN_PACKET_3));
        delay(200);
        sendCommand(HANDSHAKE_SYN_PACKET_4, sizeof(HANDSHAKE_SYN_PACKET_4));
        delay(200);
        sendCommand(HANDSHAKE_SYN_PACKET_5, sizeof(HANDSHAKE_SYN_PACKET_5));
        delay(200);
        sendCommand(HANDSHAKE_SYN_PACKET_6, sizeof(HANDSHAKE_SYN_PACKET_6));
        delay(200);
        _sendWake = 1;
        _lastSendWake = millis();
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> First handshake sent. Waiting for SYN/ACK packet"));
        #endif
    } else if (_handshake && !_connected && !_ready) {  // when received syn/ack then send ack
        sendCommand(HANDSHAKE_ACK_PACKET_1, sizeof(HANDSHAKE_ACK_PACKET_1));
        delay(200);
        sendCommand(HANDSHAKE_ACK_PACKET_2, sizeof(HANDSHAKE_ACK_PACKET_2));
        delay(100);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Waiting for ready feedback"));
        #endif
        _handshake = 0;
        _ready = 1;
        _sendWake = 1;
        _lastSendWake = millis();
    }
}

void ToshibaCarrierHvac::queryall(void) {
    byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "STATE");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SETPOINT");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "MODE");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SWING");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "FANMODE");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PURE");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PSEL");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OP");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OFFTIMER");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "ONTIMER");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "ROOMTEMP");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OUTSIDETEMP");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(200);
    yield();
    packetMonitor();
}

void ToshibaCarrierHvac::queryTemperature(void) {
    byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "ROOMTEMP");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(100);
    fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OUTSIDETEMP");
    createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, fnByte, 0);
    delay(10);
}

bool ToshibaCarrierHvac::syncUserSettings(void) {
    if (strcasecmp(wantedSettings.state, userSettings.state) != 0) {    // state
        wantedSettings.state = userSettings.state;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "STATE");
        byte fnValue = getByteByName(STATE_BYTE, OFF_ON_MAP, sizeof(STATE_BYTE), wantedSettings.state);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted State-> "));
        DEBUG_PORT.println(wantedSettings.state);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    if (wantedSettings.setpoint != userSettings.setpoint) {    // setpoint
        if ((userSettings.setpoint >= 17) && (userSettings.setpoint <= 30)) {
            wantedSettings.setpoint = userSettings.setpoint;
            byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SETPOINT");
            byte fnValue = wantedSettings.setpoint;
            createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
            #ifdef HVAC_DEBUG
            DEBUG_PORT.print(F("HVAC> User wanted Setpoint-> "));
            DEBUG_PORT.println(wantedSettings.setpoint);
            #endif
            _lastSyncSettings = millis();
            return true;
        } else if (userSettings.setpoint < 17) {    // if value lower then minimun set to minimun
            #ifdef HVAC_DEBUG
            DEBUG_PORT.print(F("HVAC> error: User wanted out of range, set to minimum - User wanted Setpoint-> "));
            DEBUG_PORT.println(userSettings.setpoint);
            #endif
            userSettings.setpoint = 17;
            return false;
        } else if (userSettings.setpoint > 30) {    // if value higher then maximun set to maximum
            #ifdef HVAC_DEBUG
            DEBUG_PORT.print(F("HVAC> error: User wanted out of range, set to maximum - User wanted Setpoint-> "));
            DEBUG_PORT.println(userSettings.setpoint);
            #endif
            userSettings.setpoint = 30;
            return false;
        }
    }
    if (strcasecmp(wantedSettings.mode, userSettings.mode) != 0) {    // mode
        wantedSettings.mode = userSettings.mode;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "MODE");
        byte fnValue = getByteByName(MODE_BYTE, MODE_BYTE_MAP, sizeof(MODE_BYTE), wantedSettings.mode);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted Mode-> "));
        DEBUG_PORT.println(wantedSettings.mode);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    if (strcasecmp(wantedSettings.swing, userSettings.swing) != 0) {    // swing
        wantedSettings.swing = userSettings.swing;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SWING");
        byte fnValue = getByteByName(SWING_BYTE, OFF_ON_MAP, sizeof(SWING_BYTE), wantedSettings.swing);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted Swing-> "));
        DEBUG_PORT.println(wantedSettings.swing);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    if (strcasecmp(wantedSettings.fanMode, userSettings.fanMode) != 0) {    // fan mode
        wantedSettings.fanMode = userSettings.fanMode;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "FANMODE");
        byte fnValue = getByteByName(FANMODE_BYTE, FANMODE_BYTE_MAP, sizeof(FANMODE_BYTE), wantedSettings.fanMode);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted FanMode-> "));
        DEBUG_PORT.println(wantedSettings.fanMode);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    if (strcasecmp(wantedSettings.pure, userSettings.pure) != 0) {    // pure
        wantedSettings.pure = userSettings.pure;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PURE");
        byte fnValue = getByteByName(PURE_BYTE, OFF_ON_MAP, sizeof(PURE_BYTE), wantedSettings.pure);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted Pure-> "));
        DEBUG_PORT.println(wantedSettings.pure);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    if (strcasecmp(wantedSettings.powerSelect, userSettings.powerSelect) != 0) {    // power select
        wantedSettings.powerSelect = userSettings.powerSelect;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PSEL");
        byte fnValue = getByteByName(PSEL_BYTE, PSEL_BYTE_MAP, sizeof(PSEL_BYTE), wantedSettings.powerSelect);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted PowerSelect-> "));
        DEBUG_PORT.println(wantedSettings.powerSelect);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    if (strcasecmp(wantedSettings.operation, userSettings.operation) != 0) {    // operation
        wantedSettings.operation = userSettings.operation;
        byte fnByte = getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OP");
        byte fnValue = getByteByName(OP_BYTE, OP_BYTE_MAP, sizeof(OP_BYTE), wantedSettings.operation);
        createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 2, fnByte, fnValue);
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> User wanted Operation-> "));
        DEBUG_PORT.println(wantedSettings.operation);
        #endif
        _lastSyncSettings = millis();
        return true;
    }
    return false;
}

bool ToshibaCarrierHvac::processData(byte* data, size_t dataLen) {
    if (data[0] == getByteByName(DATA_TYPE, DATA_TYPE_MAP, sizeof(DATA_TYPE), "SETTING")) {    // process setting data
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> Process data result: "));
        #endif
        // connection status
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "STATUS")) {  // status
            #ifdef HVAC_DEBUG
            DEBUG_PORT.print(F("Status-> "));
            #endif
            if (data[2] == getByteByName(STATUS_BYTE, STATUS_BYTE_MAP, sizeof(STATUS_BYTE), "READY")) {
                #ifdef HVAC_DEBUG
                DEBUG_PORT.println(F("READY"));
                #endif
                _connected = 1;
                _sendWake = 0;
                _ready = 0;
                return true;
            } else {
                #ifdef HVAC_DEBUG
                DEBUG_PORT.println(getNameByByte(STATUS_BYTE_MAP, STATUS_BYTE, sizeof(STATUS_BYTE), data[1]));
                #endif
                return false;
            }
        }
        // data
        if (_connected) {   // process data when connected
            if ((data[1] == 187) || (data[1] == 190) || (data[1] == 144) || (data[1] == 148)) {    // status
                hvacStatus receiveStatus;
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "ROOMTEMP")) {    // room temperature
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("RoomTemp-> "));
                    #endif
                    receiveStatus.roomTemperature = temperatureCorrection(data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveStatus.roomTemperature);
                    #endif
                    if (currentStatus.roomTemperature != receiveStatus.roomTemperature) {
                        if (statusUpdatedCallback) {
                            currentStatus.roomTemperature = receiveStatus.roomTemperature;
                            _statusCallbackBucket++;
                            _lastStatusCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            currentStatus.roomTemperature = receiveStatus.roomTemperature;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            currentStatus.roomTemperature = receiveStatus.roomTemperature;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            currentStatus.roomTemperature = receiveStatus.roomTemperature;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OUTSIDETEMP")) { // outside temperature and cdu state
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("OutsideTemp-> "));
                    #endif
                    receiveStatus.outsideTemperature = temperatureCorrection(data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveStatus.outsideTemperature);
                    #endif
                    if (receiveStatus.outsideTemperature == 127) {  // cdu not running, not update outside temperature and update cdu state
                        #ifdef HVAC_DEBUG
                        DEBUG_PORT.println(F("HVAC> Not update outside temperature, condensing unit not running"));
                        #endif
                        receiveStatus.running = 0;
                        if (currentStatus.running != receiveStatus.running) {
                            if (statusUpdatedCallback) {
                                currentStatus.running = receiveStatus.running;
                                _statusCallbackBucket++;
                                _lastStatusCallback = millis();
                                return true;
                            } else if (updateCallback) {
                                currentStatus.running = receiveStatus.running;
                                _updateCallbackBucket++;
                                _lastUpdateCallback = millis();
                                return true;
                            } else if (whichFunctionUpdatedCallback) {
                                currentStatus.running = receiveStatus.running;
                                whichFunctionUpdatedCallback("CDU_STATE");
                                return true;
                            } else {
                                currentStatus.running = receiveStatus.running;
                                return true;
                            }
                        }
                        return false;
                    } else if (currentStatus.outsideTemperature != receiveStatus.outsideTemperature) {
                        currentStatus.running = 1;
                        if (statusUpdatedCallback) {
                            currentStatus.outsideTemperature = receiveStatus.outsideTemperature;
                            _statusCallbackBucket++;
                            _lastStatusCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            currentStatus.outsideTemperature = receiveStatus.outsideTemperature;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            currentStatus.outsideTemperature = receiveStatus.outsideTemperature;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            whichFunctionUpdatedCallback("CDU_STATE");
                        } else {
                            currentStatus.outsideTemperature = receiveStatus.outsideTemperature;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OFFTIMER")) {   // off timer
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("OffTimer-> "));
                    #endif
                    receiveStatus.offTimer = getNameByByte(OFF_ON_MAP, TIMER_BYTE, sizeof(TIMER_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveStatus.offTimer);
                    #endif
                    if (currentStatus.offTimer != receiveStatus.offTimer) {
                        if (statusUpdatedCallback) {
                            currentStatus.offTimer = receiveStatus.offTimer;
                            _statusCallbackBucket++;
                            _lastStatusCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            currentStatus.offTimer = receiveStatus.offTimer;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            currentStatus.offTimer = receiveStatus.offTimer;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            currentStatus.offTimer = receiveStatus.offTimer;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "ONTIMER")) {   // on timer
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("OnTimer-> "));
                    #endif
                    receiveStatus.onTimer = getNameByByte(OFF_ON_MAP, TIMER_BYTE, sizeof(TIMER_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveStatus.onTimer);
                    #endif
                    if (currentStatus.onTimer != receiveStatus.onTimer) {
                        if (statusUpdatedCallback) {
                            currentStatus.onTimer = receiveStatus.onTimer;
                            _statusCallbackBucket++;
                            _lastStatusCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            currentStatus.onTimer = receiveStatus.onTimer;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            currentStatus.onTimer = receiveStatus.onTimer;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            currentStatus.onTimer = receiveStatus.onTimer;
                            return true;
                        }
                    }
                    return false;
                }
                return false;
            } else {    // setting
                hvacSettings receiveSettings;
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "STATE")) {   // state
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("State-> "));
                    #endif
                    receiveSettings.state = getNameByByte(OFF_ON_MAP, STATE_BYTE, sizeof(STATE_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.state);
                    #endif
                    if (currentSettings.state != receiveSettings.state) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.state = userSettings.state = currentSettings.state = receiveSettings.state;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.state = userSettings.state = currentSettings.state = receiveSettings.state;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.state = userSettings.state = currentSettings.state = receiveSettings.state;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.state = userSettings.state = currentSettings.state = receiveSettings.state;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SETPOINT")) {    // setpoint
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("Setpoint-> "));
                    #endif
                    receiveSettings.setpoint = data[2];
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.setpoint);
                    #endif
                    if (currentSettings.setpoint != receiveSettings.setpoint) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.setpoint = userSettings.setpoint = currentSettings.setpoint = receiveSettings.setpoint;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.setpoint = userSettings.setpoint = currentSettings.setpoint = receiveSettings.setpoint;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.setpoint = userSettings.setpoint = currentSettings.setpoint = receiveSettings.setpoint;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.setpoint = userSettings.setpoint = currentSettings.setpoint = receiveSettings.setpoint;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "MODE")) {    // mode
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("Mode-> "));
                    #endif
                    receiveSettings.mode = getNameByByte(MODE_BYTE_MAP, MODE_BYTE, sizeof(MODE_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.mode);
                    #endif
                    if (currentSettings.mode != receiveSettings.mode) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.mode = userSettings.mode = currentSettings.mode = receiveSettings.mode;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.mode = userSettings.mode = currentSettings.mode = receiveSettings.mode;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.mode = userSettings.mode = currentSettings.mode = receiveSettings.mode;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.mode = userSettings.mode = currentSettings.mode = receiveSettings.mode;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SWING")) {   // swing
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("Swing-> "));
                    #endif
                    receiveSettings.swing = getNameByByte(OFF_ON_MAP, SWING_BYTE, sizeof(SWING_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.swing);
                    #endif
                    if (currentSettings.swing != receiveSettings.swing) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.swing = userSettings.swing = currentSettings.swing = receiveSettings.swing;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.swing = userSettings.swing = currentSettings.swing = receiveSettings.swing;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.swing = userSettings.swing = currentSettings.swing = receiveSettings.swing;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.swing = userSettings.swing = currentSettings.swing = receiveSettings.swing;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "FANMODE")) { // fan mode
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("Fanmode-> "));
                    #endif
                    receiveSettings.fanMode = getNameByByte(FANMODE_BYTE_MAP, FANMODE_BYTE, sizeof(FANMODE_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.fanMode);
                    #endif
                    if (currentSettings.fanMode != receiveSettings.fanMode) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.fanMode = userSettings.fanMode = currentSettings.fanMode = receiveSettings.fanMode;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.fanMode = userSettings.fanMode = currentSettings.fanMode = receiveSettings.fanMode;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.fanMode = userSettings.fanMode = currentSettings.fanMode = receiveSettings.fanMode;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.fanMode = userSettings.fanMode = currentSettings.fanMode = receiveSettings.fanMode;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PURE")) {    // pure
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("Pure-> "));
                    #endif
                    receiveSettings.pure = getNameByByte(OFF_ON_MAP, PURE_BYTE, sizeof(PURE_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.pure);
                    #endif
                    if (currentSettings.pure != receiveSettings.pure) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.pure = userSettings.pure = currentSettings.pure = receiveSettings.pure;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.pure = userSettings.pure = currentSettings.pure = receiveSettings.pure;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.pure = userSettings.pure = currentSettings.pure = receiveSettings.pure;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.pure = userSettings.pure = currentSettings.pure = receiveSettings.pure;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PSEL")) {    // power select
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("PowerSelect-> "));
                    #endif
                    receiveSettings.powerSelect = getNameByByte(PSEL_BYTE_MAP, PSEL_BYTE, sizeof(PSEL_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.powerSelect);
                    #endif
                    if (currentSettings.powerSelect != receiveSettings.powerSelect) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.powerSelect = userSettings.powerSelect = currentSettings.powerSelect = receiveSettings.powerSelect;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.powerSelect = userSettings.powerSelect = currentSettings.powerSelect = receiveSettings.powerSelect;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.powerSelect = userSettings.powerSelect = currentSettings.powerSelect = receiveSettings.powerSelect;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.powerSelect = userSettings.powerSelect = currentSettings.powerSelect = receiveSettings.powerSelect;
                            return true;
                        }
                    }
                    return false;
                }
                if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OP")) {    // operation
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.print(F("Operation-> "));
                    #endif
                    receiveSettings.operation = getNameByByte(OP_BYTE_MAP, OP_BYTE, sizeof(OP_BYTE), data[2]);
                    #ifdef HVAC_DEBUG
                    DEBUG_PORT.println(receiveSettings.operation);
                    #endif
                    if (currentSettings.operation != receiveSettings.operation) {
                        if (settingsUpdatedCallback) {
                            wantedSettings.operation = userSettings.operation = currentSettings.operation = receiveSettings.operation;
                            _settingsCallbackBucket++;
                            _lastSettingsCallback = millis();
                            return true;
                        } else if (updateCallback) {
                            wantedSettings.operation = userSettings.operation = currentSettings.operation = receiveSettings.operation;
                            _updateCallbackBucket++;
                            _lastUpdateCallback = millis();
                            return true;
                        } else if (whichFunctionUpdatedCallback) {
                            wantedSettings.operation = userSettings.operation = currentSettings.operation = receiveSettings.operation;
                            whichFunctionUpdatedCallback(getNameByByte(FUNCTION_BYTE_MAP, FUNCTION_BYTE, sizeof(FUNCTION_BYTE), data[2]));
                            return true;
                        } else {
                            wantedSettings.operation = userSettings.operation = currentSettings.operation = receiveSettings.operation;
                            return true;
                        }
                    }
                    return false;
                }
                #ifdef HVAC_DEBUG
                DEBUG_PORT.println(F("error: Received unknown function, skipped"));
                #endif
                return false;
            }
        } else {    // received data when not connected
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("error: Received data when not connected, skipped"));
            #endif
            return false;
        }
    } else if (data[0] == getByteByName(DATA_TYPE, DATA_TYPE_MAP, sizeof(DATA_TYPE), "QUERY")) {    // setting changed reply
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> Received setting changed reply-> "));
        #endif
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "STATE")) {   // state
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("STATE"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SETPOINT")) {    // setpoint
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("SETPOINT"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "MODE")) {    // mode
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("MODE"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "SWING")) {   // swing
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("SWING"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "FANMODE")) { //fan mode
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("FANMODE"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PURE")) {    // pure
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("PURE"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "PSEL")) {    // power select
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("PSEL"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        if (data[1] == getByteByName(FUNCTION_BYTE, FUNCTION_BYTE_MAP, sizeof(FUNCTION_BYTE), "OP")) {    // operation
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("OP"));
            #endif
            return createPacket(PACKET_HEADER, sizeof(PACKET_HEADER), 16, 1, data[1], 0);
        }
        return false;
    } else {    // received unknown data type
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("error: Received unknown data type, ignored"));
        #endif
        return false;
    }
    return false;
}

bool ToshibaCarrierHvac::readPacket(byte data[], size_t dataLen) {
    byte newData[3];
    if (data[3] == getByteByName(PACKET_TYPE, PACKET_TYPE_MAP, sizeof(PACKET_TYPE), "FEEDBACK")) {  // feedback
        newData[0] = data[11]; newData[1] = data[12]; newData[2] = data[13];
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> Received feedback from hvac with data: "));
        DEBUG_PORT.print(newData[1]);
        DEBUG_PORT.print(F(" "));
        DEBUG_PORT.println(newData[2]);
        #endif
        return processData(newData, dataLen);
    } else if (data[3] == getByteByName(PACKET_TYPE, PACKET_TYPE_MAP, sizeof(PACKET_TYPE), "REPLY")) {  // reply
        newData[0] = data[13]; newData[1] = data[14]; newData[2] = data[15];
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> Received reply from hvac with data: "));
        DEBUG_PORT.print(newData[1]);
        DEBUG_PORT.print(F(" "));
        DEBUG_PORT.println(newData[2]);
        #endif
        return processData(newData, dataLen);
    } else if (data[3] == getByteByName(PACKET_TYPE, PACKET_TYPE_MAP, sizeof(PACKET_TYPE), "SYN/ACK")) {    // syn/ack
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Received handshake SYN/ACK"));
        #endif
        _handshake = 1;
        return true;
    } else if (data[3] == getByteByName(PACKET_TYPE, PACKET_TYPE_MAP, sizeof(PACKET_TYPE), "ACK")) {    // ack
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Received confirm handshake, your code may crash or hw problem cause node mcu restarted"));
        #endif
        _ready = _handshake = _init = 0;
        _connected = 1;
        return true;
    } else if (_connected && _init) {   // unknown
        #ifdef HVAC_DEBUG
        DEBUG_PORT.print(F("HVAC> Received "));
        DEBUG_PORT.print(getNameByByte(PACKET_TYPE_MAP, PACKET_TYPE, sizeof(PACKET_TYPE_MAP), data[3]));
        DEBUG_PORT.println(F(" packet from hvac"));
        #endif
        return false;
    } else return false;
}

bool ToshibaCarrierHvac::findHeader(byte data[], size_t dataLen, const byte header[], size_t headerLen) {
    bool match[headerLen];
    bool master[headerLen];
    memset(master, 1, headerLen);
    for (uint8_t i=0; i<headerLen; i++) {
        if (header[i] == data[i]) match[i] = 1;
    }
    if (memcmp(match, master, headerLen) == 0) {
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Header of packet found"));
        #endif
        return true;
    } else {
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Header of packet not found"));
        #endif
        return false;
    }
}

uint8_t ToshibaCarrierHvac::findHeaderStart(byte data[], size_t dataLen, const byte header[], size_t headerLen) {
  byte data2[dataLen];
  byte temp;
  uint8_t num=0;
  memcpy(data2, data, dataLen);
  while (((data2[0] != header[0]) || (data2[1] != header[1]) || (data2[2] != header[2])) && (num < dataLen)) {
    byte temp = data2[0];
    for (uint8_t i=0; i<dataLen; i++) {
      data2[i] = data2[i+1];
    }
    data2[dataLen-1] = temp;
    num++;
  }
  if (num == dataLen) {
      #ifdef HVAC_DEBUG
      DEBUG_PORT.println(F("HVAC> Header start not found"));
      #endif
      return 255;
  } else {
      if (num > 0) {
            #ifdef HVAC_DEBUG
            DEBUG_PORT.print(F("HVAC> Header start found at: "));
            DEBUG_PORT.println(num+1);
            #endif
            return num;
        } else {
            #ifdef HVAC_DEBUG
            DEBUG_PORT.print(F("HVAC> Header start found at: "));
            DEBUG_PORT.println(num);
            #endif
            return num;
        }
  }
}

bool ToshibaCarrierHvac::chunkSplit(byte data[], size_t dataLen) {
    uint8_t _start , start , restSize = dataLen;
    if (!_ready && !_connected) _start = start = findHeaderStart(data, dataLen, HANDSHAKE_HEADER, sizeof(HANDSHAKE_HEADER));
    else _start = start = findHeaderStart(data, dataLen, PACKET_HEADER, sizeof(PACKET_HEADER));
    if (start == 255) {
        _start = start = findHeaderStart(data, dataLen, CONFIRM_HEADER, sizeof(CONFIRM_HEADER));
        if (start == 255) return false;
    }
    while(restSize >= 10) {
        uint8_t dataSize = data[start+6] + 8;
        if ((restSize >= dataSize) && ((dataSize >= 8) || (dataSize <= 17))) {
            byte newData[dataSize];
            for (;start < (dataSize+_start); start++) {
                newData[start-_start] = data[start];
            }
            restSize -= dataSize;
            _start = start;
            yield();
            if (readPacket(newData, dataSize));
        } else if (dataSize == 14) {
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("HVAC> Received query packet, ignored"));
            #endif
            restSize -= dataSize;
            _start = start;
        } else if (dataSize >= 250) {
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("HVAC> Received large packet, not process this packet to avoid overflow"));
            #endif
            return false;
        } else {
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("HVAC> Packet length invalid"));
            #endif
            return false;
        }
    }
    return true;
}

bool ToshibaCarrierHvac::packetMonitor(void) {
    if (_serial->available()) { // if received 1st data then continue to read rest of data until timeout
        byte rxBuffer[MAX_RX_BYTE_READ];
        uint8_t dataLen = 0;
        uint32_t start = millis();
        while (((millis() - start) < RX_READ_TIMEOUT) && (dataLen < MAX_RX_BYTE_READ)) {
            if (_serial->available()) {
                uint8_t c = _serial->read();
                rxBuffer[dataLen++] = c;
            }
        }

        #ifdef HVAC_DEBUG
        if ((MAX_RX_BYTE_READ - dataLen) < 1) DEBUG_PORT.println(F("HVAC> Serial buffer almost full"));
        DEBUG_PORT.print(F("HVAC> Received data length: "));
        DEBUG_PORT.println(dataLen);
        DEBUG_PORT.print(F("HVAC> Data: "));
        for (uint8_t i=0; i<dataLen; i++) {
            DEBUG_PORT.print(rxBuffer[i]);
            DEBUG_PORT.print(" ");
        }
        DEBUG_PORT.println("");
        #endif

        _lastReceive = millis();
        _sendWake = 0;
        if (!_connected) _sendWake = 1;
        if ((dataLen == 15) && (dataLen == 17) && findHeader(rxBuffer, dataLen, PACKET_HEADER, sizeof(PACKET_HEADER))) {
            return readPacket(rxBuffer, dataLen);
        } else return chunkSplit(rxBuffer, dataLen);
    } else return false;
}

void ToshibaCarrierHvac::handleHvac(void) {
    if (!_connected) {
        sendHandshake();
    }

    if (_firstRun) {
        _idleTimeout = IDLE_TIMEOUT * 60 * 1000;
        _connectionTimeout = CONNECTION_TIMEOUT * 60 * 1000;
        _queryallDelay = START_DELAY * 1000;
        _firstRun = 0;
    }

    // query all data once after connected
    if (((millis() - _lastReceive) >= _queryallDelay) && !_init && _connected) {
        queryall();
        _init = 1;
    }

    packetMonitor();    // process data

    // idle timeout
    if (((millis() - _lastReceive) >= _idleTimeout) && !_sendWake && _init) {
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Max idle timeout reached"));
        #endif
        // try to query room temperature
        queryTemperature();
        _sendWake = 1;
        _lastSendWake = millis();
    }

    // connection timeout
    if (((millis() - _lastSendWake) >= _connectionTimeout) && _sendWake) {
        #ifdef HVAC_DEBUG
        DEBUG_PORT.println(F("HVAC> Connection timeout, try to send new handshake"));
        #endif
        _firstRun = 1;
        _handshake = _ready = _connected = _sendWake = _init = 0;
        _lastReceive = _lastSendWake = millis();
    }

    if(_init && ((millis() - _lastSyncSettings) >= SETTINGS_SEND_DELAY)) {
        if (syncUserSettings()) {
            #ifdef HVAC_DEBUG
            DEBUG_PORT.println(F("HVAC> New setting has been sync, waiting for reply"));
            #endif
        }
    }
    // settings callback limit
    if ((_settingsCallbackBucket == 1) && ((millis() - _lastSettingsCallback) >= SINGLE_QUEUE_TIMEOUT)) {
        settingsUpdatedCallback(currentSettings);
        _settingsCallbackBucket = 0;
    } else if ((_settingsCallbackBucket > 1) && ((millis() - _lastSettingsCallback) >= MULTI_QUEUE_TIMEOUT)) {
        settingsUpdatedCallback(currentSettings);
        _settingsCallbackBucket = 0;
    }
    // status callback limit
    if ((_statusCallbackBucket == 1) && ((millis() - _lastStatusCallback) >= SINGLE_QUEUE_TIMEOUT)) {
        statusUpdatedCallback(currentStatus);
        _statusCallbackBucket = 0;
    } else if ((_statusCallbackBucket > 1) && ((millis() - _lastStatusCallback) >= MULTI_QUEUE_TIMEOUT)) {
        statusUpdatedCallback(currentStatus);
        _statusCallbackBucket = 0;
    }
    // update callback limit
    if ((_updateCallbackBucket == 1) && ((millis() - _lastUpdateCallback) >= SINGLE_QUEUE_TIMEOUT)) {
        updateCallback();
        _updateCallbackBucket = 0;
    } else if ((_updateCallbackBucket > 1) && ((millis() - _lastUpdateCallback) >= MULTI_QUEUE_TIMEOUT)) {
        updateCallback();
        _updateCallbackBucket = 0;
    }
}

void ToshibaCarrierHvac::applyPreset(hvacSettings newSettings) {
    userSettings = newSettings;
}

void ToshibaCarrierHvac::setState(const char* newState) {
    userSettings.state = newState;
}

void ToshibaCarrierHvac::setSetpoint(uint8_t newSetpoint) {
    userSettings.setpoint = newSetpoint;
}

void ToshibaCarrierHvac::setMode(const char* newMode) {
    userSettings.mode = newMode;
}

void ToshibaCarrierHvac::setSwing(const char* newSwing) {
    userSettings.swing = newSwing;
}

void ToshibaCarrierHvac::setFanMode(const char* newFanMode) {
    userSettings.fanMode = newFanMode;
}

void ToshibaCarrierHvac::setPure(const char* newPure) {
    userSettings.pure = newPure;
}

void ToshibaCarrierHvac::setPowerSelect(const char* newPowerSelect) {
    userSettings.powerSelect = newPowerSelect;
}

void ToshibaCarrierHvac::setOperation(const char* newOperation) {
    userSettings.operation = newOperation;
}

hvacStatus ToshibaCarrierHvac::getStatus(void) {
    return currentStatus;
}

hvacSettings ToshibaCarrierHvac::getSettings(void) {
    return currentSettings;
}

int8_t ToshibaCarrierHvac::getRoomTemperature(void) {
    return currentStatus.roomTemperature;
}

int8_t ToshibaCarrierHvac::getOutsideTemperature(void) {
    return currentStatus.outsideTemperature;
}

const char* ToshibaCarrierHvac::getState(void) {
    return currentSettings.state;
}

uint8_t ToshibaCarrierHvac::getSetpoint(void) {
    return currentSettings.setpoint;
}

const char* ToshibaCarrierHvac::getMode(void) {
    return currentSettings.mode;
}

const char* ToshibaCarrierHvac::getSwing(void) {
    return currentSettings.swing;
}

const char* ToshibaCarrierHvac::getFanMode(void) {
    return currentSettings.fanMode;
}

const char* ToshibaCarrierHvac::getPure(void) {
    return currentSettings.pure;
}

const char* ToshibaCarrierHvac::getOffTimer(void) {
    return currentStatus.offTimer;
}

const char* ToshibaCarrierHvac::getOnTimer(void) {
    return currentStatus.onTimer;
}

const char* ToshibaCarrierHvac::getPowerSelect(void) {
    return currentSettings.powerSelect;
}

const char* ToshibaCarrierHvac::getOperation(void) {
    return currentSettings.operation;
}

bool ToshibaCarrierHvac::isCduRunning(void) {
    return currentStatus.running;
}

bool ToshibaCarrierHvac::isConnected(void) {
    return _connected;
}

void ToshibaCarrierHvac::forceQueryAllData(void) {
    _init = 0;
}