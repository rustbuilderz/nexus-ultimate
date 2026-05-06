#pragma once

#include "pch.h"
#include "imgui/imgui.h"
namespace Input {

    static const char* VkToNameViaMapVirtualKeyA(int vk, char* out, size_t outSz)
    {
        if (!out || outSz == 0) return "";

        if (vk <= 0)
        {
            strcpy_s(out, outSz, "None");
            return out;
        }

        UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);

        switch (vk)
        {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT:
        case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE:
        case VK_DIVIDE: case VK_NUMLOCK:
            scan |= 0xE000;
            break;
        default:
            break;
        }

        LONG lParam = (LONG)(scan << 16);
        int len = GetKeyNameTextA(lParam, out, (int)outSz);
        if (len <= 0)
            strcpy_s(out, outSz, "Unknown");

        return out;
    }
    inline void SnapshotKeyAsyncState(SHORT* outPrev256)
    {
        for (int i = 1; i <= 254; ++i)
            outPrev256[i] = GetAsyncKeyState(i);
        outPrev256[0] = 0;
        outPrev256[255] = 0;
    }

    static void KeybindWidget(
        const char* label,
        int* bindVK,
        float buttonWidth = 160.0f,
        float buttonHeight = 0.0f
    )
    {
        if (!bindVK) return;

        static int* s_captureTarget = nullptr;
        static SHORT s_prev[256] = {};

        ImGui::PushID(bindVK);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        char nameBuf[64]{};
        VkToNameViaMapVirtualKeyA(*bindVK, nameBuf, sizeof(nameBuf));

        char btnText[96]{};
        if (s_captureTarget == bindVK)
            strcpy_s(btnText, "Press a key...");
        else
            sprintf_s(btnText, "%s", nameBuf);

        if (ImGui::Button(btnText, ImVec2(buttonWidth, buttonHeight)))
        {
            if (s_captureTarget == bindVK)
                s_captureTarget = nullptr;
            else
            {
                s_captureTarget = bindVK;
                SnapshotKeyAsyncState(s_prev);
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            *bindVK = 0;
            if (s_captureTarget == bindVK) s_captureTarget = nullptr;
            if (!SaveKeybindsToJson(nullptr))
                LOGW("SaveKeybindsToJson failed");
        }

        if (s_captureTarget == bindVK)
        {
            for (int vk = 1; vk <= 254; ++vk)
            {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON)
                    continue;

                SHORT cur = GetAsyncKeyState(vk);
                bool downNow = (cur & 0x8000) != 0;
                bool downPrev = (s_prev[vk] & 0x8000) != 0;

                if (downNow && !downPrev)
                {
                    if (vk == VK_ESCAPE)
                    {
                        s_captureTarget = nullptr;
                        break;
                    }
                    *bindVK = vk;
                    s_captureTarget = nullptr;
                    if (!SaveKeybindsToJson(nullptr))
                        LOGW("SaveKeybindsToJson failed");
                    break;
                }

                s_prev[vk] = cur;
            }

            ImGui::SameLine();
            ImGui::TextDisabled("(Esc cancel, RMB clear)");
        }

        ImGui::PopID();
    }

}

