#pragma once

#include "pch.h"

namespace Input {

    inline const char* VkToName(int vk) {
        switch (vk) {
        case 0:            return "(none)";
        case VK_LBUTTON:   return "Mouse1 (LMB)";
        case VK_RBUTTON:   return "Mouse2 (RMB)";
        case VK_MBUTTON:   return "Mouse3 (MMB)";
        case VK_XBUTTON1:  return "Mouse4";
        case VK_XBUTTON2:  return "Mouse5";
        case VK_BACK:      return "Backspace";
        case VK_TAB:       return "Tab";
        case VK_RETURN:    return "Enter";
        case VK_SHIFT:     return "Shift";
        case VK_CONTROL:   return "Ctrl";
        case VK_MENU:      return "Alt";
        case VK_PAUSE:     return "Pause";
        case VK_CAPITAL:   return "CapsLock";
        case VK_ESCAPE:    return "Esc";
        case VK_SPACE:     return "Space";
        case VK_PRIOR:     return "PageUp";
        case VK_NEXT:      return "PageDown";
        case VK_END:       return "End";
        case VK_HOME:      return "Home";
        case VK_LEFT:      return "Left";
        case VK_UP:        return "Up";
        case VK_RIGHT:     return "Right";
        case VK_DOWN:      return "Down";
        case VK_INSERT:    return "Insert";
        case VK_DELETE:    return "Delete";
        case VK_F1:        return "F1";
        case VK_F2:        return "F2";
        case VK_F3:        return "F3";
        case VK_F4:        return "F4";
        case VK_F5:        return "F5";
        case VK_F6:        return "F6";
        case VK_F7:        return "F7";
        case VK_F8:        return "F8";
        case VK_F9:        return "F9";
        case VK_F10:       return "F10";
        case VK_F11:       return "F11";
        case VK_F12:       return "F12";
        default: break;
        }

        if (vk >= 'A' && vk <= 'Z') {
            static thread_local char s[2]{};
            s[0] = static_cast<char>(vk);
            s[1] = '\0';
            return s;
        }

        if (vk >= '0' && vk <= '9') {
            static thread_local char s[2]{};
            s[0] = static_cast<char>(vk);
            s[1] = '\0';
            return s;
        }

        return "(vk)";
    }

    inline bool CaptureVkOnce(int& outVk, bool allowMouseButtons = true) {
        auto pressedThisFrame = [](int vk) -> bool {
            return (GetAsyncKeyState(vk) & 1) != 0;
            };

        if (allowMouseButtons) {
            if (pressedThisFrame(VK_LBUTTON)) { outVk = VK_LBUTTON;  return true; }
            if (pressedThisFrame(VK_RBUTTON)) { outVk = VK_RBUTTON;  return true; }
            if (pressedThisFrame(VK_MBUTTON)) { outVk = VK_MBUTTON;  return true; }
            if (pressedThisFrame(VK_XBUTTON1)) { outVk = VK_XBUTTON1; return true; }
            if (pressedThisFrame(VK_XBUTTON2)) { outVk = VK_XBUTTON2; return true; }
        }

        for (int vk = 8; vk <= 0xFE; ++vk) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
                vk == VK_XBUTTON1 || vk == VK_XBUTTON2) {
                continue;
            }

            if (pressedThisFrame(vk)) {
                outVk = vk;
                return true;
            }
        }

        return false;
    }

} // namespace Input
