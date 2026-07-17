#include "src/agent/react_agent_loop.h"
#include <regex>
#include <iostream>

void ReActAgentLoop::observe() {
    // Nạp pending_observation (do run() set trước mỗi turn, hoặc do act()
    // để lại từ turn trước) vào lịch sử hội thoại dưới dạng 1 Message thật.
    conversation_history.push_back(Message{"user", pending_observation, {}});
}

std::string ReActAgentLoop::think() {
    last_response = llm->chat(conversation_history);
    // Ghi lại lời AI vào history để turn sau LLM "nhớ" mình vừa nói gì
    conversation_history.push_back(Message{"assistant", last_response.content, {}});
    return last_response.content;
}

Step ReActAgentLoop::act() {
    Step step;
    const std::string& thought = last_response.content;
    step.thought     = thought;
    step.tokens_used = last_response.tokens_used;
    step.latency_ms  = last_response.latency_ms;

    static const std::regex done_regex(R"(Final Answer:\s*([^\n\r]+))");
    static const std::regex tool_regex(R"(Action:\s*([^\n\r]+)\s*[\r\n]+Action Input:\s*([^\n\r]+))");
    std::smatch match;

    if (std::regex_search(thought, match, done_regex)) {
        step.action_type = "done";
        step.tool_result  = match[1].str();
    }
    else if (std::regex_search(thought, match, tool_regex)) {
        step.action_type = "tool_call";
        step.tool_name    = match[1].str();
        step.tool_args    = match[2].str();

        std::cout << "  [Tool Executor] -> " << "tool name: " << step.tool_name
                  << "tool_args: " << step.tool_args << "\n";

        if (tools) {
            auto opt_res = tools->executeTool(step.tool_name, step.tool_args);
            step.tool_result = opt_res.value_or("ERROR: Tool failed or returned empty.");
        } else {
            step.tool_result = "ERROR: ToolRegistry is null.";
        }
    }
    else {
        step.action_type = "error";
        step.tool_result  = "SYNTAX_VIOLATION: expected 'Action:' or 'Final Answer:'";
    }

    return step;
}