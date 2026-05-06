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

extern ULONG_PTR g_gdiplusToken;
extern HWND g_hwnd;

extern std::atomic<bool> g_running;
extern std::atomic<bool> g_holding;
extern std::atomic<bool> g_scriptEnabled;
extern std::atomic<bool> g_consoleVisible;

extern HWND g_consoleHwnd;

extern std::mutex g_selMutex;
extern std::string g_selectedOpName;

extern std::vector<std::wstring> g_paths;
extern std::vector<Bitmap*> g_bitmaps;

extern std::vector<std::wstring> g_displayPaths;
extern std::vector<Bitmap*> g_displayBitmaps;

extern int g_selectedIndex;

extern int g_grid;
extern const int g_maxImages;
extern const int g_uiTop;

extern HWND g_hChkEnableOnlyAvailable;
extern bool g_enableOnlyAvailable;
extern std::unordered_set<std::string> g_opsNames;

extern const wchar_t* const g_excludedNames[];
extern const size_t g_excludedNamesCount;

extern int g_downForce;
extern int g_rightDrift;
extern int g_leftDrift;
extern int   g_delayMs;
extern bool  g_toggleBox;

extern int g_bindToggleScriptVK;
extern int g_bindQuitVK;
extern int g_bindToggleConsoleVK;
extern int g_bindReloadVK;
extern int g_bindHoldModifierVK;

extern bool g_requireBothMouseButtons;
extern int g_bindHoldMouse1VK;
extern int g_bindHoldMouse2VK;

extern int g_moveDiv;

extern int g_themeIndex;

extern int g_selectedBmpConfigIndex;

extern std::atomic<bool> g_reloadMoveFromOps;

extern bool g_scriptConfigWindowOpen;
