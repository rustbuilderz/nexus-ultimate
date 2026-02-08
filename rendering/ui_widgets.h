#pragma once

#include <windows.h>
#include "imgui/imgui.h"

namespace UI {

    struct ImStyleGuard {
        int vars = 0;
        int cols = 0;

        ~ImStyleGuard() {
            if (cols) ImGui::PopStyleColor(cols);
            if (vars) ImGui::PopStyleVar(vars);
        }

        void Var(ImGuiStyleVar idx, float v) {
            ImGui::PushStyleVar(idx, v);
            ++vars;
        }

        void Var(ImGuiStyleVar idx, const ImVec2& v) {
            ImGui::PushStyleVar(idx, v);
            ++vars;
        }

        void Col(ImGuiCol idx, ImU32 v) {
            ImGui::PushStyleColor(idx, v);
            ++cols;
        }

        void Col(ImGuiCol idx, const ImVec4& v) {
            ImGui::PushStyleColor(idx, v);
            ++cols;
        }
    };

    struct Panel {
        bool open = false;
        ImStyleGuard st;

        Panel(const char* id, const char* title, ImVec2 size, bool scroll);
        ~Panel();
    };

    enum UiPage : int {
        PAGE_HOME = 0,
        PAGE_SCRIPT = 1,
        PAGE_CUSTOMIZE = 2,
        PAGE_SETTINGS = 3,
    };

    void ApplyRoundedRegion(HWND hwnd, int radius);

    void DrawWindowButtons(HWND hwnd);
    void DrawHeaderBar(HWND hwnd, int& page);
    void ApplyOrangeBlackTheme();

} // namespace UI
