#include "pch.h"
#include "globals.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

ULONG_PTR g_gdiplusToken = 0;
HWND g_hwnd = nullptr;

std::atomic<bool> g_running(true);
std::atomic<bool> g_holding(false);
std::atomic<bool> g_scriptEnabled(true);
std::atomic<bool> g_consoleVisible{ false };

HWND g_consoleHwnd = nullptr;

std::mutex g_selMutex;
std::string g_selectedOpName;

std::vector<std::wstring> g_paths;
std::vector<Bitmap*> g_bitmaps;

std::vector<std::wstring> g_displayPaths;
std::vector<Bitmap*> g_displayBitmaps;

int g_selectedIndex = -1;

int g_grid = 9;
const int g_maxImages = 76;
const int g_uiTop = 40;

HWND g_hChkEnableOnlyAvailable = nullptr;
bool g_enableOnlyAvailable = true;

std::unordered_set<std::string> g_opsNames;

const wchar_t* const g_excludedNames[] = {
    L"ignore",
};

const size_t g_excludedNamesCount = sizeof(g_excludedNames) / sizeof(g_excludedNames[0]);

float g_downForce = 0.0f;
float g_upForce = 0.0f;
float g_rightDrift = 0.0f;
float g_leftDrift = 0.0f;
int   g_delayMs = 0;
bool  g_toggleBox = false;

int g_bindToggleScriptVK = VK_XBUTTON2;
int g_bindQuitVK = VK_END;
int g_bindToggleConsoleVK = VK_INSERT;
int g_bindReloadVK = VK_F5;

int  g_bindHoldModifierVK = 0;
bool g_requireBothMouseButtons = true;
int  g_bindHoldMouse1VK = VK_LBUTTON;
int  g_bindHoldMouse2VK = VK_RBUTTON;

std::atomic<MoveBackend> g_moveBackend{ MoveBackend::WindowsSendInput };
