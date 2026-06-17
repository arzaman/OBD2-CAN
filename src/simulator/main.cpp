#include <Arduino.h>
#include <M5Unified.h>

#include "../Config.h"
#include "../../lib/HalCan/HalCan.h"
#include "../../lib/Obd2/Obd2Decoder.h" // Just to reuse PID constants
#include <Adafruit_NeoPixel.h>

#define LED_PIN 35
#define NUM_LEDS 1
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

bool emulate_29_bit = false; // Default 11-bit

void updateLed() {
    if (emulate_29_bit) {
        pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
    } else {
        pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
    }
    pixels.show();
}

// -----------------------------------------------------------------------------
// ECU SIMULATOR TASK
// -----------------------------------------------------------------------------
void EcuSimulatorTask(void* pvParameters) {
    LOG_INFO("ECU Simulator Listening Task started. Mode: %s", emulate_29_bit ? "29-bit" : "11-bit");

    static float sim_rpm = 800.0f;
    const float sim_speed = 0.0f;
    const float sim_temp = 80.0f;
    const float sim_load = 20.0f;
    const float sim_fuel = 80.0f;
    const float sim_throttle = 20.0f;
    uint32_t last_physics_update = millis();

    static uint32_t last_click_time = 0;
    static bool waiting_second_click = false;

    while (1) {
        // --- 1. Physics Engine ---
        M5.update();
        uint32_t now_ms = millis();
        float dt = (now_ms - last_physics_update) / 1000.0f;
        last_physics_update = now_ms;

        // Double Click detection to switch mode
        if (M5.BtnA.wasPressed()) {
            if (waiting_second_click && (now_ms - last_click_time < 400)) {
                emulate_29_bit = !emulate_29_bit;
                updateLed();
                LOG_INFO("Mode switched manually to: %s", emulate_29_bit ? "29-bit" : "11-bit");
                waiting_second_click = false;
            } else {
                waiting_second_click = true;
                last_click_time = now_ms;
            }
        }
        if (waiting_second_click && (now_ms - last_click_time > 400)) {
            waiting_second_click = false;
        }

        // Button A = Accelerator. Releasing it = Engine Brake
        if (M5.BtnA.isPressed()) {
            sim_rpm += 2000.0f * dt;      // Accel 2000 RPM/s
            if (sim_rpm > 6500.0f) sim_rpm = 6500.0f;
        } else {
            sim_rpm -= 3000.0f * dt;      // Drops fast when releasing throttle
            if (sim_rpm < 800.0f) sim_rpm = 800.0f;
        }

        // --- Hardware Auto-Recovery ---
        twai_state_t sim_state = HalCan::getInstance().getState();
        if (sim_state != TWAI_STATE_RUNNING) {
            HalCan::getInstance().recover();
            vTaskDelay(pdMS_TO_TICKS(sim_state == TWAI_STATE_BUS_OFF ? 500 : 50));
            continue;
        }

        twai_message_t rx_msg;
        // Block until a frame is received
        if (HalCan::getInstance().readFrame(rx_msg, 50)) {
            LOG_INFO(">>> DEBUG: RAW FRAME RECEIVED. ID: 0x%03X, DLC: %d, EXTD: %d", rx_msg.identifier, rx_msg.data_length_code, rx_msg.extd);
            
            uint32_t expected_req_id = emulate_29_bit ? CAN_ID_OBD_REQUEST_EXT : CAN_ID_OBD_REQUEST_STD;
            uint32_t reply_id = emulate_29_bit ? (CAN_ID_OBD_REPLY_EXT_VAL | 0x10) : CAN_ID_OBD_REPLY_STD_MIN;
            uint8_t reply_extd = emulate_29_bit ? 1 : 0;

            // Check if it's the expected OBD2 Broadcast Request
            if (rx_msg.identifier == expected_req_id && rx_msg.extd == reply_extd && rx_msg.data_length_code >= 3) {
                
                uint8_t mode = rx_msg.data[1];
                if (mode == OBD_MODE_CURRENT_DATA) {
                    uint8_t pid = rx_msg.data[2];
                    
                    // Prepare response frame
                    twai_message_t tx_msg = {0};
                    tx_msg.identifier = reply_id; 
                    tx_msg.extd = reply_extd;
                    tx_msg.rtr = 0;               // Data frame
                    tx_msg.data_length_code = 8;
                    
                    // Fixed header for response:
                    // [Length] [Mode + 0x40] [PID] [A] [B] ...
                    tx_msg.data[1] = mode + 0x40; // 0x41
                    tx_msg.data[2] = pid;
                    
                    bool validRequest = false;

                    switch (pid) {
                        case PID_ENGINE_RPM:
                            // Return dynamic RPM 
                            // Formula: (A*256 + B)/4 = RPM -> A*256 + B = RPM * 4
                            tx_msg.data[0] = 4;
                            tx_msg.data[3] = (((int)(sim_rpm) * 4) >> 8) & 0xFF; // A
                            tx_msg.data[4] = ((int)(sim_rpm) * 4) & 0xFF;        // B
                            validRequest = true;
                            break;

                        case PID_VEHICLE_SPEED:
                            // Return dynamic Speed
                            // Formula: A = Speed
                            tx_msg.data[0] = 3; 
                            tx_msg.data[3] = (int)(sim_speed) & 0xFF; // A
                            validRequest = true;
                            break;

                        case PID_COOLANT_TEMP:
                            // Return dynamic Celsius
                            // Formula: A - 40 = Temp -> A = Temp + 40
                            tx_msg.data[0] = 3;
                            tx_msg.data[3] = ((int)(sim_temp) + 40) & 0xFF; // A
                            validRequest = true;
                            break;

                        case PID_ENGINE_LOAD:
                            // Return dynamic Load
                            // Formula: A * 100 / 255 = Load -> A = Load * 255 / 100
                            tx_msg.data[0] = 3;
                            tx_msg.data[3] = ((int)(sim_load) * 255 / 100) & 0xFF; // A
                            validRequest = true;
                            break;

                        case 0x2F: // PID_FUEL_LEVEL
                            // Return static Fuel Level
                            // Formula: A * 100 / 255 = Fuel -> A = Fuel * 255 / 100
                            tx_msg.data[0] = 3;
                            tx_msg.data[3] = ((int)(sim_fuel) * 255 / 100) & 0xFF; // A
                            validRequest = true;
                            break;

                        case 0x11: // PID_THROTTLE
                            // Return static Throttle Position
                            // Formula: A * 100 / 255 = Throttle -> A = Throttle * 255 / 100
                            tx_msg.data[0] = 3;
                            tx_msg.data[3] = ((int)(sim_throttle) * 255 / 100) & 0xFF; // A
                            validRequest = true;
                            break;

                        default:
                            LOG_WARN("Requested Unknown PID: %02X", pid);
                            break;
                    }

                    // Pad remaining bytes
                    for (int i = tx_msg.data[0] + 1; i < 8; i++) {
                        tx_msg.data[i] = 0xAA; // 0xAA or 0x00 common padding
                    }

                    if (validRequest) {
                        // Send the fake reply onto the bus
                        HalCan::getInstance().writeFrame(tx_msg, 20);
                    }
                }
            }
        }
        
        static uint32_t last_sim_dump = 0;
        // In freeRTOS tasks without main loop delay, use tick count instead of millis
        TickType_t now = xTaskGetTickCount();
        if (now - last_sim_dump > pdMS_TO_TICKS(3000)) {
            HalCan::getInstance().dumpStatus();
            last_sim_dump = now;
        }
    }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(2000); 
    
    // Init M5Unified for Button Support (AtomS3 Lite has central button on screen-less body)
    auto cfg = M5.config();
    M5.begin(cfg);

    LOG_INFO("--- OBD2 ECU SIMULATOR ---");
    LOG_INFO("Hardware: AtomS3 Lite + CAN Unit (CA-IS3050G)");

    // Initialize RGB LED
    pixels.begin();
    pixels.setBrightness(50);
    updateLed(); // Set initial color

    // Initialize CAN HAL with SIMULATOR specific pins (G1, G2)
    // AtomS3 Lite Grove: TX = Yellow (G1), RX = White (G2)
    if (!HalCan::getInstance().begin(SIM_CAN_TX_PIN, SIM_CAN_RX_PIN, CAN_BAUDRATE)) {
        LOG_ERR("CAN hardware failed to initialize.");
        while(1) { delay(100); }
    }

    // Create RTOS Task to act as the ECU Brain
    xTaskCreatePinnedToCore(
        EcuSimulatorTask,
        "ECU_Sim_Task",
        4096,
        NULL,
        5,  // High priority since we must respond quickly
        NULL,
        1
    );

    LOG_INFO("Simulated Engine is RUNNING.");
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
