#pragma once

#include <Arduino.h>
#include "driver/twai.h"

// -----------------------------------------------------------------------------
// DEBUG & LOGGING
// -----------------------------------------------------------------------------
#define DEBUG_MODE 1

#if DEBUG_MODE
    #define LOG_INFO(fmt, ...)  Serial.printf("[INFO][%lu] " fmt "\n", millis(), ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)  Serial.printf("[WARN][%lu] " fmt "\n", millis(), ##__VA_ARGS__)
    #define LOG_ERR(fmt, ...)   Serial.printf("[ERR][%lu] " fmt "\n", millis(), ##__VA_ARGS__)
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
