#include "pch.h"
#include "worker.h"

#include "config.h"
#include "globals.h"
#include "json_parsing/loadops.h"

#include <chrono>
#include <cmath>
#include <thread>
#include <windows.h>

namespace {

    HANDLE OpenAndConfigureSerial() {
        HANDLE hSerial = CreateFileW(Config::kComPort, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hSerial == INVALID_HANDLE_VALUE) {
            const DWORD e = GetLastError();
            const std::wstring m = Console::Win32ErrorMessageW(e);
            LOGW("Serial: open failed on %ls (err=%lu: %ls)", Config::kComPort, (unsigned long)e, m.c_str());
            return INVALID_HANDLE_VALUE;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);

        WIN_CHECK_RET(GetCommState(hSerial, &dcb), INVALID_HANDLE_VALUE);

        dcb.BaudRate = 9600;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;

        WIN_CHECK_RET(SetCommState(hSerial, &dcb), INVALID_HANDLE_VALUE);

        LOGI("Serial: connected to %ls @ 9600 8N1", Config::kComPort);
        return hSerial;
    }

    bool SendMoveArduino(HANDLE hSerial, double s, char dir, double step) {
        std::string s1 = "S" + std::to_string(s) + "\n";

        std::string s2;
        s2.push_back(dir);
        s2 += std::to_string(step);
        s2 += "\n";

        DWORD written = 0;
        if (!WriteFile(hSerial, s1.c_str(), (DWORD)s1.size(), &written, nullptr)) return false;
        if (!WriteFile(hSerial, s2.c_str(), (DWORD)s2.size(), &written, nullptr)) return false;
        return true;
    }

    void MoveMouseWindows(int dx, int dy) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
        in.mi.dx = dx;
        in.mi.dy = dy;
        SendInput(1, &in, sizeof(in));
    }

    struct MouseAccum {
        double remX = 0.0;
        double remY = 0.0;

        bool Step(double fx, double fy, int& outDx, int& outDy) {
            remX += fx;
            remY += fy;

            const int dx = (int)std::trunc(remX);
            const int dy = (int)std::trunc(remY);

            if (dx) remX -= dx;
            if (dy) remY -= dy;

            outDx = dx;
            outDy = dy;
            return dx != 0 || dy != 0;
        }
    };

    inline bool IsPressed(int vk) {
        return vk != 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    inline bool WasPressed(int vk) {
        return vk != 0 && (GetAsyncKeyState(vk) & 1) != 0;
    }

    void ApplyMoveGlobals(double s_json, char dir, double step_json, int delay) {
        if (delay < 1) delay = 1;
        if (delay > 50) delay = 50;

        g_downForce = MoveMath::ForceJsonToUi((float)s_json);
        g_delayMs = delay;

        const float step_ui = MoveMath::ForceJsonToUi((float)step_json);

        if (dir == 'W') {
            g_leftDrift = step_ui;
            g_rightDrift = 0.0f;
        }
        else {
            g_rightDrift = step_ui;
            g_leftDrift = 0.0f;
        }
    }

    void ReloadOpsAndTextures() {
        LOGI("ReloadOpsAndTextures(): not implemented (stub)");
    }

} // namespace

