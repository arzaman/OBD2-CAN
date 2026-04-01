#include <Arduino.h>
#include <M5Unified.h>

#include "Config.h"
#include "../lib/HalCan/HalCan.h"
#include "../lib/Obd2/Obd2Decoder.h"
#include "../lib/UI/DisplayManager.h"

// -----------------------------------------------------------------------------
// GLOBAL INSTANCES
// -----------------------------------------------------------------------------
Obd2Decoder obdDecoder;

// -----------------------------------------------------------------------------
// RTOS TASKS HANDLES
// -----------------------------------------------------------------------------
TaskHandle_t canTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;

// -----------------------------------------------------------------------------
// CAN POLLING TASK
// -----------------------------------------------------------------------------
// Handles both checking for incoming frames and firing off periodic requests
void CanPollTask(void* pvParameters) {
    TickType_t xLastWakeTimeRequest = xTaskGetTickCount();
    LOG_INFO("CAN Polling task started.");

    while (1) {
        // --- 0. Hardware Level CAN Auto-Recovery ---
        // If bus is off or something failed, try to recover
        if (HalCan::getInstance().getState() == TWAI_STATE_BUS_OFF) {
            HalCan::getInstance().recover();
            vTaskDelay(pdMS_TO_TICKS(100)); // Give it time to recover
            continue;
        }

        // --- 1. Processing incoming frames ---
        twai_message_t rx_msg;
        // Non-blocking (or very short block) read
        if (HalCan::getInstance().readFrame(rx_msg, 0)) {
            // Forward to decoder layer
            obdDecoder.processRxFrame(rx_msg);
        }

        // --- 1.5 Debug logging state ---
        static bool last_conn = !obdDecoder.isConnected(); // Force first trigger
        if (obdDecoder.isConnected() != last_conn) {
            last_conn = obdDecoder.isConnected();
            LOG_INFO("Backend Connection State is now: %s", last_conn ? "TRUE (Connected)" : "FALSE (Disconnected)");
        }

        // --- 2. Periodic Request generation ---
        // Determine interval: slower if disconnected, faster if connected
        uint32_t polling_interval = obdDecoder.isConnected() ? OBD2_REQUEST_INTERVAL_MS : (OBD2_REQUEST_INTERVAL_MS * 4);
        
        // Using RTOS tick differences to send every interval
        TickType_t xNow = xTaskGetTickCount();
        if ((xNow - xLastWakeTimeRequest) >= pdMS_TO_TICKS(polling_interval)) {
            twai_message_t txReq = obdDecoder.generateTxRequest();
            if (!HalCan::getInstance().writeFrame(txReq, 10)) {
                LOG_WARN("Scanner TX Failed! Buffer full or bus error?");
            }
            xLastWakeTimeRequest = xNow;
        }

        static TickType_t xLastDump = 0;
        if ((xNow - xLastDump) >= pdMS_TO_TICKS(3000)) {
            HalCan::getInstance().dumpStatus();
            xLastDump = xNow;
        }
        
        // Yield to let other tasks run, sleep a tiny bit to not starve the idle task
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// -----------------------------------------------------------------------------
// UI AND INPUT HANDLING TASK
// -----------------------------------------------------------------------------
void UIUpdateTask(void* pvParameters) {
    LOG_INFO("UI Update task started.");
    
    // UI Refresh Loop
    while (1) {
        // M5 Unified library update handles reading buttons state etc.
        M5.update();
        
        // Handle Button press
        if (M5.BtnA.wasPressed()) { // Central button of AtomS3
            LOG_INFO("Button A Pressed. Changing view...");
            DisplayManager::getInstance().nextMode();
        }

        // Refresh UI with latest Decoded OBD2 Data and connection state
        DisplayManager::getInstance().update(obdDecoder.getData(), obdDecoder.isConnected());
        
        // 50ms UI loop typically fast enough for responsive buttons
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    // Give serial time to attach (USB CDC)
    delay(2000); 
    
    LOG_INFO("--- OBD2 OBD-II CAN Scanner ---");
    LOG_INFO("Starting System Initialization...");

    // Init UI 
    DisplayManager::getInstance().begin();

    // Init CAN HAL
    if (!HalCan::getInstance().begin()) {
        LOG_ERR("Halted execution: CAN hardware failed to initialize.");
        // We could print an error on display here
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.drawString("CAN INIT FAIL", M5.Display.width()/2, M5.Display.height()/2);
        while(1) { vTaskDelay(100); }
    }

    // Create RTOS Tasks
    // CAN usually gets high priority to avoid missing fast frames
    xTaskCreatePinnedToCore(
        CanPollTask,        // Function to implement the task
        "CAN_Poll_Task",    // Name of the task
        4096,               // Stack size in words
        NULL,               // Task input parameter
        5,                  // Priority of the task (0-24, higher is better)
        &canTaskHandle,     // Core where the task should run
        1                   // Use Core 1 (App Core) or 0 (Pro Core)
    );

    // UI/Input task
    xTaskCreatePinnedToCore(
        UIUpdateTask,
        "UI_Update_Task",
        4096,
        NULL,
        2,                  // Lower priority than CAN
        &uiTaskHandle,
        1
    );

    LOG_INFO("System Initialized Successfully.");
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
    // In ESP-IDF/Arduino with RTOS, the loop() is just another task (priority 1 by default).
    // We let our specific FreeRTOS tasks to do the heavy lifting.
    // So here we can just delete it or add a very slow heartbeat just for sanity.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
