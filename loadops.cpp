#include "pch.h"
#include "json_parsing/loadops.h"

#include "config.h"
#include "globals.h"

using json = nlohmann::json;

#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <windows.h>

static constexpr int kDelayMin = 1;
static constexpr int kDelayMax = 50;

std::filesystem::path ExpandPath(const wchar_t* macroPath) {
    const std::wstring expanded = Config::ExpandEnvW(macroPath);
    return std::filesystem::path(expanded);
}

std::filesystem::path OpsPath() {
    return ExpandPath(Config::kOpsPath);
}

std::filesystem::path KeybindsPath() {
    return ExpandPath(Config::kKeybindPath);
}

std::filesystem::path UiSettingsPath() {
    return ExpandPath(Config::kUiSettingsPath);
}

std::string ReadWholeFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

bool WriteFileAtomic(const std::filesystem::path& target, const std::string& bytes) {
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);

    const auto tmp = target.parent_path() / (target.filename().wstring() + L".tmp");

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;

        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        f.flush();
        if (!f) return false;
    }

    std::filesystem::rename(tmp, target, ec);
    if (!ec) return true;

    std::filesystem::remove(target, ec);
    ec.clear();
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }

    return true;
}

bool LoadOpsJson(json& out) {
    const auto p = OpsPath();

    std::ifstream f(p, std::ios::binary);
    if (!f) {
        LOGE("LoadOpsJson: cannot open ops.json");
        return false;
    }

    const std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    try {
        out = json::parse(content);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("LoadOpsJson: parse failed (%s)", e.what());
        return false;
    }
    catch (...) {
        LOGE("LoadOpsJson: parse failed (unknown)");
        return false;
    }
}

bool SaveOpsJson(const json& j) {
    const auto p = OpsPath();
    const std::string dumped = j.dump(2);

    if (!WriteFileAtomic(p, dumped)) {
        LOGE("SaveOpsJson: write/replace failed");
        return false;
    }

    return true;
}

bool ValidateOpObject(const json& item, std::string* err) {
    if (!item.is_object()) { if (err) *err = "entry is not an object"; return false; }

    for (auto it = item.begin(); it != item.end(); ++it) {
        const std::string& k = it.key();
        if (k == "name" || k == "s" || k == "e" || k == "w" || k == "delay") continue;
        if (err) *err = "invalid key: " + k;
        return false;
    }

    if (!item.contains("name") || !item["name"].is_string()) {
        if (err) *err = "missing/invalid name";
        return false;
    }

    if (!item.contains("s") || !(item["s"].is_number_float() || item["s"].is_number_integer())) {
        if (err) *err = "missing/invalid s";
        return false;
    }

    if (!item.contains("delay") || !item["delay"].is_number_integer()) {
        if (err) *err = "missing/invalid delay";
        return false;
    }

    const int d = item["delay"].get<int>();
    if (d < kDelayMin || d > kDelayMax) {
        if (err) *err = "delay must be 1..50";
        return false;
    }

    const bool hasE = item.contains("e");
    const bool hasW = item.contains("w");
    if (hasE == hasW) {
        if (err) *err = "must have exactly one of e or w";
        return false;
    }

    if (hasE && !(item["e"].is_number_float() || item["e"].is_number_integer())) {
        if (err) *err = "invalid e";
        return false;
    }
    if (hasW && !(item["w"].is_number_float() || item["w"].is_number_integer())) {
        if (err) *err = "invalid w";
        return false;
    }

    return true;
}

bool FindOperator(const json& arr, const std::string& name, const json** outItem) {
    if (!arr.is_array()) return false;

    for (const auto& item : arr) {
        if (!item.is_object()) continue;

        auto itName = item.find("name");
        if (itName == item.end() || !itName->is_string()) continue;

        if (itName->get<std::string>() == name) {
            if (outItem) *outItem = &item;
            return true;
        }
    }

    return false;
}

bool ExtractMoveFields(
    const json& item,
    double& outS,
    char& outDir,
    double& outStep,
    int& outDelay,
    std::string* outErr
) {
    std::string err;
    if (!ValidateOpObject(item, &err)) {
        if (outErr) *outErr = err;
        return false;
    }

    outS = item["s"].get<double>();
    outDelay = item["delay"].get<int>();

    if (item.contains("e")) {
        outDir = 'E';
        outStep = item["e"].get<double>();
    }
    else {
        outDir = 'W';
        outStep = item["w"].get<double>();
    }

    if (outErr) outErr->clear();
    return true;
}

