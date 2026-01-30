// g++ -o ColorTracker.exe aim_assist.cpp -lgdi32
// Build: g++ -O2 -o ColorTracker.exe aim_assist.cpp -lgdi32

#include <windows.h>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

// ---------- Config Structure ----------
struct Config {
    int searchRadius = 75;
    int tolerance = 15;
    int loopSleepMs = 1;
    int toggleKey = 'E'; // Default 'E'
    int modeKey = VK_F4; // Default F4
    int enableKey = VK_F5; // Default F5
};

Config currentConfig;
FILETIME lastConfigTime = {0, 0};
const std::string CONFIG_FILE = "config.json";

// Helper to parse simple JSON int values
int parseJsonInt(const std::string& content, const std::string& key, int defaultVal) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return defaultVal;
    
    size_t colon = content.find(":", pos);
    if (colon == std::string::npos) return defaultVal;
    
    size_t valStart = content.find_first_of("0123456789", colon);
    if (valStart == std::string::npos) return defaultVal;
    
    size_t valEnd = content.find_first_not_of("0123456789", valStart);
    std::string valStr = content.substr(valStart, valEnd - valStart);
    
    try {
        return std::stoi(valStr);
    } catch (...) {
        return defaultVal;
    }
}

// Helper to parse simple JSON string keys to VK codes (Simplified)
int parseJsonKey(const std::string& content, const std::string& key, int defaultVal) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return defaultVal;
    
    size_t colon = content.find(":", pos);
    if (colon == std::string::npos) return defaultVal;

    size_t quoteStart = content.find("\"", colon);
    if (quoteStart == std::string::npos) return defaultVal;
    
    size_t quoteEnd = content.find("\"", quoteStart + 1);
    std::string valStr = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    
    if (valStr.empty()) return defaultVal;
    
    // Convert to upper case for comparison
    std::transform(valStr.begin(), valStr.end(), valStr.begin(), ::toupper);

    // Function Keys
    if (valStr == "F1") return VK_F1;
    if (valStr == "F2") return VK_F2;
    if (valStr == "F3") return VK_F3;
    if (valStr == "F4") return VK_F4;
    if (valStr == "F5") return VK_F5;
    if (valStr == "F6") return VK_F6;
    if (valStr == "F7") return VK_F7;
    if (valStr == "F8") return VK_F8;
    if (valStr == "F9") return VK_F9;
    if (valStr == "F10") return VK_F10;
    if (valStr == "F11") return VK_F11;
    if (valStr == "F12") return VK_F12;

    // Common Keys
    if (valStr == "SPACE") return VK_SPACE;
    if (valStr == "SHIFT") return VK_SHIFT;
    if (valStr == "CTRL")  return VK_CONTROL;
    if (valStr == "ALT")   return VK_MENU;
    if (valStr == "TAB")   return VK_TAB;
    if (valStr == "ESC")   return VK_ESCAPE;
    if (valStr == "ENTER") return VK_RETURN;
    
    // Single Character (A-Z, 0-9)
    if (valStr.length() == 1) return valStr[0];
    
    return defaultVal;
}

bool loadConfig() {
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(CONFIG_FILE.c_str(), GetFileExInfoStandard, &attr)) return false;

    // Check modification time
    // For robust polling, we trust the file time, but we also ensure we don't spam reads if time is same.
    if (CompareFileTime(&attr.ftLastWriteTime, &lastConfigTime) == 0) return false;
    lastConfigTime = attr.ftLastWriteTime;

    std::ifstream f(CONFIG_FILE);
    if (!f.is_open()) return false;
    
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();

    Config newConfig;
    newConfig.searchRadius = parseJsonInt(content, "searchRadius", 50);
    newConfig.tolerance = parseJsonInt(content, "tolerance", 24);
    newConfig.loopSleepMs = parseJsonInt(content, "loopSleepMs", 1);
    newConfig.toggleKey = parseJsonKey(content, "toggleKey", 'E');
    newConfig.modeKey = parseJsonKey(content, "modeKey", VK_F4);
    newConfig.enableKey = parseJsonKey(content, "enableKey", VK_F5);

    bool changed = false;
    if (newConfig.searchRadius != currentConfig.searchRadius) changed = true;
    if (newConfig.tolerance != currentConfig.tolerance) changed = true;
    if (newConfig.loopSleepMs != currentConfig.loopSleepMs) changed = true;
    if (newConfig.toggleKey != currentConfig.toggleKey) changed = true;
    if (newConfig.modeKey != currentConfig.modeKey) changed = true;
    if (newConfig.enableKey != currentConfig.enableKey) changed = true;

    currentConfig = newConfig;

    if (changed) {
        std::cout << "STATE:CONFIG_LOADED" << std::endl;
        return true;
    }
    return false;
}

static inline bool similarRGB(int r1, int g1, int b1, int r2, int g2, int b2, int tol) {
    return (abs(r1 - r2) <= tol) && (abs(g1 - g2) <= tol) && (abs(b1 - b2) <= tol);
}

