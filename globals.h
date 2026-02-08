#pragma once

#include "pch.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

using Gdiplus::Bitmap;
using Gdiplus::Graphics;
using Gdiplus::InterpolationModeHighQualityBicubic;
using Gdiplus::Rect;

using json = nlohmann::json;

enum class MoveBackend : int {
    ArduinoSerial = 0,
    WindowsSendInput = 1,
};

// Win32 / lifetime
extern ULONG_PTR g_gdiplusToken;
extern HWND g_hwnd;

// Runtime flags
extern std::atomic<bool> g_running;
extern std::atomic<bool> g_holding;
extern std::atomic<bool> g_scriptEnabled;
extern std::atomic<bool> g_consoleVisible;

extern HWND g_consoleHwnd;

// Selection state
extern std::mutex g_selMutex;
extern std::string g_selectedOpName;

// Bitmaps / paths (GDI+ side)
extern std::vector<std::wstring> g_paths;
extern std::vector<Bitmap*> g_bitmaps;

extern std::vector<std::wstring> g_displayPaths;
extern std::vector<Bitmap*> g_displayBitmaps;

extern int g_selectedIndex;

// UI sizing
extern int g_grid;
extern const int g_maxImages;
extern const int g_uiTop;

// Operator filtering
extern HWND g_hChkEnableOnlyAvailable;
extern bool g_enableOnlyAvailable;
extern std::unordered_set<std::string> g_opsNames;

extern const wchar_t* const g_excludedNames[];
extern const size_t g_excludedNamesCount;

// Persistable UI state
extern float g_downForce;
extern float g_upForce;
extern float g_rightDrift;
extern float g_leftDrift;
extern int   g_delayMs;
extern bool  g_toggleBox;

// Keybinds
extern int g_bindToggleScriptVK;
extern int g_bindQuitVK;
extern int g_bindToggleConsoleVK;
extern int g_bindReloadVK;
extern int g_bindHoldModifierVK;

extern bool g_requireBothMouseButtons;
extern int g_bindHoldMouse1VK;
extern int g_bindHoldMouse2VK;

// Backend toggle
extern std::atomic<MoveBackend> g_moveBackend;
