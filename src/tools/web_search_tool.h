#pragma once

#include "src/tools/tool.h"

#include <optional>
#include <string>

// WebSearchTool: goi toi mot SearXNG instance (hoac bat ky API tuong thich
// dinh dang JSON cua SearXNG) de tim kiem thong tin tren web.
// Args dau vao la JSON: {"query": "tu khoa can tim", "num_results": 5}
//   - "num_results" la tuy chon, mac dinh va gioi han toi da boi
//     max_results_ (truyen tu constructor).
//
// Day la 1 trong 5 tool bat buoc (muc 3.2 de bai).
//
// search_api_url_: base URL cua SearXNG instance, vi du "http://localhost:8080".
//   KHONG bao gom "/search" phia sau - tool tu ghep.
class WebSearchTool : public Tool {
public:
    explicit WebSearchTool(std::string search_api_url,
                            int maxResults = 5,
                            int timeoutSeconds = 10);

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::string getDescription() const override;
    std::optional<std::string> execute(const std::string& args) override;

private:
    std::string search_api_url_;
    int max_results_;
    int timeout_seconds_;

    // Goi HTTP GET toi {search_api_url_}/search?q=...&format=json bang libcurl,
    // tra ve raw JSON string. nullopt neu khong ket noi duoc / timeout.
    std::optional<std::string> httpGet(const std::string& url) const;

    // Parse raw JSON tu SearXNG, cat lay toi da num_results ket qua,
    // dinh dang lai thanh text de LLM de doc.
    std::optional<std::string> formatResults(const std::string& raw_json,
                                              int num_results) const;
};