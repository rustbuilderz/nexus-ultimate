#include "pch.h"
#include "draw.h"
#include "imgui/imgui.h"
#include <vector>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <Lmcons.h>
#include <GLFW/glfw3native.h>
#include "globals.h"
#include "helpers/VK_sanitizer.h"
#include "json_parsing/loadops.h"
#include "rendering/operator_config_window.h"
#include <cwctype>

#pragma comment(lib, "advapi32.lib")

using Input::KeybindWidget;

static const std::string& CachedWindowsUsernameUtf8()
{
    static std::string s_name;
    static bool s_loaded = false;
    if (!s_loaded) {
        s_loaded = true;
        wchar_t w[UNLEN + 1]{};
        DWORD n = UNLEN + 1;
        if (!GetUserNameW(w, &n)) {
            s_name = "User";
            return s_name;
        }
        const int need = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (need <= 1) {
            s_name = "User";
            return s_name;
        }
        s_name.resize(static_cast<size_t>(need - 1));
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s_name.data(), need, nullptr, nullptr);
    }
    return s_name;
}

static float frand01()
{
    return (float)(rand() % 10000) / 10000.0f;
}

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static ImU32 with_alpha(ImU32 c, float a01)
{
    ImU8 r = (ImU8)((c >> IM_COL32_R_SHIFT) & 0xFF);
    ImU8 g = (ImU8)((c >> IM_COL32_G_SHIFT) & 0xFF);
    ImU8 b = (ImU8)((c >> IM_COL32_B_SHIFT) & 0xFF);
    ImU8 a = (ImU8)(clamp01(a01) * 255.0f);
    return IM_COL32(r, g, b, a);
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty())
        return {};
    const int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (need <= 0)
        return {};
    std::string out(static_cast<size_t>(need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), need, nullptr, nullptr);
    return out;
}

static std::string BmpStemDisplayName(const std::string& bmpFilenameUtf8)
{
    std::string label = bmpFilenameUtf8;
    const size_t dot = label.rfind('.');
    if (dot != std::string::npos)
        label = label.substr(0, dot);
    return label;
}

static void RefreshBmpFileList(std::vector<std::string>& outUtf8)
{
    outUtf8.clear();
    const std::wstring dirW = Config::ExpandEnvW(Config::kBmpConfigDir);
    if (dirW.empty())
        return;

    const std::filesystem::path dir(dirW);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;
        std::wstring ext = entry.path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
        if (ext != L".bmp")
            continue;
        outUtf8.push_back(WideToUtf8(entry.path().filename().wstring()));
    }
    std::sort(outUtf8.begin(), outUtf8.end());
}

