#include "DisplayManager.h"

void DisplayManager::begin() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(0); // 0 = Upright, USB connector usually at the bottom
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM); // Middle center
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Set to true so on first update(false) it instantly triggers a redraw of the RED dot
    _wasConnected = true; 
    _lastRpm = -1;
    _lastSpeed = -1;
    _lastTemp = -1;
    _lastLoad = -1;
}

void DisplayManager::nextMode() {
    if (_mode == DisplayMode::SHOW_RPM) {
        _mode = DisplayMode::SHOW_SPEED;
    } else if (_mode == DisplayMode::SHOW_SPEED) {
        _mode = DisplayMode::SHOW_GRID;
    } else {
        _mode = DisplayMode::SHOW_RPM;
    }
    
    // Clear screen and force full redraw
    M5.Display.fillScreen(TFT_BLACK);
    _lastRpm = -1;
    _lastSpeed = -1;
    _lastTemp = -1;
    _lastLoad = -1;
    
    // Force draw connection status immediately
    drawConnectionLayer(_wasConnected);
}

void DisplayManager::drawConnectionLayer(bool isConnected) {
    // Top right corner small filled circle
    int radius = 4;
    int x = M5.Display.width() - radius - 2;
    int y = radius + 2;
    // Use explicit RGB encoding to avoid macro swap issues across TFT screens
    uint16_t red_col = M5.Display.color565(255, 0, 0);
    uint16_t green_col = M5.Display.color565(0, 255, 0);
    uint16_t color = isConnected ? green_col : red_col;
    M5.Display.fillCircle(x, y, radius, color);
}

void DisplayManager::update(const ObdData& data, bool isConnected) {
    bool force = false;
    
    // Check if connection status changed
    if (isConnected != _wasConnected) {
        _wasConnected = isConnected;
        drawConnectionLayer(isConnected);
    } // Always draw if we just cleared the screen but that is handled in nextMode

    // Draw main content based on mode
    switch (_mode) {
        case DisplayMode::SHOW_RPM:
            if (data.rpm != _lastRpm || force) {
                drawRpm(data.rpm, force);
                _lastRpm = data.rpm;
            }
            break;
            
        case DisplayMode::SHOW_SPEED:
            if (data.speed != _lastSpeed || force) {
                drawSpeed(data.speed, force);
                _lastSpeed = data.speed;
            }
            break;
            
        case DisplayMode::SHOW_GRID:
            if (data.rpm != _lastRpm || data.speed != _lastSpeed || 
                data.coolantTemp != _lastTemp || data.engineLoad != _lastLoad || force) {
                drawGrid(data, force);
                _lastRpm = data.rpm;
                _lastSpeed = data.speed;
                _lastTemp = data.coolantTemp;
                _lastLoad = data.engineLoad;
            }
            break;

        default:
            break;
    }
}

void DisplayManager::drawRpm(int rpm, bool forceFullClear) {
    if (forceFullClear) M5.Display.fillScreen(TFT_BLACK);
    
    // Small title
    M5.Display.setTextFont(2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(0,255,0), TFT_BLACK);
    M5.Display.drawString("RPM Engine", M5.Display.width()/2, 20);
    
    // Big Value
    M5.Display.setTextFont(4); 
    M5.Display.setTextSize(2); 
    M5.Display.setTextColor(M5.Display.color565(255,255,255), TFT_BLACK);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d", rpm);
    M5.Display.drawString(buf, M5.Display.width()/2, M5.Display.height()/2 + 10);
}

void DisplayManager::drawSpeed(int speed, bool forceFullClear) {
    if (forceFullClear) M5.Display.fillScreen(TFT_BLACK);
    
    // Small title
    M5.Display.setTextFont(2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(0,255,255), TFT_BLACK);
    M5.Display.drawString("Speed (km/h)", M5.Display.width()/2, 20);
    
    // Big Value
    M5.Display.setTextFont(4);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(M5.Display.color565(255,255,255), TFT_BLACK);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%03d", speed);
    M5.Display.drawString(buf, M5.Display.width()/2, M5.Display.height()/2 + 10);
}

void DisplayManager::drawGrid(const ObdData& data, bool forceFullClear) {
    int w = M5.Display.width();
    int h = M5.Display.height();
    int qW = w / 2;
    int qH = h / 2;

    if (forceFullClear) {
        M5.Display.fillScreen(TFT_BLACK);
        // Draw crosshair separators
        M5.Display.drawLine(qW, 6, qW, h - 6, TFT_DARKGREY);
        M5.Display.drawLine(6, qH, w - 6, qH, TFT_DARKGREY);
    }
    
    char buf[16];

    // --- Top-Left: RPM (Green) ---
    // Label
    M5.Display.setTextFont(1);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(0,180,0), TFT_BLACK);
    M5.Display.drawString("RPM", qW/2, qH/2 - 14);
    // Value (larger font, fixed 4 digits)
    M5.Display.setTextFont(2);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(M5.Display.color565(0,255,0), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%4d", data.rpm);
    M5.Display.drawString(buf, qW/2, qH/2 + 8);

    // --- Top-Right: Speed (Cyan) ---
    M5.Display.setTextFont(1);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(0,180,180), TFT_BLACK);
    M5.Display.drawString("km/h", qW + qW/2, qH/2 - 14);
    M5.Display.setTextFont(2);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(M5.Display.color565(0,255,255), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%3d", data.speed);
    M5.Display.drawString(buf, qW + qW/2, qH/2 + 8);

    // --- Bottom-Left: Temp (Orange-Red) ---
    M5.Display.setTextFont(1);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(180,80,0), TFT_BLACK);
    M5.Display.drawString("TEMP C", qW/2, qH + qH/2 - 14);
    M5.Display.setTextFont(2);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(M5.Display.color565(255,120,0), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%2d", data.coolantTemp);
    M5.Display.drawString(buf, qW/2, qH + qH/2 + 8);

    // --- Bottom-Right: Load (Yellow) ---
    M5.Display.setTextFont(1);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(180,180,0), TFT_BLACK);
    M5.Display.drawString("LOAD %", qW + qW/2, qH + qH/2 - 14);
    M5.Display.setTextFont(2);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(M5.Display.color565(255,255,0), TFT_BLACK);
    snprintf(buf, sizeof(buf), "%2d", data.engineLoad);
    M5.Display.drawString(buf, qW + qW/2, qH + qH/2 + 8);
}