bool LoadKeybindsFromJson(std::string* outErr) {
    if (outErr) outErr->clear();

    const auto p = KeybindsPath();
    if (p.empty()) {
        if (outErr) *outErr = "ExpandEnvironmentStrings failed for keybind path";
        return false;
    }

    if (!std::filesystem::exists(p))
        return true;

    const std::string content = ReadWholeFile(p);
    if (content.empty())
        return true;

    json j;
    try {
        j = json::parse(content);
    }
    catch (const std::exception& e) {
        if (outErr) *outErr = std::string("Parse failed: ") + e.what();
        return false;
    }
    catch (...) {
        if (outErr) *outErr = "Parse failed (unknown)";
        return false;
    }

    if (!j.is_object()) return true;

    auto bindsIt = j.find("binds");
    if (bindsIt != j.end() && bindsIt->is_object()) {
        const json& b = *bindsIt;

        auto getInt = [&](const char* key, int& dst) {
            auto it = b.find(key);
            if (it != b.end() && it->is_number()) dst = it->get<int>();
            };

        getInt("toggle_script", g_bindToggleScriptVK);
        getInt("quit", g_bindQuitVK);
        getInt("toggle_console", g_bindToggleConsoleVK);
        getInt("reload", g_bindReloadVK);
        getInt("hold_mouse1", g_bindHoldMouse1VK);
        getInt("hold_mouse2", g_bindHoldMouse2VK);
        getInt("hold_modifier", g_bindHoldModifierVK);
    }

    auto flagsIt = j.find("flags");
    if (flagsIt != j.end() && flagsIt->is_object()) {
        const json& fl = *flagsIt;
        auto it = fl.find("require_both_mouse_buttons");
        if (it != fl.end() && it->is_boolean()) g_requireBothMouseButtons = it->get<bool>();
    }

    auto clampNonNegative = [](int& v) { if (v < 0) v = 0; };

    clampNonNegative(g_bindHoldModifierVK);
    clampNonNegative(g_bindToggleScriptVK);
    clampNonNegative(g_bindQuitVK);
    clampNonNegative(g_bindToggleConsoleVK);
    clampNonNegative(g_bindReloadVK);
    clampNonNegative(g_bindHoldMouse1VK);
    clampNonNegative(g_bindHoldMouse2VK);

    return true;
}

bool SaveKeybindsToJson(std::string* outErr) {
    if (outErr) outErr->clear();

    const auto p = KeybindsPath();
    if (p.empty()) {
        if (outErr) *outErr = "ExpandEnvironmentStrings failed for keybind path";
        return false;
    }

    json j;
    j["version"] = 1;
    j["binds"] = {
        {"toggle_script",  g_bindToggleScriptVK},
        {"quit",           g_bindQuitVK},
        {"toggle_console", g_bindToggleConsoleVK},
        {"reload",         g_bindReloadVK},
        {"hold_mouse1",    g_bindHoldMouse1VK},
        {"hold_mouse2",    g_bindHoldMouse2VK},
        {"hold_modifier",  g_bindHoldModifierVK},
    };
    j["flags"] = {
        {"require_both_mouse_buttons", g_requireBothMouseButtons},
    };

    const std::string pretty = j.dump(2);
    if (!WriteFileAtomic(p, pretty)) {
        if (outErr) *outErr = "Write/replace keybinds.json failed";
        return false;
    }

    LOGI("Saved keybinds to %s", p.u8string().c_str());
    return true;
}

bool LoadUiSettingsFromJson(std::string* outErr) {
    if (outErr) outErr->clear();

    constexpr int kThemeSlotCount = 6;

    const auto p = UiSettingsPath();
    if (p.empty()) {
        if (outErr) *outErr = "ExpandEnvironmentStrings failed for ui.json path";
        return false;
    }

    if (!std::filesystem::exists(p))
        return true;

    const std::string content = ReadWholeFile(p);
    if (content.empty())
        return true;

    json j;
    try {
        j = json::parse(content);
    }
    catch (const std::exception& e) {
        if (outErr) *outErr = std::string("Parse failed: ") + e.what();
        return false;
    }
    catch (...) {
        if (outErr) *outErr = "Parse failed (unknown)";
        return false;
    }

    if (!j.is_object()) return true;

    auto it = j.find("theme_index");
    if (it != j.end() && it->is_number()) {
        int ti = it->get<int>();
        if (ti < 0) ti = 0;
        if (ti >= kThemeSlotCount) ti = kThemeSlotCount - 1;
        g_themeIndex = ti;
    }

    return true;
}

