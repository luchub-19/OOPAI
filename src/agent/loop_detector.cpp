#include "loop_detector.h"
#include <iostream>

LoopDetector::LoopDetector(int warn_thresh, int crit_thresh)
    : warning_threshold(warn_thresh), critical_threshold(crit_thresh) {}

bool LoopDetector::detectLoop(const std::vector<Step>& history) const {
    if (history.size() < static_cast<size_t>(warning_threshold)) return false;

    // --- LOẠI 1: GENERIC REPEAT (Lặp hành động y hệt liên tiếp) ---
    const auto& last = history.back();
    if (last.action_type == "tool_call") {
        int consecutive_repeats = 1;
        
        for (auto it = history.rbegin() + 1; it != history.rend(); ++it) {
            if (it->action_type == "tool_call" && 
                it->tool_name == last.tool_name && 
                it->tool_args == last.tool_args) {
                consecutive_repeats++;
            } else {
                break;
            }
        }

        if (consecutive_repeats >= critical_threshold) {
            std::cerr << "[CRITICAL LOOP] Agent kẹt lặp lệnh: " << last.tool_name << "(" << last.tool_args << ") " << consecutive_repeats << " lần!\n";
            return true;
        } else if (consecutive_repeats >= warning_threshold) {
            std::cerr << "[Warning Loop] Agent đang có xu hướng lặp lệnh: " << last.tool_name << "\n";
        }
    }

    // --- LOẠI 2: PING-PONG LOOP (Đánh bóng bàn A -> B -> A -> B) ---
    return isPingPongLoop(history);
}

bool LoopDetector::isPingPongLoop(const std::vector<Step>& history) const {
    if (history.size() < 4) return false;

    size_t n = history.size();
    const Step& s0 = history[n - 1]; // A
    const Step& s1 = history[n - 2]; // B
    const Step& s2 = history[n - 3]; // A'
    const Step& s3 = history[n - 4]; // B'

    bool a_match = (s0.tool_name == s2.tool_name && s0.tool_args == s2.tool_args);
    bool b_match = (s1.tool_name == s3.tool_name && s1.tool_args == s3.tool_args);
    bool distinct = (s0.tool_name != s1.tool_name || s0.tool_args != s1.tool_args);

    if (a_match && b_match && distinct) {
        std::cerr << "[CRITICAL PING-PONG] kẹt đánh bóng bàn giữa [" << s0.tool_name << "] và [" << s1.tool_name << "]\n";
        return true;
    }
    return false;
}