#include "Obd2Decoder.h"

Obd2Decoder::Obd2Decoder() : _state(ObdState::IDLE), _lastRxTime(0), _hasReceived(false) {
    // Initializer list covers defaults
}

twai_message_t Obd2Decoder::buildRequest(uint8_t mode, uint8_t pid) {
    twai_message_t req = {0};
    req.identifier = CAN_ID_OBD_REQUEST;
    req.extd = 0;              // Standard 11-bit ID
    req.rtr = 0;               // Data frame
    req.data_length_code = 8;  // DLC is always 8 for classic CAN OBD requests

    // Payload: [Length] [Mode] [PID] [0] ... [0]
    req.data[0] = 0x02; // We are sending 2 bytes (Mode + PID)
    req.data[1] = mode;
    req.data[2] = pid;
    for (int i = 3; i < 8; ++i) {
        req.data[i] = 0x55; // Padding, some ECUs prefer 00 or 55
    }

    return req;
}

twai_message_t Obd2Decoder::generateTxRequest() {
    // Basic State Machine to alternate requests
    switch (_state) {
        case ObdState::IDLE:
        case ObdState::WAITING_RPM:
            _state = ObdState::WAITING_SPEED; // Next expected wait
            return buildRequest(OBD_MODE_CURRENT_DATA, PID_ENGINE_RPM);
        
        case ObdState::WAITING_SPEED:
            _state = ObdState::WAITING_TEMP; // Next expected wait
            return buildRequest(OBD_MODE_CURRENT_DATA, PID_VEHICLE_SPEED);

        case ObdState::WAITING_TEMP:
            _state = ObdState::WAITING_LOAD; 
            return buildRequest(OBD_MODE_CURRENT_DATA, PID_COOLANT_TEMP);

        case ObdState::WAITING_LOAD:
            _state = ObdState::WAITING_RPM; 
            return buildRequest(OBD_MODE_CURRENT_DATA, PID_ENGINE_LOAD);

        default:
            _state = ObdState::WAITING_RPM;
            return buildRequest(OBD_MODE_CURRENT_DATA, PID_ENGINE_RPM);
    }
}

bool Obd2Decoder::isConnected() const {
    if (!_hasReceived) return false;
    // If we received a message in the last 2.5 seconds, we consider it connected
    // This assumes OBD2_REQUEST_INTERVAL is around 250ms/500ms
    return (millis() - _lastRxTime) < 2500;
}

void Obd2Decoder::processRxFrame(const twai_message_t& msg) {
    // Filter OBD-II Response frames range (0x7E8 to 0x7EF)
    if (msg.identifier >= CAN_ID_OBD_REPLY_MIN && msg.identifier <= CAN_ID_OBD_REPLY_MAX) {
        if (msg.data_length_code >= 3) {
            uint8_t count = msg.data[0];    // How many valid bytes in payload
            if (count > 7) count = 7;       // Safeguard, max OBD response fits in single frame if small amount

            uint8_t mode = msg.data[1];     // Usually 0x41 for a 0x01 request
            if (mode == (OBD_MODE_CURRENT_DATA + 0x40)) {
                _hasReceived = true;
                _lastRxTime = millis();     // We got a VALID OBD2 reply!
                uint8_t pid = msg.data[2];
                // Extract useful payload based on length byte (discounting mode and pid bytes)
                decodePayload(pid, &msg.data[3], count - 2); 
            }
        }
    }
}

void Obd2Decoder::decodePayload(uint8_t pid, const uint8_t* payload, uint8_t payload_len) {
    if (payload_len < 1) return;

    switch (pid) {
        case PID_ENGINE_RPM:
            if (payload_len >= 2) {
                // Formula: ((A * 256) + B) / 4
                uint16_t rpm_raw = (payload[0] << 8) | payload[1];
                _data.rpm = rpm_raw / 4;
                LOG_INFO("Decoded RPM: %d", _data.rpm);
            }
            break;

        case PID_VEHICLE_SPEED:
            if (payload_len >= 1) {
                // Formula: A
                _data.speed = payload[0];
                LOG_INFO("Decoded Speed: %d km/h", _data.speed);
            }
            break;

        case PID_COOLANT_TEMP:
            if (payload_len >= 1) {
                // Formula: A - 40
                _data.coolantTemp = payload[0] - 40;
                LOG_INFO("Decoded Coolant Temp: %d C", _data.coolantTemp);
            }
            break;

        case PID_ENGINE_LOAD:
            if (payload_len >= 1) {
                // Formula: A * 100 / 255
                _data.engineLoad = (payload[0] * 100) / 255;
                LOG_INFO("Decoded Engine Load: %d %%", _data.engineLoad);
            }
            break;

        default:
            LOG_WARN("Received unhandled PID: %02X", pid);
            break;
    }
}
