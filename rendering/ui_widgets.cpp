#include "pch.h"
#include "ui_widgets.h"

#include <shellapi.h>

namespace UI {

    namespace {
        constexpr float kHeaderH = 54.0f;

        constexpr float kCapBtnW = 38.0f;
        constexpr float kCapBtnH = 26.0f;
        constexpr float kCapGap = 6.0f;
        constexpr float kCapRightPad = 14.0f;
        constexpr float kCapTopPad = 14.0f;

        bool TabButtonSized(const char* label, bool active, float w) {
            const ImVec4 col = active ? ImVec4(0.20f, 0.20f, 0.20f, 1.00f)
                : ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(col.x + 0.05f, col.y + 0.05f, col.z + 0.05f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));

            const bool pressed = ImGui::Button(label, ImVec2(w, 0));

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            return pressed;
        }
    } // namespace

    Panel::Panel(const char* id, const char* title, ImVec2 size, bool scroll) {
        st.Var(ImGuiStyleVar_ChildRounding, 14.0f);
        st.Var(ImGuiStyleVar_ChildBorderSize, 1.0f);
        st.Var(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
        st.Col(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 0.70f));
        st.Col(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.20f, 0.90f));

        ImGuiWindowFlags flags = 0;
        if (!scroll) flags |= ImGuiWindowFlags_NoScrollbar;

        open = ImGui::BeginChild(id, size, true, flags);

        if (title && title[0]) {
            ImGui::TextUnformatted(title);
            ImGui::Separator();
            ImGui::Spacing();
        }
    }

    Panel::~Panel() {
        ImGui::EndChild();
    }

    void ApplyRoundedRegion(HWND hwnd, int radius) {
        RECT rc{};
        GetClientRect(hwnd, &rc);

        HRGN rgn = CreateRoundRectRgn(
            0, 0,
            rc.right + 1, rc.bottom + 1,
            radius * 2, radius * 2);

        SetWindowRgn(hwnd, rgn, TRUE);
    }

    void DrawWindowButtons(HWND hwnd) {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;

        ImGui::SetCursorScreenPos(
            ImVec2(p0.x + w - (kCapBtnW * 2.0f + kCapGap) - kCapRightPad, p0.y + kCapTopPad));

        if (ImGui::Button("-", ImVec2(kCapBtnW, kCapBtnH)))
            ShowWindow(hwnd, SW_MINIMIZE);

        ImGui::SameLine(0.0f, kCapGap);

        ImStyleGuard st;
        st.Col(ImGuiCol_Button, IM_COL32(180, 40, 40, 255));
        st.Col(ImGuiCol_ButtonHovered, IM_COL32(210, 60, 60, 255));
        st.Col(ImGuiCol_ButtonActive, IM_COL32(150, 30, 30, 255));

        if (ImGui::Button("X", ImVec2(kCapBtnW, kCapBtnH)))
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    void DrawHeaderBar(HWND hwnd, int& page) {
        (void)hwnd; // reserved (kept signature)

        ImGui::BeginChild("##header", ImVec2(0, kHeaderH), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::SetCursorPos(ImVec2(12.0f, 10.0f));
        constexpr float kGap = 8.0f;

        if (TabButtonSized("Home", page == PAGE_HOME, 92.0f)) page = PAGE_HOME;
        ImGui::SameLine(0.0f, kGap);

        if (TabButtonSized("Script", page == PAGE_SCRIPT, 92.0f)) page = PAGE_SCRIPT;
        ImGui::SameLine(0.0f, kGap);

        if (TabButtonSized("Customization", page == PAGE_CUSTOMIZE, 160.0f)) page = PAGE_CUSTOMIZE;
        ImGui::SameLine(0.0f, kGap);

        if (TabButtonSized("Settings", page == PAGE_SETTINGS, 110.0f)) page = PAGE_SETTINGS;

        ImGui::EndChild();
    }

    void ApplyOrangeBlackTheme() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 10.0f;
        s.ChildRounding = 12.0f;
        s.FrameRounding = 8.0f;
        s.GrabRounding = 8.0f;
        s.ScrollbarRounding = 10.0f;
        s.PopupRounding = 10.0f;
        s.TabRounding = 8.0f;

        s.FrameBorderSize = 1.0f;
        s.WindowBorderSize = 1.0f;
        s.ChildBorderSize = 1.0f;

        ImVec4* c = s.Colors;

        const ImVec4 orange(1.00f, 0.45f, 0.10f, 1.00f);
        const ImVec4 orangeHover(1.00f, 0.55f, 0.18f, 1.00f);
        const ImVec4 orangeActive(1.00f, 0.40f, 0.06f, 1.00f);

        c[ImGuiCol_WindowBg] = ImVec4(0.045f, 0.045f, 0.045f, 1.00f);
        c[ImGuiCol_ChildBg] = ImVec4(0.060f, 0.060f, 0.060f, 0.65f);
        c[ImGuiCol_PopupBg] = ImVec4(0.050f, 0.050f, 0.050f, 0.95f);

        c[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        c[ImGuiCol_Text] = ImVec4(1.00f, 0.55f, 0.18f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(1.00f, 0.55f, 0.18f, 0.45f);

        c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

        c[ImGuiCol_SliderGrab] = orange;
        c[ImGuiCol_SliderGrabActive] = orangeActive;

        c[ImGuiCol_Button] = orange;
        c[ImGuiCol_ButtonHovered] = orangeHover;
        c[ImGuiCol_ButtonActive] = orangeActive;

        c[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

        c[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        c[ImGuiCol_SeparatorHovered] = orangeHover;
        c[ImGuiCol_SeparatorActive] = orange;

        c[ImGuiCol_CheckMark] = orange;

        c[ImGuiCol_TitleBg] = ImVec4(0.045f, 0.045f, 0.045f, 1.00f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.045f, 0.045f, 0.045f, 1.00f);
    }

} // namespace UI