static ImVec4 u32_to_vec4(ImU32 c)
{
    ImU8 r = (ImU8)((c >> IM_COL32_R_SHIFT) & 0xFF);
    ImU8 g = (ImU8)((c >> IM_COL32_G_SHIFT) & 0xFF);
    ImU8 b = (ImU8)((c >> IM_COL32_B_SHIFT) & 0xFF);
    ImU8 a = (ImU8)((c >> IM_COL32_A_SHIFT) & 0xFF);
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

struct NexusTheme
{
    const char* name;

    ImU32 bgWash;

    ImU32 accent;
    ImU32 panelFill;
    ImU32 panelBorder;

    ImU32 titleFill;
    ImU32 titleLine;

    ImU32 boxFill;
    ImU32 boxBorder;

    ImU32 rowFill;
    ImU32 rowBorder;

    ImU32 textBrand;

    ImU32 nodePalette[4];
    ImU32 horizLine;
};

enum ThemeId
{
    THEME_ORIGINAL = 0,
    THEME_EMERALD = 1,
    THEME_NAVY_GOLD = 2,
    THEME_CHARCOAL_RED = 3,
    THEME_SLATE_VIOLET = 4,
    THEME_FROST_BLUE = 5,
    THEME_COUNT
};

static const NexusTheme g_themes[THEME_COUNT] =
{
    {
        "Original (Cyan/Purple)",
        IM_COL32(10, 12, 18, 255),
        IM_COL32(40, 220, 235, 255),
        IM_COL32(18, 20, 28, 190),
        IM_COL32(40, 220, 235, 55),
        IM_COL32(16, 18, 26, 150),
        IM_COL32(40, 220, 235, 70),
        IM_COL32(14, 16, 24, 150),
        IM_COL32(40, 220, 235, 55),
        IM_COL32(14, 16, 24, 135),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(210, 245, 250, 230),
        {
            IM_COL32(90,  80, 160, 255),
            IM_COL32(60, 160, 200, 255),
            IM_COL32(120,  70, 140, 255),
            IM_COL32(40, 110, 160, 255)
        },
        IM_COL32(255, 255, 255, 10)
    },

    {
        "Graphite + Emerald",
        IM_COL32(14, 15, 18, 255),
        IM_COL32(70, 230, 150, 255),
        IM_COL32(20, 22, 26, 205),
        IM_COL32(70, 230, 150, 55),
        IM_COL32(18, 20, 24, 170),
        IM_COL32(70, 230, 150, 80),
        IM_COL32(16, 18, 22, 150),
        IM_COL32(70, 230, 150, 45),
        IM_COL32(16, 18, 22, 135),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(220, 235, 230, 235),
        {
            IM_COL32(60, 220, 140, 255),
            IM_COL32(90, 200, 255, 255),
            IM_COL32(170, 180, 190, 255),
            IM_COL32(120, 140, 160, 255)
        },
        IM_COL32(255, 255, 255, 8)
    },

    {
        "Navy + Gold",
        IM_COL32(9, 12, 20, 255),
        IM_COL32(240, 200, 90, 255),
        IM_COL32(15, 18, 28, 210),
        IM_COL32(240, 200, 90, 55),
        IM_COL32(13, 16, 26, 175),
        IM_COL32(240, 200, 90, 85),
        IM_COL32(12, 15, 24, 155),
        IM_COL32(240, 200, 90, 45),
        IM_COL32(12, 15, 24, 140),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(240, 235, 220, 235),
        {
            IM_COL32(240, 200,  90, 255),
            IM_COL32(140, 180, 255, 255),
            IM_COL32(200, 170, 120, 255),
            IM_COL32(120, 140, 190, 255)
        },
        IM_COL32(255, 255, 255, 8)
    },

    {
        "Charcoal + Red",
        IM_COL32(12, 12, 13, 255),
        IM_COL32(255, 80, 90, 255),
        IM_COL32(18, 18, 20, 215),
        IM_COL32(255, 80, 90, 65),
        IM_COL32(16, 16, 18, 180),
        IM_COL32(255, 80, 90, 90),
        IM_COL32(15, 15, 17, 155),
        IM_COL32(255, 80, 90, 45),
        IM_COL32(15, 15, 17, 140),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(235, 230, 230, 235),
        {
            IM_COL32(255,  80,  90, 255),
            IM_COL32(255, 140,  80, 255),
            IM_COL32(180, 180, 190, 255),
            IM_COL32(120, 120, 130, 255)
        },
        IM_COL32(255, 255, 255, 8)
    },

    {
        "Slate + Violet",
        IM_COL32(13, 14, 18, 255),
        IM_COL32(170, 120, 255, 255),
        IM_COL32(19, 20, 26, 205),
        IM_COL32(170, 120, 255, 55),
        IM_COL32(17, 18, 24, 175),
        IM_COL32(170, 120, 255, 80),
        IM_COL32(16, 17, 22, 150),
        IM_COL32(170, 120, 255, 45),
        IM_COL32(16, 17, 22, 135),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(235, 230, 245, 235),
        {
            IM_COL32(170, 120, 255, 255),
            IM_COL32(120, 170, 255, 255),
            IM_COL32(200, 190, 220, 255),
            IM_COL32(110, 120, 160, 255)
        },
        IM_COL32(255, 255, 255, 8)
    },

    {
        "Black + Ice Blue",
        IM_COL32(6, 7, 10, 255),
        IM_COL32(120, 220, 255, 255),
        IM_COL32(12, 14, 18, 215),
        IM_COL32(120, 220, 255, 55),
        IM_COL32(10, 12, 16, 185),
        IM_COL32(120, 220, 255, 85),
        IM_COL32(10, 12, 16, 160),
        IM_COL32(120, 220, 255, 45),
        IM_COL32(10, 12, 16, 145),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(225, 235, 245, 235),
        {
            IM_COL32(160, 240, 255, 255),
            IM_COL32(90, 200, 255, 255),
            IM_COL32(200, 210, 230, 255),
            IM_COL32(120, 140, 160, 255)
        },
        IM_COL32(255, 255, 255, 8)
    }
};

static const NexusTheme& GetTheme()
{
    int idx = g_themeIndex;
    if (idx < 0) idx = 0;
    if (idx >= THEME_COUNT) idx = THEME_COUNT - 1;
    return g_themes[idx];
}

struct NexusNode
{
    ImVec2 pos;
    ImVec2 vel;
    float  r;
    ImU32  col;
};

static void DrawAnimatedBackground()
{
    const NexusTheme& th = GetTheme();

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    bg->AddRectFilled(ImVec2(0, 0), io.DisplaySize, th.bgWash);

    constexpr int   kNodeCount = 32;
    constexpr float kMinSpeed = 18.0f;
    constexpr float kMaxSpeed = 55.0f;
    constexpr float kMinR = 2.0f;
    constexpr float kMaxR = 4.5f;

    constexpr float kLinkDist = 150.0f;
    constexpr int   kMaxLinksPer = 4;
    constexpr float kLineMinA = 0.02f;
    constexpr float kLineMaxA = 0.18f;
    constexpr float kWrapMargin = 60.0f;
    constexpr float kCellSize = kLinkDist;
    constexpr int   kCircleSegments = 12;

    static bool inited = false;
    static int  lastTheme = -1;
    static std::vector<NexusNode> nodes;
    static std::vector<int> linkCount;
    static std::vector<std::vector<int>> buckets;

    const float dw = io.DisplaySize.x;
    const float dh = io.DisplaySize.y;

    if (!inited || lastTheme != g_themeIndex)
    {
        inited = true;
        lastTheme = g_themeIndex;

        nodes.clear();
        nodes.reserve(kNodeCount);

        for (int i = 0; i < kNodeCount; i++)
        {
            NexusNode n{};
            n.pos = ImVec2(frand01() * dw, frand01() * dh);

            float speed = kMinSpeed + frand01() * (kMaxSpeed - kMinSpeed);
            float ang = frand01() * 6.2831853f;
            n.vel = ImVec2(cosf(ang) * speed, sinf(ang) * speed);

            n.r = kMinR + frand01() * (kMaxR - kMinR);

            ImU32 base = th.nodePalette[rand() % 4];
            n.col = with_alpha(base, 0.35f);

            nodes.push_back(n);
        }
    }

    const float dt = (io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f);

    for (auto& n : nodes)
    {
        n.pos.x += n.vel.x * dt;
        n.pos.y += n.vel.y * dt;

        if (n.pos.x < -kWrapMargin) n.pos.x = dw + kWrapMargin;
        if (n.pos.x > dw + kWrapMargin) n.pos.x = -kWrapMargin;
        if (n.pos.y < -kWrapMargin) n.pos.y = dh + kWrapMargin;
        if (n.pos.y > dh + kWrapMargin) n.pos.y = -kWrapMargin;
    }

    const float linkDist2 = kLinkDist * kLinkDist;
    int numCols = (int)std::ceil(dw / kCellSize);
    int numRows = (int)std::ceil(dh / kCellSize);
    numCols = (numCols < 1) ? 1 : numCols;
    numRows = (numRows < 1) ? 1 : numRows;
    const int numCells = numCols * numRows;

    buckets.resize(static_cast<size_t>(numCells));
    for (auto& b : buckets)
        b.clear();

    auto cellIndex = [&](float x, float y) -> int {
        int cx = (int)(x / kCellSize);
        int cy = (int)(y / kCellSize);
        if (cx < 0) cx = 0; else if (cx >= numCols) cx = numCols - 1;
        if (cy < 0) cy = 0; else if (cy >= numRows) cy = numRows - 1;
        return cx + cy * numCols;
        };

    for (int i = 0; i < (int)nodes.size(); ++i)
        buckets[static_cast<size_t>(cellIndex(nodes[i].pos.x, nodes[i].pos.y))].push_back(i);

    linkCount.assign(nodes.size(), 0);

    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        if (linkCount[static_cast<size_t>(i)] >= kMaxLinksPer) continue;

        const int ci = cellIndex(nodes[i].pos.x, nodes[i].pos.y);
        const int cx = ci % numCols;
        const int cy = ci / numCols;

        for (int ny = -1; ny <= 1; ny++)
        {
            for (int nx = -1; nx <= 1; nx++)
            {
                const int ncx = cx + nx;
                const int ncy = cy + ny;
                if (ncx < 0 || ncy < 0 || ncx >= numCols || ncy >= numRows) continue;
                const int cidx = ncx + ncy * numCols;

                for (int j : buckets[static_cast<size_t>(cidx)])
                {
                    if (j <= i) continue;
                    if (linkCount[static_cast<size_t>(i)] >= kMaxLinksPer) break;
                    if (linkCount[static_cast<size_t>(j)] >= kMaxLinksPer) continue;

                    const float dx = nodes[j].pos.x - nodes[i].pos.x;
                    const float dy = nodes[j].pos.y - nodes[i].pos.y;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 > linkDist2 || d2 < 1e-6f) continue;

                    const float dist = sqrtf(d2);
                    const float t = 1.0f - clamp01(dist / kLinkDist);
                    const float a = lerp(kLineMinA, kLineMaxA, t);

                    const ImU32 lineCol = with_alpha(th.accent, a);
                    bg->AddLine(nodes[i].pos, nodes[j].pos, lineCol, 1.0f);
                    linkCount[static_cast<size_t>(i)]++;
                    linkCount[static_cast<size_t>(j)]++;
                }
            }
        }
    }

    const float tt = (float)ImGui::GetTime();
    for (auto& n : nodes)
    {
        const float pulse = 0.85f + 0.15f * sinf(tt * 0.9f + n.r * 10.0f);
        const float rr = n.r * pulse;
        bg->AddCircleFilled(n.pos, rr, n.col, kCircleSegments);
    }

    constexpr int lines = 8;
    for (int i = 0; i < lines; i++)
    {
        const float y = (dh / (float)(lines + 1)) * (float)(i + 1);
        bg->AddLine(ImVec2(0, y), ImVec2(dw, y), th.horizLine, 1.0f);
    }
}

