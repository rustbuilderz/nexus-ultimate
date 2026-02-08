#include "pch.h"
#include "json_parsing/loadops.h"

#include "config.h"
#include "globals.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <windows.h>

namespace {
    constexpr int kDelayMin = 1;
    constexpr int kDelayMax = 50;

    std::filesystem::path ExpandPath(const wchar_t* macroPath) {
        const std::wstring expanded = Config::ExpandEnvW(macroPath);
        return std::filesystem::path(expanded);
    }

    std::string ReadFileBinary(const std::filesystem::path& p) {
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

    int ClampDelay(int d) {
        if (d < kDelayMin) return kDelayMin;
        if (d > kDelayMax) return kDelayMax;
        return d;
    }

    // Shared validation: allowed keys and required fields
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

    bool ApplyMoveToGlobals(double s, char dir, double step, int delay) {
        g_downForce = MoveMath::ForceJsonToUi(static_cast<float>(s));
        g_delayMs = ClampDelay(delay);

        if (dir == 'E') {
            g_rightDrift = MoveMath::ForceJsonToUi(static_cast<float>(step));
            g_leftDrift = 0.0f;
        }
        else {
            g_leftDrift = MoveMath::ForceJsonToUi(static_cast<float>(step));
            g_rightDrift = 0.0f;
        }

        return true;
    }

    std::filesystem::path OpsPath() {
        return ExpandPath(Config::kOpsPath);
    }

    std::filesystem::path KeybindsPath() {
        return ExpandPath(Config::kKeybindPath);
    }

} // namespace

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

bool LoadUiForOperator(const std::string& name) {
    json j;
    if (!LoadOpsJson(j)) return false;

    const json* item = nullptr;
    if (!FindOperator(j, name, &item) || !item) return false;

    double s = 0.0;
    double step = 0.0;
    int delay = 0;
    char dir = 'E';

    std::string err;
    if (!ExtractMoveFields(*item, s, dir, step, delay, &err)) return false;

    return ApplyMoveToGlobals(s, dir, step, delay);
}

bool ApplyUiFromOperatorJson(const std::string& name, std::string* outErr) {
    if (outErr) outErr->clear();

    json j;
    if (!LoadOpsJson(j)) {
        if (outErr) *outErr = "ops.json failed to load";
        return false;
    }
    if (!j.is_array()) {
        if (outErr) *outErr = "ops.json is not an array";
        return false;
    }

    const json* item = nullptr;
    if (!FindOperator(j, name, &item) || !item) {
        if (outErr) *outErr = "operator not found in ops.json";
        return false;
    }

    double s = 0.0;
    double step = 0.0;
    int delay = 0;
    char dir = 'E';

    if (!ExtractMoveFields(*item, s, dir, step, delay, outErr)) return false;

    // Keep existing clamping behavior from the original ApplyUiFromOperatorJson
    if (s < 0.0) s = 0.0;
    if (s > 100.0) s = 100.0;
    if (step < 0.0) step = 0.0;
    if (step > 100.0) step = 100.0;

    return ApplyMoveToGlobals(s, dir, step, delay);
}

bool SaveUiForOperator(const std::string& name, std::string* outErr) {
    if (outErr) outErr->clear();

    if (g_delayMs < kDelayMin || g_delayMs > kDelayMax) {
        if (outErr) *outErr = "Delay must be 1..50";
        return false;
    }

    const float down = MoveMath:: Snap1(MoveMath::ZeroEps(g_downForce));
    const float east = MoveMath::Snap1(MoveMath::ZeroEps(g_rightDrift));
    const float west = MoveMath::Snap1(MoveMath::ZeroEps(g_leftDrift));

    const bool eNonZero = (east != 0.0f);
    const bool wNonZero = (west != 0.0f);

    if (eNonZero && wNonZero) {
        if (outErr) *outErr = "Either East (e) or West (w) must be 0 (not both non-zero)";
        return false;
    }
    if (!eNonZero && !wNonZero) {
        if (outErr) *outErr = "Set either East (e) or West (w) to a non-zero value";
        return false;
    }

    json j;
    if (!LoadOpsJson(j) || !j.is_array()) j = json::array();

    json* target = nullptr;
    for (auto& item : j) {
        if (!item.is_object()) continue;

        auto itName = item.find("name");
        if (itName != item.end() && itName->is_string() && itName->get<std::string>() == name) {
            target = &item;
            break;
        }
    }

    if (!target) {
        j.push_back(json::object());
        target = &j.back();
        (*target)["name"] = name;
    }

    (*target)["s"] = static_cast<double>(MoveMath::Snap2(MoveMath::ForceUiToJson(down)));
    (*target)["delay"] = g_delayMs;

    if (eNonZero) {
        target->erase("w");
        (*target)["e"] = static_cast<double>(MoveMath::Snap2(MoveMath::ForceUiToJson(east)));
    }
    else {
        target->erase("e");
        (*target)["w"] = static_cast<double>(MoveMath::Snap2(MoveMath::ForceUiToJson(west)));
    }

    std::string err;
    if (!ValidateOpObject(*target, &err)) {
        if (outErr) *outErr = err;
        return false;
    }

    if (!SaveOpsJson(j)) {
        if (outErr) *outErr = "Failed to write ops.json";
        return false;
    }

    return true;
}

bool LoadKeybindsFromJson(std::string* outErr) {
    if (outErr) outErr->clear();

    const auto p = KeybindsPath();
    if (p.empty()) {
        if (outErr) *outErr = "ExpandEnvironmentStrings failed for KEYBIND_PATH";
        return false;
    }

    if (!std::filesystem::exists(p)) return true;

    const std::string content = ReadFileBinary(p);
    if (content.empty()) {
        if (outErr) *outErr = "Cannot open keybinds.json";
        return false;
    }

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
            if (it != b.end() && it->is_number_integer()) dst = it->get<int>();
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
        if (outErr) *outErr = "ExpandEnvironmentStrings failed for KEYBIND_PATH";
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
        if (outErr) *outErr = "Write/replace failed";
        return false;
    }

    return true;
}
