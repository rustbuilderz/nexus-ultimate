#pragma once

#include <windows.h>
#include <string>

namespace Config {

    inline constexpr const wchar_t* kOpsPath =
        L"%LOCALAPPDATA%\\nexus_ultimate\\ops.json";

    inline constexpr const wchar_t* kKeybindPath =
        L"%LOCALAPPDATA%\\nexus_ultimate\\keybinds.json";

    inline constexpr const wchar_t* kUiSettingsPath =
        L"%LOCALAPPDATA%\\nexus_ultimate\\ui.json";

    inline constexpr const wchar_t* kBmpConfigDir =
        L"%LOCALAPPDATA%\\nexus_ultimate\\bmp";

    inline std::wstring ExpandEnvW(const wchar_t* s) {
        const DWORD need = ExpandEnvironmentStringsW(s, nullptr, 0);
        if (need == 0) return {};

        std::wstring out;
        out.resize(static_cast<size_t>(need - 1));

        const DWORD wrote = ExpandEnvironmentStringsW(s, out.data(), need);
        if (wrote == 0) return {};

        return out;
    }

}
