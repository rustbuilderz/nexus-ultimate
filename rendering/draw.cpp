#include "pch.h"
#include "draw.h"

#include "config.h"
#include "globals.h"
#include "helpers/vk_sanitizer.h"
#include "json_parsing/loadops.h"
#include "rendering/bitmap_handler.h"
#include "rendering/ui_widgets.h"

#include <algorithm>
#include <filesystem>
#include <shellapi.h>

namespace fs = std::filesystem;
using Draw = Render::Draw;

namespace {

    void OpenUrl(const wchar_t* url) {
        ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }

    std::string StemAsciiLower(const std::wstring& fullPath) {
        const std::wstring stemW = fs::path(fullPath).stem().wstring();

        std::string out;
        out.reserve(stemW.size());

        for (wchar_t wc : stemW) {
            const wchar_t lc = static_cast<wchar_t>(towlower(wc));
            out.push_back((lc <= 0x7F) ? static_cast<char>(lc) : '_');
        }

        return out;
    }

    void LoadAllTextures(Draw& d) {
        d.textures.clear();
        d.visibleIndices.clear();
        d.selectedIndex = -1;

        const fs::path folder = Paths::PickBmpFolder();
        LOGI("Image folder: %ls", folder.wstring().c_str());

        if (!fs::exists(folder) || !fs::is_directory(folder)) {
            LOGW("Could not find bmp folder.");
            return;
        }

        std::vector<fs::path> files;
        for (const auto& e : fs::directory_iterator(folder)) {
            if (!e.is_regular_file()) continue;

            const auto& p = e.path();
            if (Paths::HasBmpExtension(p)) files.push_back(p);
        }

        std::sort(files.begin(), files.end(), [](const fs::path& x, const fs::path& y) {
            return x.filename().wstring() < y.filename().wstring();
            });

        const int cap = static_cast<int>(std::min<size_t>(files.size(), g_maxImages));
        LOGI("BMP files found: %zu, loading up to: %d", files.size(), cap);

        d.textures.reserve(cap);

        for (int i = 0; i < cap; ++i) {
            const std::wstring path = files[i].wstring();

            Render::LoadedTexture tex = d.wic.LoadTextureBGRA(d.dx.device.Get(), path);
            if (!tex) {
                LOGW("Failed to load texture: %ls", path.c_str());
                continue;
            }

            Render::TextureItem texItem{};
            texItem.path = path;
            texItem.srv = tex.srv;

            d.textures.push_back(std::move(texItem));
        }

        LOGI("Loaded %zu textures.", d.textures.size());
    }