bool SaveUiSettingsToJson(std::string* outErr) {
    if (outErr) outErr->clear();

    constexpr int kThemeSlotCount = 6;

    const auto p = UiSettingsPath();
    if (p.empty()) {
        if (outErr) *outErr = "ExpandEnvironmentStrings failed for ui.json path";
        return false;
    }

    int ti = g_themeIndex;
    if (ti < 0) ti = 0;
    if (ti >= kThemeSlotCount) ti = kThemeSlotCount - 1;

    json j;
    j["version"] = 1;
    j["theme_index"] = ti;

    const std::string pretty = j.dump(2);
    if (!WriteFileAtomic(p, pretty)) {
        if (outErr) *outErr = "Write/replace ui.json failed";
        return false;
    }

    LOGI("Saved ui settings to %s", p.u8string().c_str());
    return true;
}

bool GetMoveForOperator(const std::string& name, double& outS, char& outDir, double& outStep, int& outDelay) {
    json j;
    if (!LoadOpsJson(j)) return false;

    const json* item = nullptr;
    if (!FindOperator(j, name, &item) || !item) return false;

    std::string err;
    if (!ExtractMoveFields(*item, outS, outDir, outStep, outDelay, &err)) {
        LOGW("GetMoveForOperator: invalid entry for '%s': %s", name.c_str(), err.c_str());
        return false;
    }

    return true;
}

static std::string Utf8LowerAscii(std::string s)
{
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

bool GetMoveForOperatorByBmpStem(
    const std::string& bmpFilenameUtf8,
    double& outS,
    char& outDir,
    double& outStep,
    int& outDelay,
    std::string* outCanonicalName
)
{
    if (outCanonicalName)
        outCanonicalName->clear();

    std::string stem = bmpFilenameUtf8;
    const size_t dot = stem.rfind('.');
    if (dot != std::string::npos)
        stem = stem.substr(0, dot);
    const std::string stemLower = Utf8LowerAscii(stem);

    json j;
    if (!LoadOpsJson(j) || !j.is_array())
        return false;

    for (const auto& item : j) {
        if (!item.is_object())
            continue;
        auto itName = item.find("name");
        if (itName == item.end() || !itName->is_string())
            continue;
        const std::string n = itName->get<std::string>();
        if (Utf8LowerAscii(n) != stemLower)
            continue;

        std::string err;
        if (!ExtractMoveFields(item, outS, outDir, outStep, outDelay, &err)) {
            LOGW("GetMoveForOperatorByBmpStem: invalid entry for '%s': %s", n.c_str(), err.c_str());
            return false;
        }
        if (outCanonicalName)
            *outCanonicalName = n;
        return true;
    }

    return false;
}

bool SaveOperatorToOpsJson(
    const std::string& operatorName,
    int delayMs,
    double southS,
    char ewDir,
    double ewStep,
    std::string* outErr)
{
    if (outErr)
        outErr->clear();

    json j;
    if (!LoadOpsJson(j)) {
        if (outErr)
            *outErr = "cannot load ops.json";
        return false;
    }

    if (!j.is_array()) {
        if (outErr)
            *outErr = "ops.json must be a JSON array";
        return false;
    }

    const char dir = (char)std::toupper((unsigned char)ewDir);
    if (dir != 'E' && dir != 'W') {
        if (outErr)
            *outErr = "direction must be E or W";
        return false;
    }

    bool found = false;
    for (auto& item : j) {
        if (!item.is_object())
            continue;
        auto itName = item.find("name");
        if (itName == item.end() || !itName->is_string())
            continue;
        if (itName->get<std::string>() != operatorName)
            continue;

        item["delay"] = delayMs;
        item["s"] = southS;
        if (dir == 'E') {
            item.erase("w");
            item["e"] = ewStep;
        }
        else {
            item.erase("e");
            item["w"] = ewStep;
        }

        std::string err;
        if (!ValidateOpObject(item, &err)) {
            if (outErr)
                *outErr = err;
            return false;
        }
        found = true;
        break;
    }

    if (!found) {
        if (outErr)
            *outErr = "operator not found: " + operatorName;
        return false;
    }

    if (!SaveOpsJson(j)) {
        if (outErr)
            *outErr = "failed to write ops.json";
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(g_selMutex);
        if (g_selectedOpName == operatorName)
            g_reloadMoveFromOps.store(true, std::memory_order_relaxed);
    }

    LOGI("Saved operator '%s' to ops.json", operatorName.c_str());
    return true;
}
