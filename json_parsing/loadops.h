#pragma once

#include <cmath>
#include <string>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool LoadOpsJson(json& out);
bool SaveOpsJson(const json& j);

bool GetMoveForOperator(
    const std::string& name,
    double& outS,
    char& outDir,
    double& outStep,
    int& outDelay
);

bool LoadUiForOperator(const std::string& name);
bool SaveUiForOperator(const std::string& name, std::string* outErr);
bool ApplyUiFromOperatorJson(const std::string& name, std::string* outErr);

bool LoadKeybindsFromJson(std::string* outErr);
bool SaveKeybindsToJson(std::string* outErr);

namespace MoveMath {

    constexpr float kUiMin = 0.0f;
    constexpr float kUiMax = 5.0f;

    inline float Snap1(float v) { return std::round(v * 10.0f) / 10.0f; }
    inline float Snap2(float v) { return std::round(v * 100.0f) / 100.0f; }

    inline float ZeroEps(float v, float eps = 0.0001f) {
        return (std::fabs(v) < eps) ? 0.0f : v;
    }

    inline float Clamp(float v, float minV, float maxV) {
        if (v < minV) return minV;
        if (v > maxV) return maxV;
        return v;
    }

    inline float ForceUiToJson(float ui) {
        ui = Clamp(ui, kUiMin, kUiMax);
        return Snap2(ui);
    }

    inline float ForceJsonToUi(float j) {
        j = Clamp(j, kUiMin, kUiMax);
        return Snap2(j);
    }

} // namespace MoveMath
