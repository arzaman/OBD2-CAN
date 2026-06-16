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
// DIAGNOSTIC COUNTERS (for verbose logging)
// -----------------------------------------------------------------------------
static volatile uint32_t diag_rx_count = 0;
static volatile uint32_t diag_tx_ok_count = 0;
static volatile uint32_t diag_tx_fail_count = 0;
static volatile uint32_t diag_bus_off_count = 0;

// -----------------------------------------------------------------------------
// CAN POLLING TASK
// -----------------------------------------------------------------------------
void CanPollTask(void* pvParameters) {
    TickType_t xLastWakeTimeRequest = xTaskGetTickCount();
    LOG_INFO("CAN Polling task started.");

#if DIAG_LISTEN_ONLY
    LOG_INFO(">>> DIAGNOSTIC MODE: LISTEN_ONLY — No TX, passive sniffing <<<");
#elif DIAG_NO_ACK
    LOG_INFO(">>> DIAGNOSTIC MODE: NO_ACK — TX without requiring bus ACK <<<");
#else
    LOG_INFO(">>> OPERATING MODE: NORMAL — Full CAN participation <<<");
#endif

    while (1) {
        // --- 0. Hardware Level CAN Auto-Recovery ---
        twai_state_t can_state = HalCan::getInstance().getState();

#if DIAG_LISTEN_ONLY
        // In LISTEN_ONLY mode, the controller should never go BUS_OFF.
        // But we still log if something unexpected happens.
        if (can_state != TWAI_STATE_RUNNING) {
            LOG_WARN("LISTEN_ONLY: Unexpected state %d (should always be RUNNING)", can_state);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
#else
        // NORMAL or NO_ACK mode: handle recovery
        if (can_state != TWAI_STATE_RUNNING) {
            if (can_state == TWAI_STATE_BUS_OFF) {
                diag_bus_off_count++;
                LOG_WARN("BUS_OFF detected! (count: %u) Initiating recovery...", diag_bus_off_count);
            }
            HalCan::getInstance().recover();
            // After a BUS_OFF recovery, wait longer before retrying to let the bus stabilize
            vTaskDelay(pdMS_TO_TICKS(can_state == TWAI_STATE_BUS_OFF ? 500 : 50));
            continue;
        }
#endif

        // --- 1. Processing incoming frames ---
        twai_message_t rx_msg;
        if (HalCan::getInstance().readFrame(rx_msg, 0)) {
            diag_rx_count++;

#if DIAG_VERBOSE
            // Log ALL received frames with full raw data
            char data_str[32] = {0};
            int offset = 0;
            for (int i = 0; i < rx_msg.data_length_code && i < 8; i++) {
                offset += snprintf(data_str + offset, sizeof(data_str) - offset, "%02X ", rx_msg.data[i]);
            }
            LOG_INFO("RX #%u | ID: 0x%03X | DLC: %d | Data: %s| Ext: %d | RTR: %d",
                     diag_rx_count, rx_msg.identifier, rx_msg.data_length_code, 
                     data_str, rx_msg.extd, rx_msg.rtr);
#endif

#if !DIAG_LISTEN_ONLY
            // Forward to OBD2 decoder only if we're in active mode
            obdDecoder.processRxFrame(rx_msg);
#endif
        }

        // --- 1.5 Debug logging state ---
#if !DIAG_LISTEN_ONLY
        static bool last_conn = !obdDecoder.isConnected();
        if (obdDecoder.isConnected() != last_conn) {
            last_conn = obdDecoder.isConnected();
            LOG_INFO("OBD2 Connection State: %s", last_conn ? "CONNECTED" : "DISCONNECTED");
        }
#endif

        // --- 2. Periodic Request generation (only in active modes) ---
#if !DIAG_LISTEN_ONLY
        uint32_t polling_interval = obdDecoder.isConnected() ? OBD2_REQUEST_INTERVAL_MS : (OBD2_REQUEST_INTERVAL_MS * 4);
        
        TickType_t xNow = xTaskGetTickCount();
        if ((xNow - xLastWakeTimeRequest) >= pdMS_TO_TICKS(polling_interval)) {
            twai_message_t txReq = obdDecoder.generateTxRequest();
            
            // TX Retry loop: try up to 10 times before giving up
            bool tx_success = false;
            esp_err_t tx_result = ESP_FAIL;
            for (int retry = 0; retry < 10; retry++) {
                tx_result = twai_transmit(&txReq, pdMS_TO_TICKS(50));
                if (tx_result == ESP_OK) {
                    tx_success = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(5)); // Small pause between retries
            }
            
            if (tx_success) {
                diag_tx_ok_count++;
#if DIAG_VERBOSE
                LOG_INFO("TX OK #%u | ID: 0x%03X | PID: 0x%02X", 
                         diag_tx_ok_count, txReq.identifier, txReq.data[2]);
#endif
            } else {
                diag_tx_fail_count++;
                const char* err_str = (tx_result == ESP_ERR_TIMEOUT)      ? "TIMEOUT (bus busy or no ACK)" :
                                      (tx_result == ESP_ERR_INVALID_STATE) ? "INVALID_STATE (driver not running)" :
                                      (tx_result == ESP_ERR_NOT_SUPPORTED) ? "NOT_SUPPORTED (listen-only mode)" :
                                      (tx_result == ESP_FAIL)              ? "FAIL (TX queue full)" :
                                                                             "UNKNOWN";
                LOG_WARN("TX FAIL #%u (after 10 retries) | Error: %s (0x%x) | ID: 0x%03X", 
                         diag_tx_fail_count, err_str, tx_result, txReq.identifier);
            }
            xLastWakeTimeRequest = xNow;
        }
#endif

        // --- 3. Periodic Status Dump ---
        static TickType_t xLastDump = 0;
        TickType_t xNowDump = xTaskGetTickCount();
        if ((xNowDump - xLastDump) >= pdMS_TO_TICKS(3000)) {
            HalCan::getInstance().dumpStatus();
            LOG_INFO("STATS: RX=%u TX_OK=%u TX_FAIL=%u BUS_OFF=%u",
                     diag_rx_count, diag_tx_ok_count, diag_tx_fail_count, diag_bus_off_count);
            xLastDump = xNowDump;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// -----------------------------------------------------------------------------
// UI AND INPUT HANDLING TASK
// -----------------------------------------------------------------------------
void UIUpdateTask(void* pvParameters) {
    LOG_INFO("UI Update task started.");
    
    while (1) {
        M5.update();
        
        if (M5.BtnA.wasPressed()) {
            LOG_INFO("Button A Pressed. Changing view...");
            DisplayManager::getInstance().nextMode();
        }

        DisplayManager::getInstance().update(obdDecoder.getData(), obdDecoder.isConnected());
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(2000); 
    
    LOG_INFO("--- OBD2 CAN Scanner ---");

#if DIAG_LISTEN_ONLY
    LOG_INFO("=== DIAGNOSTIC STEP 1: LISTEN_ONLY MODE ===");
    LOG_INFO("The scanner will ONLY listen to CAN bus traffic.");
    LOG_INFO("If you see RX frames -> hardware is working!");
    LOG_INFO("HW Filter: ACCEPT_ALL (sniffing all traffic)");
    twai_mode_t can_mode = TWAI_MODE_LISTEN_ONLY;
    twai_filter_config_t can_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
#elif DIAG_NO_ACK
    LOG_INFO("=== DIAGNOSTIC STEP 2: NO_ACK (self-test) MODE ===");
    LOG_INFO("TX will not require ACK from bus nodes.");
    LOG_INFO("HW Filter: OBD2 responses only (0x7E8-0x7EF)");
    twai_mode_t can_mode = TWAI_MODE_NO_ACK;
    // HW filter: accept only OBD2 response IDs 0x7E8-0x7EF (standard 11-bit)
    // Mask 0x7F8 means bits 0-2 are don't-care → matches 0x7E8 through 0x7EF
    twai_filter_config_t can_filter = {
        .acceptance_code = (uint32_t)(0x7E8UL << 21),  // Standard ID in bits [31:21]
        .acceptance_mask = ~(uint32_t)(0x7F8UL << 21),  // Match upper 8 bits of ID
        .single_filter = true
    };
#else
    LOG_INFO("=== NORMAL OPERATING MODE ===");
    LOG_INFO("HW Filter: ACCEPT_ALL (software filtering for OBD2)");
    twai_mode_t can_mode = TWAI_MODE_NORMAL;
    // We use ACCEPT_ALL to allow both 11-bit and 29-bit auto-discovery.
    // Filtering is handled in Obd2Decoder::processRxFrame().
    twai_filter_config_t can_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
#endif

    LOG_INFO("Starting System Initialization...");

    // Init UI 
    DisplayManager::getInstance().begin();

    // Init CAN HAL with selected mode and filter
    if (!HalCan::getInstance().begin(CAN_TX_PIN, CAN_RX_PIN, CAN_BAUDRATE, can_mode, can_filter)) {
        LOG_ERR("Halted execution: CAN hardware failed to initialize.");
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.drawString("CAN INIT FAIL", M5.Display.width()/2, M5.Display.height()/2);
        while(1) { vTaskDelay(100); }
    }

    // Create RTOS Tasks
    xTaskCreatePinnedToCore(
        CanPollTask,
        "CAN_Poll_Task",
        4096,
        NULL,
        5,
        &canTaskHandle,
        1
    );

    xTaskCreatePinnedToCore(
        UIUpdateTask,
        "UI_Update_Task",
        4096,
        NULL,
        2,
        &uiTaskHandle,
        1
    );

    LOG_INFO("System Initialized Successfully.");
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
