#pragma once

#include <windows.h>
#include <string>

namespace Config {

    inline constexpr const wchar_t* kComPort = L"\\\\.\\COM7";
    inline constexpr const wchar_t* kOpsPath = L"%USERPROFILE%\\nexus_ultimate\\ops.json";
    inline constexpr const wchar_t* kKeybindPath = L"%USERPROFILE%\\nexus_ultimate\\keybinds.json";

    inline std::wstring ExpandEnvW(const wchar_t* s) {
        const DWORD need = ExpandEnvironmentStringsW(s, nullptr, 0);
        if (need == 0) return {};

        std::wstring out;
        out.resize(static_cast<size_t>(need - 1)); // exclude null terminator

        const DWORD wrote = ExpandEnvironmentStringsW(s, out.data(), need);
        if (wrote == 0) return {};

        return out;
    }

} // namespace Config
