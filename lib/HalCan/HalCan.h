#pragma once

#include <Arduino.h>
#include "driver/twai.h"
#include "../../src/Config.h"

class HalCan {
public:
    static HalCan& getInstance() {
        static HalCan instance;
        return instance;
    }

    // Initialize CAN TWAI interface
    bool begin(gpio_num_t tx_pin = CAN_TX_PIN, gpio_num_t rx_pin = CAN_RX_PIN, const twai_timing_config_t& timing = CAN_BAUDRATE);

    // Stop TWAI interface
    void end();

    // Read a CAN frame from bus
    bool readFrame(twai_message_t& rx_msg, uint32_t timeout_ms = 0);

    // Write a CAN frame to bus
    bool writeFrame(const twai_message_t& tx_msg, uint32_t timeout_ms = 0);

    // Get current TWAI state (useful for detecting BUS_OFF)
    twai_state_t getState();

    // Deep debug: dump Error Counters and State
    void dumpStatus();

    // Trigger auto-recovery if in BUS_OFF state
    bool recover();

private:
    HalCan() = default;
    ~HalCan() = default;
    HalCan(const HalCan&) = delete;
    HalCan& operator=(const HalCan&) = delete;

    bool _initialized = false;
};
