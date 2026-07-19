#include "web_search_tool.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

// Ham callback ma libcurl goi moi khi nhan duoc 1 phan du lieu response.
// Giong het pattern trong ollama_client.cpp - gop du lieu vao 1 std::string.
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

WebSearchTool::WebSearchTool(std::string search_api_url, int maxResults, int timeoutSeconds)
    : search_api_url_(std::move(search_api_url)),
      max_results_(maxResults),
      timeout_seconds_(timeoutSeconds) {}

std::string WebSearchTool::getName() const { return "web_search"; }

std::string WebSearchTool::getDescription() const {
    return "Cong cu tim kiem thong tin tren Internet (qua SearXNG). "
           "Tham so dau vao (args) phai la JSON hop le: "
           "{\"query\": \"tu khoa can tim\", \"num_results\": 5}. "
           "'num_results' la tuy chon, mac dinh va toi da " +
           std::to_string(max_results_) +
           " ket qua. Dung tool nay khi can thong tin moi/thoi su/khong co "
           "trong kien thuc san co.";
}

std::optional<std::string> WebSearchTool::httpGet(const std::string& url) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::nullopt;
    }

    std::string read_buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds_));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    // Mot so SearXNG instance chan request khong co User-Agent.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AI-Agent-Framework/1.0");

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return std::nullopt;
    }
    if (http_code < 200 || http_code >= 300) {
        return std::nullopt;
    }

    return read_buffer;
}

std::optional<std::string> WebSearchTool::formatResults(const std::string& raw_json,
                                                          int num_results) const {
    json j;
    try {
        j = json::parse(raw_json);
    } catch (const json::parse_error& e) {
        return std::string("Loi parse JSON tra ve tu SearXNG: ") + e.what();
    }

    if (!j.contains("results") || !j["results"].is_array()) {
        return std::string("Khong tim thay truong 'results' trong response cua SearXNG.");
    }

    const auto& results = j["results"];
    if (results.empty()) {
        return std::string("Khong tim thay ket qua nao.");
    }

    std::ostringstream oss;
    int count = 0;
    for (const auto& item : results) {
        if (count >= num_results) break;

        std::string title = item.value("title", "(khong co tieu de)");
        std::string url = item.value("url", "");
        std::string content = item.value("content", "(khong co mo ta)");

        oss << (count + 1) << ". " << title << "\n";
        oss << "   URL: " << url << "\n";
        oss << "   Noi dung: " << content << "\n\n";
        ++count;
    }

    return oss.str();
}

std::optional<std::string> WebSearchTool::execute(const std::string& args) {
    json j;
    try {
        j = json::parse(args);
    } catch (const json::parse_error& e) {
        return std::string("Loi parse JSON dau vao: ") + e.what();
    }

    if (!j.contains("query") || !j["query"].is_string()) {
        return std::string("Loi: JSON args thieu truong 'query' kieu string.");
    }
    std::string query = j["query"].get<std::string>();
    if (query.empty()) {
        return std::string("Loi: 'query' rong.");
    }

    int num_results = max_results_;
    if (j.contains("num_results") && j["num_results"].is_number_integer()) {
        num_results = std::min(j["num_results"].get<int>(), max_results_);
        if (num_results <= 0) num_results = max_results_;
    }

    if (search_api_url_.empty()) {
        return std::string("Loi: chua cau hinh search_api_url cho WebSearchTool.");
    }

    // Ma hoa query an toan cho URL (dau cach, dau tieng Viet co dau, ky tu dac biet...)
    CURL* curl_for_escape = curl_easy_init();
    if (!curl_for_escape) {
        return std::string("Loi: khong khoi tao duoc CURL.");
    }
    char* escaped = curl_easy_escape(curl_for_escape, query.c_str(),
                                      static_cast<int>(query.size()));
    std::string encoded_query = escaped ? escaped : "";
    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl_for_escape);

    if (encoded_query.empty()) {
        return std::string("Loi: khong ma hoa duoc query.");
    }

    std::string full_url = search_api_url_ + "/search?q=" + encoded_query + "&format=json";

    auto raw_response = httpGet(full_url);
    if (!raw_response.has_value()) {
        return std::string("Loi: khong ket noi duoc toi SearXNG tai '") +
               search_api_url_ + "' (kiem tra service da chay chua, hoac bi timeout sau " +
               std::to_string(timeout_seconds_) + "s).";
    }

    return formatResults(*raw_response, num_results);
}