static void ApplyNexusThemeStyle(const NexusTheme& th)
{
    static int s_appliedIndex = -999;
    if (s_appliedIndex == g_themeIndex)
        return;
    s_appliedIndex = g_themeIndex;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 10.0f;

    const ImVec4 Accent = u32_to_vec4(with_alpha(th.accent, 1.0f));
    const ImVec4 AccentSoft = u32_to_vec4(with_alpha(th.accent, 0.80f));

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.24f, 0.55f);
    c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.11f, 0.13f, 0.75f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.13f, 0.15f, 0.85f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.14f, 0.15f, 0.18f, 0.95f);
    c[ImGuiCol_Button] = ImVec4(0.10f, 0.11f, 0.13f, 0.70f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.13f, 0.15f, 0.85f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.14f, 0.15f, 0.18f, 0.95f);
    c[ImGuiCol_CheckMark] = Accent;
    c[ImGuiCol_SliderGrab] = AccentSoft;
    c[ImGuiCol_SliderGrabActive] = Accent;
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.92f, 1.00f);
}

static void HandleWindowDrag(GLFWwindow* window, const ImVec2& titleMin, const ImVec2& titleMax)
{
    ImVec2 dragMax = ImVec2(titleMax.x - 100.0f, titleMax.y);
    const bool hoveringDragZone = ImGui::IsMouseHoveringRect(titleMin, dragMax, false);

    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) return;

    static bool  dragging = false;
    static POINT grabOffset = { 0, 0 };

    if (!dragging)
    {
        if (hoveringDragZone &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemActive())
        {
            POINT cur{};
            GetCursorPos(&cur);

            RECT wr{};
            GetWindowRect(hwnd, &wr);

            grabOffset.x = cur.x - wr.left;
            grabOffset.y = cur.y - wr.top;

            dragging = true;
            SetCapture(hwnd);

            ImGui::GetIO().MouseDown[0] = false;
        }
    }

    if (dragging)
    {
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
        {
            dragging = false;
            ReleaseCapture();
            return;
        }

        POINT cur{};
        GetCursorPos(&cur);

        int newX = cur.x - grabOffset.x;
        int newY = cur.y - grabOffset.y;

        SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static void DrawNexusLeftPanel(const ImVec4& Accent, const ImVec2& leftMin, float leftInnerW, float leftInnerH)
{
    static std::string userLine;
    static bool userLineInit = false;
    if (!userLineInit) {
        userLineInit = true;
        userLine = "User: ";
        userLine += CachedWindowsUsernameUtf8();
    }

    ImGui::SetCursorScreenPos(ImVec2(leftMin.x + 14, leftMin.y + 14));
    ImGui::BeginGroup();
    ImGui::BeginChild("LeftPanelChild", ImVec2(leftInnerW, leftInnerH), false);

    if (ImGui::BeginTabBar("LeftTabs"))
    {
        if (ImGui::BeginTabItem("Status"))
        {
            ImGui::TextUnformatted("Status");
            ImGui::Separator();
            ImGui::TextUnformatted(userLine.c_str());
            ImGui::TextColored(Accent, "Online");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Config"))
        {
            ImGui::TextUnformatted("Keybinds");
            ImGui::Separator();

            const float keybindButtonWidth = 80.0f;
            KeybindWidget("Toggle script", &g_bindToggleScriptVK, keybindButtonWidth);
            KeybindWidget("Toggle console", &g_bindToggleConsoleVK, keybindButtonWidth);

            ImGui::Spacing();
            ImGui::TextUnformatted("Movement");
            ImGui::Separator();

            ImGui::SliderInt("Move Div", &g_moveDiv, 1, 10);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();
    ImGui::EndGroup();
}

void Nexus_GetThemeUiColors(NexusUiColors* out)
{
    if (!out)
        return;
    const NexusTheme& th = GetTheme();
    out->accent = th.accent;
    out->panelFill = th.panelFill;
    out->panelBorder = th.panelBorder;
    out->boxFill = th.boxFill;
    out->boxBorder = th.boxBorder;
    out->rowFill = th.rowFill;
    out->rowBorder = th.rowBorder;
    out->titleFill = th.titleFill;
    out->titleLine = th.titleLine;
    out->textBrand = th.textBrand;
}

static void DrawNexusBmpConfigBox(
    const NexusTheme& th,
    ImDrawList* dl,
    const ImVec2& rightMin,
    const ImVec2& rightMax,
    float contentH)
{
    constexpr float kRowDivisor = 7.0f;
    const float rowH = contentH / kRowDivisor;
    const float y0 = rightMin.y + rowH * 5.0f;

    const float rowPadX = 14.0f;
    const float rowPadY = 10.0f;

    ImVec2 boxMin = ImVec2(rightMin.x, y0 + 4.0f);
    ImVec2 boxMax = ImVec2(rightMax.x, rightMax.y - 2.0f);
    if (boxMax.y <= boxMin.y + 24.0f)
        return;

    dl->AddRectFilled(boxMin, boxMax, th.boxFill, 12.0f);
    dl->AddRect(boxMin, boxMax, th.boxBorder, 12.0f, 0, 1.5f);

    static std::vector<std::string> s_bmpList;
    RefreshBmpFileList(s_bmpList);

    if (!s_bmpList.empty()) {
        if (g_selectedBmpConfigIndex >= (int)s_bmpList.size())
            g_selectedBmpConfigIndex = (int)s_bmpList.size() - 1;
    }

    std::string currentLabel = "(no BMP selected)";
    if (!s_bmpList.empty() && g_selectedBmpConfigIndex >= 0 &&
        g_selectedBmpConfigIndex < (int)s_bmpList.size())
        currentLabel = BmpStemDisplayName(s_bmpList[static_cast<size_t>(g_selectedBmpConfigIndex)]);

    float frameH = ImGui::GetFrameHeight();
    float innerH = (boxMax.y - boxMin.y) - (rowPadY * 2.0f);
    float y = boxMin.y + rowPadY + (innerH - frameH) * 0.5f;
    float labelX = boxMin.x + rowPadX;
    constexpr float labelW = 190.0f;
    constexpr float colGap = 14.0f;
    float controlX = labelX + labelW + colGap;

    ImGui::SetCursorScreenPos(ImVec2(labelX, y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("BMP config");

    ImGui::SetCursorScreenPos(ImVec2(controlX, y));
    if (ImGui::Button("Config selection menu"))
        g_scriptConfigWindowOpen = true;

    ImGui::SameLine(0, 12.0f);
    ImGui::TextUnformatted(currentLabel.c_str());
}

static void DrawNexusRightRows(
    const NexusTheme& th,
    ImDrawList* dl,
    const ImVec2& rightMin,
    const ImVec2& rightMax,
    float contentH)
{
    constexpr int kVisibleRows = 5;
    constexpr float kRowDivisor = 7.0f;
    const float rowH = contentH / kRowDivisor;

    static const char* comboItems2[THEME_COUNT] =
    {
        g_themes[0].name, g_themes[1].name, g_themes[2].name,
        g_themes[3].name, g_themes[4].name, g_themes[5].name
    };

    const float rowPadX = 14.0f;
    const float rowPadY = 10.0f;
    const float labelW = 190.0f;
    const float colGap = 14.0f;

    for (int i = 0; i < kVisibleRows; i++)
    {
        const float y0 = rightMin.y + rowH * (float)i;
        const float y1 = y0 + rowH;

        ImVec2 rMin = ImVec2(rightMin.x, y0);
        ImVec2 rMax = ImVec2(rightMax.x, y1);

        ImVec2 boxMin = ImVec2(rMin.x, rMin.y + 4);
        ImVec2 boxMax = ImVec2(rMax.x, rMax.y - 6);

        dl->AddRectFilled(boxMin, boxMax, th.rowFill, 12.0f);
        dl->AddRect(boxMin, boxMax, th.rowBorder, 12.0f);

        float frameH = ImGui::GetFrameHeight();
        float innerH = (boxMax.y - boxMin.y) - (rowPadY * 2.0f);
        float y = boxMin.y + rowPadY + (innerH - frameH) * 0.5f;

        float labelX = boxMin.x + rowPadX;
        float controlX = labelX + labelW + colGap;
        float controlW = (boxMax.x - rowPadX) - controlX;

        ImGui::PushID(i);

        const char* label = "";
        enum { CTRL_SLIDER, CTRL_COMBO } ctrl = CTRL_SLIDER;

        int* sptr = nullptr;
        int* iptr = nullptr;
        const char* const* items = nullptr;
        int itemsCount = 0;

        switch (i)
        {
        case 0: label = "Down Force";  ctrl = CTRL_SLIDER; sptr = &g_downForce;   break;
        case 1: label = "Right Force"; ctrl = CTRL_SLIDER; sptr = &g_rightDrift; break;
        case 2: label = "Left Force";  ctrl = CTRL_SLIDER; sptr = &g_leftDrift;  break;
        case 3: label = "Delay Time";  ctrl = CTRL_SLIDER; sptr = &g_delayMs;     break;
        case 4:
            label = "Color Scheme";
            ctrl = CTRL_COMBO;
            iptr = &g_themeIndex;
            items = comboItems2;
            itemsCount = THEME_COUNT;
            break;
        }

        ImGui::SetCursorScreenPos(ImVec2(labelX, y));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::SetCursorScreenPos(ImVec2(controlX, y));

        if (ctrl == CTRL_SLIDER)
        {
            ImGui::SetNextItemWidth(controlW);
            ImGui::SliderInt("##v", sptr, 0, 100, "%d");

            if (&g_rightDrift == sptr && g_rightDrift > 0)
                g_leftDrift = 0;

            if (&g_leftDrift == sptr && g_leftDrift > 0)
                g_rightDrift = 0;
        }
        else
        {
            ImGui::SetNextItemWidth(controlW);
            if (ImGui::Combo("##v", iptr, items, itemsCount))
            {
                if (g_themeIndex < 0) g_themeIndex = 0;
                if (g_themeIndex >= THEME_COUNT) g_themeIndex = THEME_COUNT - 1;
                if (!SaveUiSettingsToJson(nullptr))
                    LOGW("SaveUiSettingsToJson failed");
            }
        }

        ImGui::PopID();
    }
}

void RenderNexusUltimate(GLFWwindow* window, bool* p_open)
{
    DrawAnimatedBackground();

    const NexusTheme& th = GetTheme();
    ApplyNexusThemeStyle(th);

    const ImVec4 Accent = u32_to_vec4(with_alpha(th.accent, 1.0f));

    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("NexusUltimate_Main", p_open, flags))
    {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    const float rounding = 14.0f;
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), th.panelFill, rounding);
    dl->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), th.panelBorder, rounding, 0, 2.0f);

    const float pad = 18.0f;
    const float titleH = 52.0f;
    const float gap = 14.0f;

    ImVec2 innerMin = ImVec2(wp.x + pad, wp.y + pad);
    ImVec2 innerMax = ImVec2(wp.x + ws.x - pad, wp.y + ws.y - pad);

    ImVec2 titleMin = innerMin;
    ImVec2 titleMax = ImVec2(innerMax.x, innerMin.y + titleH);

    dl->AddRectFilled(titleMin, titleMax, th.titleFill, 12.0f);
    dl->AddRect(titleMin, titleMax, th.titleLine, 12.0f, 0, 1.5f);
    dl->AddText(ImVec2(titleMin.x + 14.0f, titleMin.y + 15.0f), th.textBrand, "Nexus Ultimate");

    ImGui::SetCursorScreenPos(ImVec2(titleMax.x - 90.0f, titleMin.y + 12.0f));
    ImGui::PushID("title_buttons");

    auto TitleButton = [&](const char* id, ImU32 fill, ImU32 stroke) -> bool
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 sz(30, 26);

            bool pressed = ImGui::InvisibleButton(id, sz);
            bool hovered = ImGui::IsItemHovered();

            ImU32 f = hovered ? IM_COL32(24, 26, 30, 220) : fill;

            dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), f, 8.0f);
            dl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), stroke, 8.0f, 0, 1.2f);

            ImGui::SameLine(0, 10.0f);
            return pressed;
        };

    static bool minimized = false;

    bool minPressed = TitleButton("min", IM_COL32(16, 18, 22, 190), IM_COL32(255, 255, 255, 35));
    {
        ImVec2 b = ImGui::GetItemRectMin();
        ImVec2 e = ImGui::GetItemRectMax();
        ImVec2 center((b.x + e.x) * 0.5f, (b.y + e.y) * 0.5f);
        dl->AddLine(ImVec2(center.x - 7, center.y + 5),
            ImVec2(center.x + 7, center.y + 5),
            IM_COL32(230, 235, 235, 190), 2.0f);
    }

    bool closePressed = TitleButton("close", IM_COL32(16, 18, 22, 190), IM_COL32(255, 120, 120, 70));
    {
        ImVec2 b = ImGui::GetItemRectMin();
        ImVec2 e = ImGui::GetItemRectMax();
        ImU32 xcol = IM_COL32(255, 170, 170, 215);
        dl->AddLine(ImVec2(b.x + 9, b.y + 7), ImVec2(e.x - 9, e.y - 7), xcol, 2.0f);
        dl->AddLine(ImVec2(b.x + 9, e.y - 7), ImVec2(e.x - 9, b.y + 7), xcol, 2.0f);
    }
    if (closePressed)
    {
        std::exit(0);
    }

    ImGui::PopID();

    if (minPressed) minimized = !minimized;

    HandleWindowDrag(window, titleMin, titleMax);

    ImVec2 contentMin = ImVec2(innerMin.x, titleMax.y + gap);
    ImVec2 contentMax = innerMax;

    float contentW = contentMax.x - contentMin.x;
    float contentH = contentMax.y - contentMin.y;
    float leftW = contentW * 0.25f;

    ImVec2 leftMin = contentMin;
    ImVec2 leftMax = ImVec2(contentMin.x + leftW, contentMax.y);

    ImVec2 rightMin = ImVec2(leftMax.x + gap, contentMin.y);
    ImVec2 rightMax = contentMax;

    dl->AddRectFilled(leftMin, leftMax, th.boxFill, 12.0f);
    dl->AddRect(leftMin, leftMax, th.boxBorder, 12.0f, 0, 1.5f);

    const float leftInnerW = (leftMax.x - leftMin.x) - 28.0f;
    const float leftInnerH = (leftMax.y - leftMin.y) - 28.0f;

    DrawNexusLeftPanel(Accent, leftMin, leftInnerW, leftInnerH);

    if (minimized)
    {
        dl->AddRectFilled(contentMin, contentMax, IM_COL32(0, 0, 0, 120), 12.0f);
        ImGui::End();
        return;
    }

    DrawNexusRightRows(th, dl, rightMin, rightMax, contentH);
    DrawNexusBmpConfigBox(th, dl, rightMin, rightMax, contentH);

    ImGui::End();
}
