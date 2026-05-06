#pragma once

#include <cmath>
#include <string>

bool GetMoveForOperator(
    const std::string& name,
    double& outS,
    char& outDir,
    double& outStep,
    int& outDelay
);

bool GetMoveForOperatorByBmpStem(
    const std::string& bmpFilenameUtf8,
    double& outS,
    char& outDir,
    double& outStep,
    int& outDelay,
    std::string* outCanonicalName
);

bool SaveOperatorToOpsJson(
    const std::string& operatorName,
    int delayMs,
    double southS,
    char ewDir,
    double ewStep,
    std::string* outErr
);

bool LoadKeybindsFromJson(std::string* outErr);
bool SaveKeybindsToJson(std::string* outErr);

bool LoadUiSettingsFromJson(std::string* outErr);
bool SaveUiSettingsToJson(std::string* outErr);

namespace MoveMath {

constexpr float kUiMin = 0.0f;
constexpr float kUiMax = 100.0f;

inline float Clamp(float v, float minV, float maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

inline float Snap2(float v) { return std::round(v * 100.0f) / 100.0f; }

inline float ForceJsonToUi(float j) {
    j = Clamp(j, kUiMin, kUiMax);
    return Snap2(j);
}

}