    void DrawVkPickerRow(const char* label, int& bindVk, bool allowMouse = true) {
        ImGui::TextUnformatted(label);
        ImGui::SameLine(200);
        ImGui::Text("%s", Input::VkToName(bindVk));

        ImGui::SameLine();
        ImGui::PushID(label);

        static ImGuiID s_capturingId = 0;
        const ImGuiID rowId = ImGui::GetID("vk_row");
        const bool capturing = (s_capturingId == rowId);

        if (!capturing) {
            if (ImGui::Button("Change", ImVec2(90, 0))) s_capturingId = rowId;

            ImGui::SameLine();
            if (ImGui::Button("Clear", ImVec2(70, 0))) bindVk = 0;
        }
        else {
            ImGui::TextUnformatted("Press a key...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(90, 0))) s_capturingId = 0;

            int vk = 0;
            if (Input::CaptureVkOnce(vk, allowMouse)) {
                bindVk = vk;
                s_capturingId = 0;
            }
        }

        ImGui::PopID();
    }

    void DrawHomeLeft(Draw& d) {
        UI::Panel p("##controls", "Home / Control Panel", ImVec2(0, 0), true);

        ImGui::SliderFloat("Down force", &g_downForce, 0.0f, 5.0f, "%.1f");

        if (ImGui::SliderFloat("Right Drift", &g_rightDrift, 0.0f, 5.0f, "%.1f")) {
            if (g_rightDrift != 0.0f) g_leftDrift = 0.0f;
        }

        if (ImGui::SliderFloat("Left Drift", &g_leftDrift, 0.0f, 5.0f, "%.1f")) {
            if (g_leftDrift != 0.0f) g_rightDrift = 0.0f;
        }

        if (ImGui::SliderInt("Delay", &g_delayMs, 0, 50, "%d ms")) {
            if (g_delayMs < 1) g_delayMs = 1;
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Mouse backend");

        int backend = static_cast<int>(g_moveBackend.load(std::memory_order_relaxed));
        const char* items[] = { "Arduino (COM)", "Windows (SendInput)" };

        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::Combo("##mouse_backend", &backend, items, IM_ARRAYSIZE(items))) {
            g_moveBackend.store(static_cast<MoveBackend>(backend), std::memory_order_relaxed);
            LOGI("Mouse backend -> %s", (backend == 0) ? "ArduinoSerial" : "WindowsSendInput");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Toggle box", &d.toggleBox);
        ImGui::SameLine();
        ImGui::Text("(%s)", d.toggleBox ? "ON" : "OFF");

        ImGui::Spacing();
        ImGui::Spacing();

        {
            UI::ImStyleGuard st;
            st.Col(ImGuiCol_Button, IM_COL32(255, 115, 25, 255));
            st.Col(ImGuiCol_ButtonHovered, IM_COL32(255, 140, 55, 255));
            st.Col(ImGuiCol_ButtonActive, IM_COL32(255, 95, 10, 255));
            if (ImGui::Button("Join Discord", ImVec2(200, 36))) OpenUrl(L"https://discord.com/");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Tips:");
        ImGui::BulletText("F5 reloads ops + textures");
        ImGui::BulletText("XBUTTON2 toggles script (worker thread)");
    }

    void DrawHomeRight(Draw& d) {
        const float colW = ImGui::GetContentRegionAvail().x;
        float y = ImGui::GetCursorPosY();

        constexpr float kGap = 10.0f;
        constexpr float kStatusH = 220.0f;
        constexpr float kKeybindsH = 300.0f;

        {
            ImGui::SetCursorPosY(y);
            UI::Panel p("##status", "Status", ImVec2(colW, kStatusH), false);

            std::string op;
            { std::lock_guard<std::mutex> lk(g_selMutex); op = g_selectedOpName; }

            ImGui::Text("Loaded textures: %d", static_cast<int>(d.textures.size()));
            ImGui::Text("Shown textures:  %d", static_cast<int>(d.visibleIndices.size()));
            ImGui::Text("Script: %s", g_scriptEnabled ? "ON" : "OFF");
            ImGui::Text("Holding: %s", g_holding ? "YES" : "NO");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextUnformatted("Current selected:");
            ImGui::Text("%s", op.empty() ? "(none)" : op.c_str());
        }

        y += kStatusH + kGap;

        {
            ImGui::SetCursorPosY(y);
            UI::Panel p("##keybinds", "Keybinds", ImVec2(colW, kKeybindsH), false);

            DrawVkPickerRow("Toggle script", g_bindToggleScriptVK, true);
            DrawVkPickerRow("Quit", g_bindQuitVK, false);
            DrawVkPickerRow("Toggle console", g_bindToggleConsoleVK, false);
            DrawVkPickerRow("Reload", g_bindReloadVK, false);

            ImGui::Separator();
            ImGui::Checkbox("Require both mouse buttons (hold)", &g_requireBothMouseButtons);

            if (g_requireBothMouseButtons) {
                DrawVkPickerRow("Hold mouse 1", g_bindHoldMouse1VK, true);
                DrawVkPickerRow("Hold mouse 2", g_bindHoldMouse2VK, true);
            }

            ImGui::Separator();
            DrawVkPickerRow("Hold modifier (optional)", g_bindHoldModifierVK, false);
            ImGui::TextDisabled("Set modifier to (none) to disable.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static std::string s_saveErr;
            if (ImGui::Button("Save Keybinds", ImVec2(-1, 34))) {
                std::string err;
                if (!SaveKeybindsToJson(&err)) {
                    s_saveErr = err.empty() ? "Save failed" : err;
                }
                else {
                    s_saveErr.clear();
                }
            }

            if (!s_saveErr.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", s_saveErr.c_str());
            }
        }
    }

    void DrawPicker(Draw& d) {
        {
            UI::Panel top("##picker_top", "Choose Operator", ImVec2(0, 150), false);

            std::string op;
            { std::lock_guard<std::mutex> lk(g_selMutex); op = g_selectedOpName; }
            ImGui::Text("Current selected: %s", op.empty() ? "(none)" : op.c_str());

            ImGui::Spacing();

            if (ImGui::Button("Reload (F5)", ImVec2(140, 0))) d.Reload();

            ImGui::SameLine();
            ImGui::Text("Shown: %d / %d",
                static_cast<int>(d.visibleIndices.size()),
                static_cast<int>(d.textures.size()));
        }

        ImGui::Spacing();
        static std::string s_saveErr;

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string op;
            { std::lock_guard<std::mutex> lk(g_selMutex); op = g_selectedOpName; }

            if (op.empty()) {
                s_saveErr = "No operator selected";
            }
            else {
                std::string err;
                if (!SaveUiForOperator(op, &err)) s_saveErr = err;
                else s_saveErr.clear();
            }
        }

        if (!s_saveErr.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", s_saveErr.c_str());
        }

        UI::Panel gridPanel("##picker_grid_panel", "Operators", ImVec2(0, 0), false);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##grid", avail, false, ImGuiWindowFlags_NoTitleBar);

        const ImVec2 gridAvail = ImGui::GetContentRegionAvail();
        const int cols = (g_grid > 0) ? g_grid : 1;
        constexpr float kSpacing = 6.0f;

        float tile = (gridAvail.x - (cols - 1) * kSpacing) / static_cast<float>(cols);
        constexpr float kTileMin = 42.0f;
        constexpr float kTileMax = 90.0f;
        tile = std::max(kTileMin, std::min(tile, kTileMax));

        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int i = 0; i < static_cast<int>(d.visibleIndices.size()); ++i) {
            const int texIndex = d.visibleIndices[i];

            ImGui::PushID(i);
            const ImTextureID texId = reinterpret_cast<ImTextureID>(d.textures[texIndex].srv.Get());
            const bool clicked = ImGui::ImageButton("##op", texId, ImVec2(tile, tile));

            if (i == d.selectedIndex) {
                const ImVec2 r0 = ImGui::GetItemRectMin();
                const ImVec2 r1 = ImGui::GetItemRectMax();
                dl->AddRect(r0, r1, IM_COL32(255, 115, 25, 255), 8.0f, 0, 3.0f);
                dl->AddRect(ImVec2(r0.x + 2, r0.y + 2), ImVec2(r1.x - 2, r1.y - 2),
                    IM_COL32(255, 115, 25, 170), 8.0f, 0, 2.0f);
            }

            if (clicked) {
                if (d.selectedIndex == i) {
                    d.selectedIndex = -1;
                    std::lock_guard<std::mutex> lk(g_selMutex);
                    g_selectedOpName.clear();
                }
                else {
                    d.selectedIndex = i;

                    const std::string name = StemAsciiLower(d.textures[texIndex].path);
                    {
                        std::lock_guard<std::mutex> lk(g_selMutex);
                        g_selectedOpName = name;
                    }

                    std::string err;
                    ApplyUiFromOperatorJson(name, &err);
                    if (!err.empty()) LOGW("ApplyUiFromOperatorJson(%s) failed: %s", name.c_str(), err.c_str());
                }
            }

            ImGui::PopID();

            if ((i + 1) % cols != 0) ImGui::SameLine(0.0f, kSpacing);
        }

        ImGui::EndChild();
    }

} // namespace

bool Draw::Init(HWND h) {
    hwnd = h;

    if (!dx.Create(hwnd)) return false;
    if (!wic.Init()) return false;

    return true;
}

void Draw::Shutdown() {
    if (worker.joinable()) worker.join();

    textures.clear();
    visibleIndices.clear();
    selectedIndex = -1;

    wic.Shutdown();
}

void Draw::Reload() {
    LoadAllTextures(*this);
    RebuildVisible();
}

void Draw::RebuildVisible() {
    visibleIndices.clear();
    visibleIndices.reserve(textures.size());

    for (int i = 0; i < static_cast<int>(textures.size()); ++i) {
        const std::wstring fileOnly = fs::path(textures[i].path).filename().wstring();

        if (enableOnlyAvailable && Paths::IsExcludedName(fileOnly, g_excludedNames, g_excludedNamesCount))
            continue;

        if (enableOnlyAvailable) {
            const std::string stem = StemAsciiLower(textures[i].path);
            if (g_opsNames.find(stem) == g_opsNames.end()) continue;
        }

        visibleIndices.push_back(i);
    }

    if (selectedIndex >= static_cast<int>(visibleIndices.size())) {
        selectedIndex = -1;
        std::lock_guard<std::mutex> lk(g_selMutex);
        g_selectedOpName.clear();
    }
}

void Draw::OnResize(UINT w, UINT h) {
    dx.Resize(w, h);
    UI::ApplyRoundedRegion(hwnd, 14);
}

void Draw::OnF5() {
    Reload();
}

void Draw::TickUI() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    const ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;

    UI::ImStyleGuard st;
    st.Var(ImGuiStyleVar_FrameRounding, 12.0f);
    st.Var(ImGuiStyleVar_FrameBorderSize, 0.0f);
    st.Col(ImGuiCol_Button, IM_COL32(30, 30, 30, 220));
    st.Col(ImGuiCol_ButtonHovered, IM_COL32(45, 45, 45, 235));
    st.Col(ImGuiCol_ButtonActive, IM_COL32(55, 55, 55, 255));

    ImGui::Begin("##root", nullptr, rootFlags);

    UI::DrawHeaderBar(hwnd, page);

    ImGui::Spacing();
    ImGui::Spacing();

    switch (page) {
    case UI::PAGE_HOME:
        if (ImGui::BeginTable("home_tbl", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            DrawHomeLeft(*this);
            ImGui::TableSetColumnIndex(1);
            DrawHomeRight(*this);
            ImGui::EndTable();
        }
        break;

    case UI::PAGE_SCRIPT:
        DrawPicker(*this);
        break;

    case UI::PAGE_CUSTOMIZE:
        ImGui::TextUnformatted("Customization page (todo)");
        break;

    case UI::PAGE_SETTINGS:
        ImGui::TextUnformatted("Settings page (todo)");
        break;
    }

    ImGui::End();
}
