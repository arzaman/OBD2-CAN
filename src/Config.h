#pragma once

#include <Arduino.h>
#include "driver/twai.h"

// -----------------------------------------------------------------------------
// DEBUG & LOGGING
// -----------------------------------------------------------------------------
#define DEBUG_MODE 1

#if DEBUG_MODE
    #define LOG_INFO(fmt, ...)  do { if (Serial) { Serial.printf("[INFO][%lu] " fmt "\n", millis(), ##__VA_ARGS__); } } while(0)
    #define LOG_WARN(fmt, ...)  do { if (Serial) { Serial.printf("[WARN][%lu] " fmt "\n", millis(), ##__VA_ARGS__); } } while(0)
    #define LOG_ERR(fmt, ...)   do { if (Serial) { Serial.printf("[ERR][%lu] " fmt "\n", millis(), ##__VA_ARGS__); } } while(0)
#else
    #define LOG_INFO(fmt, ...)
    #define LOG_WARN(fmt, ...)
    #define LOG_ERR(fmt, ...)
#endif

// -----------------------------------------------------------------------------
// HARDWARE / PINS CONFIGURATION
// -----------------------------------------------------------------------------
// PIN setup for M5Stack AtomS3 with Atomic CAN Base
// M5Stack Official Docs: AtomS3 CAN Base TX is G6, RX is G5
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_6


// PIN setup for M5Stack AtomS3 Lite acting as ECU SIMULATOR 
// (Using "CAN Unit" on Grove HY2.0-4P interface)
// Derived from M5Stack images: UART_TX (White) -> G1, UART_RX (Yellow) -> G2
#define SIM_CAN_TX_PIN GPIO_NUM_2 // yellow
#define SIM_CAN_RX_PIN GPIO_NUM_1 // white

// -----------------------------------------------------------------------------
// DIAGNOSTIC MODE FLAGS (set to 1 to enable, 0 to disable)
// -----------------------------------------------------------------------------
// Step 1: LISTEN_ONLY - Passive sniffing, NO transmission, NO ACK.
//         Use this to verify the transceiver can receive CAN traffic from the car.
//         If you see frames → hardware is OK. If not → HW/cable problem.
#define DIAG_LISTEN_ONLY  0

// Step 2: NO_ACK mode - Transmit frames without requiring ACK from other nodes.
//         Use this if Step 1 passes but NORMAL mode still fails (ACK errors).
//         Only used when DIAG_LISTEN_ONLY is 0.
#define DIAG_NO_ACK       0

// Verbose diagnostics: log every RX frame raw data + detailed TX error codes
#define DIAG_VERBOSE      1

// -----------------------------------------------------------------------------
// CAN BUS CONFIGURATION
// -----------------------------------------------------------------------------
// OBD-II typical baud rates: 500Kbps (most common) or 250Kbps.
// Configured here to easily change it in HAL initialization.
#define CAN_BAUDRATE TWAI_TIMING_CONFIG_500KBITS()

// Polling interval per CAN reading in ms
#define CAN_POLL_INTERVAL_MS 50 

// -----------------------------------------------------------------------------
// OBD2 CONFIGURATION
// -----------------------------------------------------------------------------
// OBD2 Request interval in ms (how often we request data like RPM, Speed)
#define OBD2_REQUEST_INTERVAL_MS 250
