#pragma once

#include <Arduino.h>
#include "driver/twai.h"
#include "../../src/Config.h"

// Standard OBD-II PIDs (Mode 01)
#define PID_ENGINE_LOAD     0x04
#define PID_COOLANT_TEMP    0x05
#define PID_ENGINE_RPM      0x0C
#define PID_VEHICLE_SPEED   0x0D
#define PID_INTAKE_TEMP     0x0F
#define PID_MAF_FLOW        0x10
#define PID_THROTTLE        0x11
// Modes
#define OBD_MODE_CURRENT_DATA 0x01

// Known standard request ID (11-bit)
#define CAN_ID_OBD_REQUEST_STD   0x7DF
#define CAN_ID_OBD_REPLY_STD_MIN 0x7E8
#define CAN_ID_OBD_REPLY_STD_MAX 0x7EF

// Known extended request ID (29-bit)
#define CAN_ID_OBD_REQUEST_EXT   0x18DB33F1
// 29-bit responses usually match 0x18DAF100 to 0x18DAF1FF
#define CAN_ID_OBD_REPLY_EXT_MASK 0xFFFFFF00
#define CAN_ID_OBD_REPLY_EXT_VAL  0x18DAF100

// Protocol type for Auto-Discovery
enum class ObdProtocol {
    UNKNOWN,
    STD_11_BIT,
    EXT_29_BIT
};

// Structure to hold decodable OBD2 values globally
struct ObdData {
    int rpm = 0;              // RPM
    int speed = 0;            // km/h
    int coolantTemp = 0;      // Celsius
    int engineLoad = 0;       // %
    // Additional parameters as needed in future
};

// OBD2 Polling State Enum
enum class ObdState {
    IDLE,
    WAITING_RPM,
    WAITING_SPEED,
    WAITING_TEMP,
    WAITING_LOAD
};

class Obd2Decoder {
public:
    Obd2Decoder();
    
    // Call this inside a polling task to process incoming CAN frames
    void processRxFrame(const twai_message_t& msg);

    // Call this periodically to request new data based on the internal state machine
    twai_message_t generateTxRequest();

    // Get a reference to the latest decoded data
    const ObdData& getData() const { return _data; }

    // Check if OBD2 connection is active based on timeout
    bool isConnected() const;

private:
    ObdData _data;
    ObdState _state;
    uint32_t _lastRxTime; // Track last valid response tick
    bool _hasReceived;    // Flag to ensure we don't show true at boot
    ObdProtocol _protocol;
    uint32_t _timeoutCount; // To track auto-discovery attempts
    
    // Internal helper to build request payload
    twai_message_t buildRequest(uint8_t mode, uint8_t pid, ObdProtocol proto);
    
    // Internal helper to decode specific PID payload
    void decodePayload(uint8_t pid, const uint8_t* data, uint8_t len);
};