void WorkerThread() {
    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        HANDLE hSerial = INVALID_HANDLE_VALUE;
        MouseAccum acc{};

        MoveBackend lastBackend = MoveBackend::WindowsSendInput;
        std::string lastOp;
        bool haveMoveConfig = false;

        while (g_running) {
            if (WasPressed(g_bindQuitVK)) {
                LOGI("Quit pressed (vk=%d) -> stopping", g_bindQuitVK);
                g_running = false;
                break;
            }

            if (WasPressed(g_bindToggleConsoleVK)) {
                LOGI("Toggle console pressed (vk=%d)", g_bindToggleConsoleVK);
                Console::ToggleConsole();
            }

            if (WasPressed(g_bindReloadVK)) {
                LOGI("Reload pressed (vk=%d)", g_bindReloadVK);
                ReloadOpsAndTextures();
            }

            if (WasPressed(g_bindToggleScriptVK)) {
                g_scriptEnabled = !g_scriptEnabled;
                LOGI("Script toggled (vk=%d) -> %s",
                    g_bindToggleScriptVK, g_scriptEnabled ? "ON" : "OFF");
            }

            const MoveBackend backend = g_moveBackend.load(std::memory_order_relaxed);
            if (backend != lastBackend) {
                acc = MouseAccum{};
                LOGI("Backend switch: %s -> %s (accumulator reset)",
                    lastBackend == MoveBackend::ArduinoSerial ? "ArduinoSerial" : "WindowsSendInput",
                    backend == MoveBackend::ArduinoSerial ? "ArduinoSerial" : "WindowsSendInput");
                lastBackend = backend;
            }

            if (backend == MoveBackend::ArduinoSerial) {
                if (hSerial == INVALID_HANDLE_VALUE) {
                    hSerial = OpenAndConfigureSerial();
                    if (hSerial == INVALID_HANDLE_VALUE) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            else {
                if (hSerial != INVALID_HANDLE_VALUE) {
                    CloseHandle(hSerial);
                    hSerial = INVALID_HANDLE_VALUE;
                    LOGI("Serial: closed (Windows backend active)");
                }
            }

            bool holdMouseOK = true;
            if (g_requireBothMouseButtons) {
                const int vk1 = (g_bindHoldMouse1VK != 0) ? g_bindHoldMouse1VK : VK_LBUTTON;
                const int vk2 = (g_bindHoldMouse2VK != 0) ? g_bindHoldMouse2VK : VK_RBUTTON;
                holdMouseOK = IsPressed(vk1) && IsPressed(vk2);
            }

            bool holdModOK = true;
            if (g_bindHoldModifierVK != 0)
                holdModOK = IsPressed(g_bindHoldModifierVK);

            g_holding = (holdMouseOK && holdModOK);

            std::string op;
            {
                std::lock_guard<std::mutex> lk(g_selMutex);
                op = g_selectedOpName;
            }

            if (op != lastOp) {
                lastOp = op;
                haveMoveConfig = false;

                if (op.empty()) {
                    LOGI("Operator cleared -> movement disabled");
                }
                else {
                    double s = 0.0;
                    double step = 0.0;
                    char dir = 0;
                    int delay = 10;

                    if (!GetMoveForOperator(op, s, dir, step, delay)) {
                        LOGW("GetMoveForOperator failed for op='%s' -> movement disabled", op.c_str());
                    }
                    else {
                        ApplyMoveGlobals(s, dir, step, delay);
                        haveMoveConfig = true;

                        LOGI("Operator changed -> '%s' down=%.6f right=%.6f left=%.6f delay=%d",
                            op.c_str(),
                            (double)g_downForce,
                            (double)g_rightDrift,
                            (double)g_leftDrift,
                            g_delayMs);
                    }
                }
            }

            int sleepMs = haveMoveConfig ? g_delayMs : 10;
            if (sleepMs < 1) sleepMs = 1;
            if (sleepMs > 50) sleepMs = 50;

            if (g_holding && g_scriptEnabled && haveMoveConfig && !op.empty()) {
                if (backend == MoveBackend::ArduinoSerial) {
                    const double s = (double)g_downForce;
                    const char dir = (g_leftDrift > 0.0f) ? 'W' : 'E';
                    const double step = (g_leftDrift > 0.0f) ? (double)g_leftDrift : (double)g_rightDrift;

                    if (!SendMoveArduino(hSerial, s, dir, step)) {
                        LOGW("Serial: write failed -> dropping connection");
                        CloseHandle(hSerial);
                        hSerial = INVALID_HANDLE_VALUE;
                    }
                }
                else {
                    const double fx = (double)g_rightDrift - (double)g_leftDrift;
                    const double fy = (double)g_downForce;

                    int dx = 0, dy = 0;
                    const bool willMove = acc.Step(fx, fy, dx, dy);

                    LOGI("WinMove: op='%s' fx=%.6f fy=%.6f -> dx=%d dy=%d | remX=%.6f remY=%.6f | delay=%d",
                        op.c_str(), fx, fy, dx, dy, acc.remX, acc.remY, sleepMs);

                    if (willMove) MoveMouseWindows(dx, dy);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }

        if (hSerial != INVALID_HANDLE_VALUE)
            CloseHandle(hSerial);

        LOGI("Worker thread exiting.");
    }
    catch (const std::exception& e) {
        LOGF("Worker thread exception: %s", e.what());
    }
    catch (...) {
        LOGF("Worker thread exception: unknown");
    }
}