int main() {
    SetProcessDPIAware();
    loadConfig(); 

    // Standard Output needs to be unbuffered for Node.js to read immediately
    std::cout.setf(std::ios::unitbuf);

    bool isEnabled = false; // Global enable/disable state (tracking on/off)
    bool haveColor = false;
    int tgtR = 0, tgtG = 0, tgtB = 0;

    // Report Initial State
    std::cout << "STATE:READY" << std::endl;
    std::cout << "STATE:ENABLED:OFF" << std::endl;

    // Screen + memory DC
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    // Initial DIB setup
    int currentFullW = currentConfig.searchRadius * 2 + 1;
    int currentFullH = currentConfig.searchRadius * 2 + 1;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = currentFullW;
    bmi.bmiHeader.biHeight = -currentFullH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pPixels = nullptr;
    HBITMAP hbm = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pPixels, NULL, 0);
    HGDIOBJ oldObj = SelectObject(hdcMem, hbm);

    // Frame counter for polling config
    int frameCount = 0;

    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    while (true) {
        // Poll config every 30 frames
        frameCount++;
        if (frameCount > 30) {
            frameCount = 0;
            if (loadConfig()) {
                // Config changed
                int newFullW = currentConfig.searchRadius * 2 + 1;
                if (newFullW != currentFullW) {
                    SelectObject(hdcMem, oldObj);
                    DeleteObject(hbm);
                    currentFullW = newFullW;
                    currentFullH = newFullW; 
                    bmi.bmiHeader.biWidth = currentFullW;
                    bmi.bmiHeader.biHeight = -currentFullH;
                    hbm = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pPixels, NULL, 0);
                    if (!hbm || !pPixels) return 1;
                    oldObj = SelectObject(hdcMem, hbm);
                    std::cout << "STATE:DIB_RESIZED" << std::endl;
                }
            }
        }

        // Check if our GUI is focused
        // We look for a window with title "ColorTracker Pro" (from index.html title)
        HWND foreground = GetForegroundWindow();
        char windowTitle[256];
        if (GetWindowTextA(foreground, windowTitle, sizeof(windowTitle))) {
            if (std::string(windowTitle) == "ColorTracker Pro") {
                // GUI is focused, pause logic to prevent accidental toggles while typing/binding
                // IMPORTANT: We must call GetAsyncKeyState to "consume" the 'pressed since last call' flag
                // otherwise it might trigger immediately when we focus back.
                if (currentConfig.toggleKey > 0) GetAsyncKeyState(currentConfig.toggleKey);
                if (currentConfig.modeKey > 0)   GetAsyncKeyState(currentConfig.modeKey);
                if (currentConfig.enableKey > 0) GetAsyncKeyState(currentConfig.enableKey);
                GetAsyncKeyState(VK_LBUTTON);
                
                Sleep(50); 
                continue;
            }
        }

        // Enable/Disable Tracking (enable key)
        if (GetAsyncKeyState(currentConfig.enableKey) & 1) {
            isEnabled = !isEnabled;
            std::cout << "STATE:ENABLED:" << (isEnabled ? "ON" : "OFF") << std::endl;
        }

        // Set Color to Track (toggle key) - captures current cursor pixel color
        if (GetAsyncKeyState(currentConfig.toggleKey) & 1) {
            POINT cur;
            GetCursorPos(&cur);
            COLORREF c = GetPixel(hdcScreen, cur.x, cur.y);
            tgtR = GetRValue(c);
            tgtG = GetGValue(c);
            tgtB = GetBValue(c);
            haveColor = true;
            std::cout << "STATE:COLOR:" << tgtR << "," << tgtG << "," << tgtB << std::endl;
        }

        // Track if enabled and we have a color
        bool shouldTrack = isEnabled && haveColor;
        
        if (shouldTrack) {
            POINT cur;
            GetCursorPos(&cur);

            {
                COLORREF c = GetPixel(hdcScreen, cur.x, cur.y);
                if (similarRGB(GetRValue(c), GetGValue(c), GetBValue(c), tgtR, tgtG, tgtB, currentConfig.tolerance)) {
                    Sleep(currentConfig.loopSleepMs);
                    continue;
                }
            }

            int r = currentConfig.searchRadius;
            int left   = std::max((int)cur.x - r, vx);
            int top    = std::max((int)cur.y - r, vy);
            int right  = std::min((int)cur.x + r, vx + vw - 1);
            int bottom = std::min((int)cur.y + r, vy + vh - 1);

            if (left > right || top > bottom) {
                Sleep(currentConfig.loopSleepMs);
                continue;
            }

            int capW = right - left + 1;
            int capH = bottom - top + 1;

            // DIB is now dynamically resized, so no need for safety check against fixed size
            // unless creation failed, which we handle above.

            if (!BitBlt(hdcMem, 0, 0, capW, capH, hdcScreen, left, top, SRCCOPY)) {
                Sleep(currentConfig.loopSleepMs);
                continue;
            }

            const int cx = cur.x - left;
            const int cy = cur.y - top;

            int bestX = -1, bestY = -1;
            int bestD2 = INT_MAX;

            BYTE* base = static_cast<BYTE*>(pPixels);
            for (int y = 0; y < capH; ++y) {
                BYTE* row = base + y * currentFullW * 4; 
                for (int x = 0; x < capW; ++x) {
                    if (similarRGB(row[x*4+2], row[x*4+1], row[x*4], tgtR, tgtG, tgtB, currentConfig.tolerance)) {
                        int dx = x - cx;
                        int dy = y - cy;
                        int d2 = dx * dx + dy * dy;
                        if (d2 < bestD2) {
                            bestD2 = d2;
                            bestX = x;
                            bestY = y;
                            if (bestD2 == 0) break;
                        }
                    }
                }
                if (bestD2 == 0) break;
            }

            if (bestX >= 0) {
                SetCursorPos(left + bestX, top + bestY);
            }
        } else {
             // In HOLD mode, reset color is handled by wasHolding check
             // In TOGGLE mode, we keep color until toggle off (handled in toggle logic)
        }

        Sleep(currentConfig.loopSleepMs);
    }

    SelectObject(hdcMem, oldObj);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    return 0;
}
