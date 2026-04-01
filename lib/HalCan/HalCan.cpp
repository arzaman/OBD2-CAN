#include "HalCan.h"

bool HalCan::begin(gpio_num_t tx_pin, gpio_num_t rx_pin, const twai_timing_config_t& timing) {
    if (_initialized) {
        LOG_WARN("CAN Interface already initialized.");
        return true;
    }

    LOG_INFO("Initializing TWAI driver...");
    
    // General Config: Set TX, RX, operating mode (Normal)
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL);
    
    // Timing Config: Provided as parameter (default 500k)
    twai_timing_config_t t_config = timing;
    
    // Filter Config: Accept all frames
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        LOG_INFO("Driver installed");
    } else {
        LOG_ERR("Failed to install driver");
        return false;
    }

    // Start driver
    if (twai_start() == ESP_OK) {
        LOG_INFO("Driver started");
        _initialized = true;
        return true;
    } else {
        LOG_ERR("Failed to start driver");
        twai_driver_uninstall();
        return false;
    }
}

void HalCan::end() {
    if (!_initialized) return;

    if (twai_stop() == ESP_OK) {
        LOG_INFO("Driver stopped");
    } else {
        LOG_ERR("Failed to stop driver");
    }

    if (twai_driver_uninstall() == ESP_OK) {
        LOG_INFO("Driver uninstalled");
    } else {
        LOG_ERR("Failed to uninstall driver");
    }
    
    _initialized = false;
}

bool HalCan::readFrame(twai_message_t& rx_msg, uint32_t timeout_ms) {
    if (!_initialized) return false;

    // Wait until timeout or message received
    esp_err_t result = twai_receive(&rx_msg, pdMS_TO_TICKS(timeout_ms));
    return (result == ESP_OK);
}

bool HalCan::writeFrame(const twai_message_t& tx_msg, uint32_t timeout_ms) {
    if (!_initialized) return false;

    // Transmit message with timeout
    esp_err_t result = twai_transmit(&tx_msg, pdMS_TO_TICKS(timeout_ms));
    return (result == ESP_OK);
}

twai_state_t HalCan::getState() {
    if (!_initialized) return TWAI_STATE_STOPPED;
    twai_status_info_t status_info;
    if (twai_get_status_info(&status_info) == ESP_OK) {
        return status_info.state;
    }
    return TWAI_STATE_STOPPED;
}

void HalCan::dumpStatus() {
    if (!_initialized) return;
    twai_status_info_t si;
    if (twai_get_status_info(&si) == ESP_OK) {
        LOG_INFO("TWAI DIAGNOSTICS: State: %d, TX Errors: %d, RX Errors: %d, TX Q: %d, RX Q: %d", 
                  si.state, si.tx_error_counter, si.rx_error_counter, si.msgs_to_tx, si.msgs_to_rx);
    }
}

bool HalCan::recover() {
    if (!_initialized) return false;
    
    twai_state_t state = getState();
    if (state == TWAI_STATE_BUS_OFF) {
        LOG_WARN("CAN is BUS_OFF. Initiating recovery...");
        twai_initiate_recovery();
        // Recovery takes time, realistically we may just wait or let next cycle check
        return true;
    } else if (state == TWAI_STATE_STOPPED || state == TWAI_STATE_RECOVERING) {
        // If it was recovering and now stopped, start it back up
        if (state == TWAI_STATE_STOPPED) {
            if (twai_start() == ESP_OK) {
                LOG_INFO("Recovery complete, CAN restarted.");
                return true;
            }
        }
    }
    return false;
}
