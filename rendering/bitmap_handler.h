#pragma once

#include "pch.h"
#include "globals.h"

namespace Paths {

    namespace fs = std::filesystem;

    inline fs::path ExeDir() {
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);

        fs::path p(buf);
        return p.parent_path();
    }

    inline bool HasBmpExtension(const fs::path& p) {
        std::wstring ext = p.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return ext == L".bmp";
    }

    inline fs::path PickBmpFolder() {
        const fs::path cwd = fs::current_path();

        const fs::path a = cwd / L"bmp";
        if (fs::exists(a) && fs::is_directory(a)) return a;

        const fs::path b = ExeDir() / L"bmp";
        if (fs::exists(b) && fs::is_directory(b)) return b;

        return a;
    }
    static bool IEquals(const std::wstring& a, const std::wstring& b) {
        if (a.size() != b.size()) return false;

        for (size_t i = 0; i < a.size(); ++i) {
            const wchar_t ca = static_cast<wchar_t>(towlower(a[i]));
            const wchar_t cb = static_cast<wchar_t>(towlower(b[i]));
            if (ca != cb) return false;
        }
        return true;
    }
    inline bool IsExcludedName(
        const std::wstring& filenameOnly,
        const wchar_t* const* excludedNames,
        size_t excludedCount
    ) {
        for (size_t i = 0; i < excludedCount; ++i) {
            if (IEquals(filenameOnly, excludedNames[i])) return true;
        }
        return false;
    }

    inline void ClearImages(
        std::vector<Gdiplus::Bitmap*>& bitmaps,
        std::vector<std::wstring>& paths,
        std::vector<Gdiplus::Bitmap*>& displayBitmaps,
        std::vector<std::wstring>& displayPaths,
        int& selectedIndex
    ) {
        for (auto* b : bitmaps) delete b;

        bitmaps.clear();
        paths.clear();
        displayBitmaps.clear();
        displayPaths.clear();
        selectedIndex = -1;
    }

} // namespace Paths
