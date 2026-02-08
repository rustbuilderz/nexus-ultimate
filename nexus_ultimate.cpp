#include "pch.h"

#include "console_alloc.h"
#include "config.h"
#include "globals.h"
#include "worker.h"

#include "rendering/draw.h"
#include "rendering/ui_widgets.h"

#include <windows.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shell32.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Draw = Render::Draw;

namespace {
    Draw* g_app = nullptr;

    constexpr int kTitlebarH = 54;
    constexpr int kResizeBorder = 8;

    constexpr int kCapBtnW = 38;
    constexpr int kCapBtnH = 26;
    constexpr int kCapGap = 6;
    constexpr int kCapRightPad = 14;
    constexpr int kCapTopPad = 14;
    constexpr int kCapBtnCount = 2;
} // namespace

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        UI::ApplyRoundedRegion(hwnd, 14);
        return 0;

    case WM_SIZE:
        if (g_app && wParam != SIZE_MINIMIZED) {
            const UINT w = static_cast<UINT>(LOWORD(lParam));
            const UINT h = static_cast<UINT>(HIWORD(lParam));
            g_app->OnResize(w, h);
        }
        return 0;

    case WM_NCHITTEST: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        const bool left = pt.x < (rc.left + kResizeBorder);
        const bool right = pt.x >= (rc.right - kResizeBorder);
        const bool top = pt.y < (rc.top + kResizeBorder);
        const bool bottom = pt.y >= (rc.bottom - kResizeBorder);

        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;

        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;

        if (pt.y < kTitlebarH) {
            // 1) Exclude the tab strip so ImGui can click it
            constexpr int kTabLeft = 12;
            constexpr int kTabTop = 10;
            constexpr int kTabH = 34;
            constexpr int kTabGap = 8;

            constexpr int kTabHomeW = 92;
            constexpr int kTabScriptW = 92;
            constexpr int kTabCustW = 160;
            constexpr int kTabSetW = 110;

            const int tabAreaW =
                kTabHomeW + kTabGap +
                kTabScriptW + kTabGap +
                kTabCustW + kTabGap +
                kTabSetW;

            const int tabRight = kTabLeft + tabAreaW;
            const int tabBot = kTabTop + kTabH;

            if (pt.x >= kTabLeft && pt.x < tabRight && pt.y >= kTabTop && pt.y < tabBot)
                return HTCLIENT;

            // 2) Exclude the window buttons (- / X)
            const int btnAreaW = kCapBtnCount * kCapBtnW + (kCapBtnCount - 1) * kCapGap;
            const int btnLeft = rc.right - kCapRightPad - btnAreaW;
            const int btnRight = rc.right - kCapRightPad;
            const int btnTop = kCapTopPad;
            const int btnBot = kCapTopPad + kCapBtnH;

            if (pt.x >= btnLeft && pt.x < btnRight && pt.y >= btnTop && pt.y < btnBot)
                return HTCLIENT;

            // 3) Everything else in the header can drag
            return HTCAPTION;
        }

        return HTCLIENT;
    }

    case WM_KEYDOWN:
        if (wParam == VK_F5 && g_app) {
            g_app->OnF5();
            return 0;
        }
        break;

    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    Console::AllocateConsoleOnce();
    SetUnhandledExceptionFilter(Console::UnhandledExceptionLogger);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    constexpr const wchar_t* kClassName = L"R6_ImGui_DX11";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 900,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    Draw draw{};
    g_app = &draw;

    if (!draw.Init(hwnd)) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    UI::ApplyOrangeBlackTheme();

    ImGuiIO& io = ImGui::GetIO();
    if (ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\CascadiaMono.ttf", 18.0f)) {
        io.FontDefault = font;
    }
    else if (ImFont* fallback = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 18.0f)) {
        io.FontDefault = fallback;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(draw.dx.device.Get(), draw.dx.ctx.Get());

    draw.Reload();

    std::string err;
    if (!LoadKeybindsFromJson(&err))
        LOGW("LoadKeybindsFromJson failed: %s", err.c_str());

    draw.worker = std::thread(WorkerThread);

    MSG msg{};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        draw.TickUI();

        ImGui::Render();

        const float clearColor[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
        draw.dx.BeginFrame(clearColor);
        draw.dx.RenderImGuiDrawData();
        draw.dx.EndFrame();
    }

    g_app = nullptr;

    draw.Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CoUninitialize();
    return 0;
}
