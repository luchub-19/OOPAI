#pragma once
#include <vector>
#include "src/common/step.h"

class LoopDetector {
private:
    int warning_threshold;
    int critical_threshold;

public:
    explicit LoopDetector(int warn_thresh = 2, int crit_thresh = 3);

    [[nodiscard]] bool detectLoop(const std::vector<Step>& history) const;
    [[nodiscard]] bool isPingPongLoop(const std::vector<Step>& history) const;
};