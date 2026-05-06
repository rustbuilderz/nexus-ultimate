#include "pch.h"
#include "worker.h"

#include "globals.h"
#include "json_parsing/loadops.h"

#include <chrono>
#include <cmath>
#include <thread>
#include <windows.h>

namespace {

void MoveMouseWindows(int dx, int dy) {
    LOGI("SENDINPUT MOVE | dx=%d dy=%d", dx, dy);

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

        LOGI("ACCUM | fx=%.6f fy=%.6f remX=%.6f remY=%.6f dx=%d dy=%d",
            fx, fy, remX, remY, dx, dy);

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

    g_downForce = (int)MoveMath::ForceJsonToUi((float)s_json);
    g_delayMs = delay;

    const int step_ui = (int)MoveMath::ForceJsonToUi((float)step_json);

    LOGI("APPLY MOVE | s=%.6f dir=%c step=%d delay=%d downForce=%f",
        s_json, dir, step_ui, delay, (double)g_downForce);

    if (dir == 'W') {
        g_leftDrift = step_ui;
        g_rightDrift = 0;
    }
    else {
        g_rightDrift = step_ui;
        g_leftDrift = 0;
    }
}

}

void WorkerThread() {
    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        MouseAccum acc{};

        std::string lastOp;
        bool haveMoveConfig = false;

        uint64_t n = 0;

        while (g_running) {
            auto loopStart = std::chrono::steady_clock::now();

            if ((++n % 200) == 0) {
                LOGI("WORKER HEARTBEAT | run=%d script=%d hold=%d op='%s' cfg=%d remX=%.3f remY=%.3f",
                    (int)g_running, (int)g_scriptEnabled, (int)g_holding,
                    lastOp.c_str(), (int)haveMoveConfig,
                    acc.remX, acc.remY);
            }

            if (WasPressed(g_bindQuitVK)) {
                LOGI("QUIT VK=%d", g_bindQuitVK);
                g_running = false;
                break;
            }

            if (WasPressed(g_bindToggleConsoleVK)) {
                LOGI("CONSOLE TOGGLE VK=%d", g_bindToggleConsoleVK);
                Console::ToggleConsole();
            }

            if (WasPressed(g_bindToggleScriptVK)) {
                g_scriptEnabled = !g_scriptEnabled;
                LOGI("SCRIPT TOGGLE -> %s", g_scriptEnabled ? "ON" : "OFF");
            }

            if (WasPressed(g_bindReloadVK)) {
                LOGI("RELOAD KEYBINDS VK=%d", g_bindReloadVK);
                LoadKeybindsFromJson(nullptr);
            }

            bool holdMouseOK = true;
            if (g_requireBothMouseButtons) {
                const int vk1 = g_bindHoldMouse1VK ? g_bindHoldMouse1VK : VK_LBUTTON;
                const int vk2 = g_bindHoldMouse2VK ? g_bindHoldMouse2VK : VK_RBUTTON;
                holdMouseOK = IsPressed(vk1) && IsPressed(vk2);
            }

            bool holdModOK = true;
            if (g_bindHoldModifierVK != 0)
                holdModOK = IsPressed(g_bindHoldModifierVK);

            g_holding = holdMouseOK && holdModOK;

            std::string op;
            {
                std::lock_guard<std::mutex> lk(g_selMutex);
                op = g_selectedOpName;
            }

            if (g_reloadMoveFromOps.exchange(false)) {
                if (!op.empty()) {
                    double s = 0, step = 0;
                    char dir = 0;
                    int delay = 10;

                    if (GetMoveForOperator(op, s, dir, step, delay)) {
                        ApplyMoveGlobals(s, dir, step, delay);
                        haveMoveConfig = true;
                        LOGI("OPS.JSON RELOAD | op='%s' s=%.6f step=%.6f dir=%c delay=%d",
                            op.c_str(), s, step, dir, delay);
                    }
                    else {
                        haveMoveConfig = false;
                        LOGW("OPS.JSON RELOAD FAILED | op='%s'", op.c_str());
                    }
                }
            }

            if (op != lastOp) {
                lastOp = op;
                haveMoveConfig = false;

                if (op.empty()) {
                    LOGI("OP CLEARED");
                }
                else {
                    double s = 0, step = 0;
                    char dir = 0;
                    int delay = 10;

                    if (GetMoveForOperator(op, s, dir, step, delay)) {
                        ApplyMoveGlobals(s, dir, step, delay);
                        haveMoveConfig = true;

                        LOGI("OP LOADED '%s' s=%.6f step=%.6f dir=%c delay=%d",
                            op.c_str(), s, step, dir, delay);
                    }
                    else {
                        LOGW("OP LOAD FAILED '%s'", op.c_str());
                    }
                }
            }

            int sleepMs = haveMoveConfig ? g_delayMs : 10;
            sleepMs = std::clamp(sleepMs, 1, 50);

            if (g_holding && g_scriptEnabled && haveMoveConfig && !op.empty()) {
                const double fx = ((g_rightDrift - g_leftDrift)
                    / ((double)g_moveDiv * 5.0));

                const double fy = ((double)g_downForce) / (double)g_moveDiv;

                int dx = 0, dy = 0;
                const bool moved = acc.Step(fx, fy, dx, dy);

                LOGI("WIN MOVE | op='%s' fx=%.6f fy=%.6f dx=%d dy=%d remX=%.6f remY=%.6f",
                    op.c_str(), fx, fy, dx, dy, acc.remX, acc.remY);

                if (moved) MoveMouseWindows(dx, dy);
            }

            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - loopStart).count();

            if (dur > 20)
                LOGW("LOOP SPIKE %lld ms", dur);

            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }

        LOGI("WORKER EXIT");
    }
    catch (const std::exception& e) {
        LOGF("WORKER EXCEPTION %s", e.what());
    }
    catch (...) {
        LOGF("WORKER EXCEPTION UNKNOWN");
    }
}
