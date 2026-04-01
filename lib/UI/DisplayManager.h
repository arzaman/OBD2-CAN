#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "../../src/Config.h"
#include "../Obd2/Obd2Decoder.h"

enum class DisplayMode {
    SHOW_RPM,
    SHOW_SPEED,
    SHOW_GRID  // Multi-data view
};

class DisplayManager {
public:
    static DisplayManager& getInstance() {
        static DisplayManager instance;
        return instance;
    }

    void begin();
    
    // update now takes boolean for connection status
    void update(const ObdData& data, bool isConnected);
    
    // Cycle modes via button
    void nextMode();

private:
    DisplayManager() : _mode(DisplayMode::SHOW_RPM), _lastRpm(-1), _lastSpeed(-1) {}
    ~DisplayManager() = default;
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    DisplayMode _mode;
    bool _wasConnected;
    
    // Cache last values to avoid flicker redraws
    int _lastRpm;
    int _lastSpeed;
    int _lastTemp;
    int _lastLoad;

    void drawConnectionLayer(bool isConnected);
    void drawRpm(int rpm, bool forceFullClear);
    void drawSpeed(int speed, bool forceFullClear);
    void drawGrid(const ObdData& data, bool forceFullClear);
};
