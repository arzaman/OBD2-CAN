#include <Arduino.h>
#include <M5Unified.h>

#include "../Config.h"
#include "../../lib/HalCan/HalCan.h"
#include "../../lib/Obd2/Obd2Decoder.h" // Just to reuse PID constants

// -----------------------------------------------------------------------------
// ECU SIMULATOR TASK
// -----------------------------------------------------------------------------
void EcuSimulatorTask(void* pvParameters) {
    LOG_INFO("ECU Simulator Listening Task started.");

    static float sim_rpm = 800.0f;
    static float sim_speed = 0.0f;
    static float sim_temp = 90.0f;
    static float sim_load = 20.0f;
    uint32_t last_physics_update = millis();

    while (1) {
        // --- 1. Physics Engine ---
        M5.update();
        uint32_t now_ms = millis();
        float dt = (now_ms - last_physics_update) / 1000.0f;
        last_physics_update = now_ms;

        // Button A = Accelerator. Releasing it = Engine Brake
        if (M5.BtnA.isPressed()) {
            sim_rpm += 2000.0f * dt;      // Accel 2000 RPM/s
            if (sim_rpm > 6500.0f) sim_rpm = 6500.0f;
            
            sim_speed += 40.0f * dt;      // Accel 40 km/h per sec
            if (sim_speed > 180.0f) sim_speed = 180.0f;
            
            sim_load += 200.0f * dt;      // Load shoots up quickly
            if (sim_load > 85.0f) sim_load = 85.0f;
            
            sim_temp += 1.0f * dt;        // Warms up slowly under load
            if (sim_temp > 105.0f) sim_temp = 105.0f;
        } else {
            sim_rpm -= 3000.0f * dt;      // Drops fast when releasing throttle
            if (sim_rpm < 800.0f) sim_rpm = 800.0f;
            
            sim_speed -= 20.0f * dt;      // Coasting / Engine Braking drops speed
            if (sim_speed < 0.0f) sim_speed = 0.0f;
            
            sim_load -= 250.0f * dt;      // Load drops immediately to base
            if (sim_load < 20.0f) sim_load = 20.0f;
            
            sim_temp -= 0.5f * dt;        // Slowly cools down 
            if (sim_temp < 90.0f) sim_temp = 90.0f;
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
            LOG_INFO(">>> DEBUG: RAW FRAME RECEIVED. ID: 0x%03X, DLC: %d", rx_msg.identifier, rx_msg.data_length_code);
            
            // Check if it's an OBD2 Broadcast Request (Scanner asking ECU)
            if (rx_msg.identifier == CAN_ID_OBD_REQUEST && rx_msg.data_length_code >= 3) {
                
                uint8_t mode = rx_msg.data[1];
                if (mode == OBD_MODE_CURRENT_DATA) {
                    uint8_t pid = rx_msg.data[2];
                    
                    // Prepare response frame
                    twai_message_t tx_msg = {0};
                    tx_msg.identifier = CAN_ID_OBD_REPLY_MIN; // 0x7E8 (Standard Engine ECU reply ID)
                    tx_msg.extd = 0;              // Standard 11-bit
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
