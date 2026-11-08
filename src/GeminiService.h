#pragma once
/**
 * GeminiService - Tích hợp Google Gemini API để kiểm duyệt & phân loại bài viết.
 *
 * Cấu hình qua file .env (không hardcode key):
 *   GEMINI_API_KEY=...
 *   GEMINI_MODEL=gemini-2.0-flash
 *   GEMINI_MODERATION_ENABLED=true
 *   GEMINI_BLOCK_SEVERE=true
 *
 * Gọi HTTPS bằng curl CLI (tương thích Windows/Linux, không cần link OpenSSL).
 */

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

struct GeminiModerationResult {
    bool ok = false;                    // API gọi thành công?
    std::string category = "doi_song";  // giai_tri | hoc_tap | cong_nghe | doi_song | the_thao | khac
    std::string category_label = "Đời sống";
    std::string risk_level = "SAFE";    // SAFE | LOW | MEDIUM | HIGH | SEVERE
    std::vector<std::string> flags;     // spam, ads, insult, violence, inappropriate...
    std::string action = "ALLOW";       // ALLOW | FLAG | BLOCK
    std::string reason;
    double confidence = 0.0;
    bool used_ai = false;               // true nếu gọi Gemini thật; false nếu fallback keyword
};

class GeminiService {
public:
    static GeminiService& instance() {
        static GeminiService inst;
        return inst;
    }

    void loadEnv(const std::string& path = ".env") {
        std::ifstream f(path);
        if (!f.is_open()) {
            // Thử thư mục cha / cwd
            f.open("./.env");
        }
        if (!f.is_open()) {
            std::cout << "[Gemini] Không tìm thấy file .env — AI moderation sẽ dùng fallback từ khóa.\n";
            std::cout << "[Gemini] Sao chép .env.example thành .env và điền GEMINI_API_KEY.\n";
            return;
        }
        std::string line;
        while (std::getline(f, line)) {
            // Bỏ comment và dòng trống
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = trim(line.substr(0, pos));
            std::string val = trim(line.substr(pos + 1));
            // Bỏ dấu ngoặc kép nếu có
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }
            env_[key] = val;
        }
        apiKey_ = envGet("GEMINI_API_KEY", "");
        model_ = envGet("GEMINI_MODEL", "gemini-2.0-flash");
        enabled_ = toBool(envGet("GEMINI_MODERATION_ENABLED", "true"));
        blockSevere_ = toBool(envGet("GEMINI_BLOCK_SEVERE", "true"));

        if (apiKey_.empty()) {
            std::cout << "[Gemini] GEMINI_API_KEY trống — dùng phân loại từ khóa local.\n";
        } else {
            std::cout << "[Gemini] Đã nạp API Key (ẩn). Model: " << model_ << " | moderation=" << (enabled_ ? "ON" : "OFF") << "\n";
        }
    }

    const std::string& model() const { return model_; }
    bool isEnabled() const { return enabled_; }
    bool hasApiKey() const { return !apiKey_.empty(); }

    /** Phân tích nội dung bài đăng: chủ đề + kiểm duyệt */
    GeminiModerationResult analyzePost(const std::string& title, const std::string& content) {
        GeminiModerationResult r;
        if (!enabled_) {
            r = fallbackAnalyze(title, content);
            r.ok = true;
            r.action = "ALLOW";
            r.reason = "Moderation disabled";
            return r;
        }
        if (apiKey_.empty()) {
            r = fallbackAnalyze(title, content);
            r.ok = true;
            return r;
        }

        std::string prompt =
            "Bạn là hệ thống kiểm duyệt mạng xã hội tiếng Việt. Phân tích bài viết sau và trả về DUY NHẤT một JSON hợp lệ, không markdown, không giải thích:\n"
            "{\n"
            "  \"category\": \"giai_tri|hoc_tap|cong_nghe|doi_song|the_thao|khac\",\n"
            "  \"category_label\": \"Giải trí|Học tập|Công nghệ|Đời sống|Thể thao|Khác\",\n"
            "  \"risk_level\": \"SAFE|LOW|MEDIUM|HIGH|SEVERE\",\n"
            "  \"flags\": [\"spam|ads|insult|violence|hate|sexual|scam|inappropriate\"],\n"
            "  \"action\": \"ALLOW|FLAG|BLOCK\",\n"
            "  \"reason\": \"mô tả ngắn tiếng Việt\",\n"
            "  \"confidence\": 0.0\n"
            "}\n"
            "Quy tắc:\n"
            "- SAFE/LOW + ALLOW: nội dung bình thường\n"
            "- MEDIUM + FLAG: nghi spam/quảng cáo nhẹ, vẫn cho đăng nhưng đánh dấu\n"
            "- HIGH/SEVERE + BLOCK: xúc phạm nặng, bạo lực, 18+, lừa đảo → từ chối đăng\n"
            "- flags để mảng rỗng [] nếu không có vấn đề\n\n"
            "Tiêu đề: " + title + "\n"
            "Nội dung: " + content + "\n";

        std::string responseText;
        if (!callGemini(prompt, responseText)) {
            r = fallbackAnalyze(title, content);
            r.ok = true;
            r.reason = "Gemini API lỗi, dùng fallback: " + r.reason;
            return r;
        }

        // Trích JSON từ response (có thể bọc ```json)
        std::string jsonStr = extractJsonObject(responseText);
        try {
            auto j = json::parse(jsonStr);
            r.ok = true;
            r.used_ai = true;
            r.category = j.value("category", "doi_song");
            r.category_label = j.value("category_label", categoryLabel(r.category));
            r.risk_level = j.value("risk_level", "SAFE");
            r.action = j.value("action", "ALLOW");
            r.reason = j.value("reason", "");
            r.confidence = j.value("confidence", 0.8);
            if (j.contains("flags") && j["flags"].is_array()) {
                for (auto& f : j["flags"]) {
                    if (f.is_string()) r.flags.push_back(f.get<std::string>());
                }
            }
            // Chuẩn hóa action theo risk nếu model trả lệch
            if (r.risk_level == "SEVERE" || r.risk_level == "HIGH") {
                if (blockSevere_) r.action = "BLOCK";
                else if (r.action == "ALLOW") r.action = "FLAG";
            }
            normalizeCategory(r);
        } catch (...) {
            r = fallbackAnalyze(title, content);
            r.ok = true;
            r.reason = "Parse Gemini JSON thất bại, dùng fallback";
        }
        return r;
    }

private:
    std::map<std::string, std::string> env_;
    std::string apiKey_;
    std::string model_ = "gemini-2.0-flash";
    bool enabled_ = true;
    bool blockSevere_ = true;

    static std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace((unsigned char)s[a])) a++;
        while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
        return s.substr(a, b - a);
    }

    std::string envGet(const std::string& k, const std::string& def) const {
        auto it = env_.find(k);
        return it == env_.end() ? def : it->second;
    }

    static bool toBool(const std::string& v) {
        std::string s = v;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s == "1" || s == "true" || s == "yes" || s == "on";
    }

    static std::string categoryLabel(const std::string& c) {
        if (c == "giai_tri") return "Giải trí";
        if (c == "hoc_tap") return "Học tập";
        if (c == "cong_nghe") return "Công nghệ";
        if (c == "the_thao") return "Thể thao";
        if (c == "doi_song") return "Đời sống";
        return "Khác";
    }

    static void normalizeCategory(GeminiModerationResult& r) {
        static const char* allowed[] = {"giai_tri", "hoc_tap", "cong_nghe", "doi_song", "the_thao", "khac"};
        bool ok = false;
        for (auto* a : allowed) if (r.category == a) { ok = true; break; }
        if (!ok) r.category = "doi_song";
        if (r.category_label.empty()) r.category_label = categoryLabel(r.category);
    }

    static std::string extractJsonObject(const std::string& text) {
        auto start = text.find('{');
        auto end = text.rfind('}');
        if (start == std::string::npos || end == std::string::npos || end <= start) return "{}";
        return text.substr(start, end - start + 1);
    }

    /** Gọi Gemini generateContent qua curl */
    bool callGemini(const std::string& prompt, std::string& outText) {
        // Escape prompt vào JSON
        json body = {
            {"contents", json::array({
                {{"parts", json::array({{{"text", prompt}}})}}
            })},
            {"generationConfig", {
                {"temperature", 0.2},
                {"maxOutputTokens", 512}
            }}
        };
        std::string bodyStr = body.dump();

        // Ghi body ra file tạm để tránh lỗi escape shell
#ifdef _WIN32
        std::string tmpIn = "gemini_req.json";
        std::string tmpOut = "gemini_res.json";
#else
        std::string tmpIn = "/tmp/gemini_req.json";
        std::string tmpOut = "/tmp/gemini_res.json";
#endif
        {
            std::ofstream ofs(tmpIn, std::ios::binary);
            if (!ofs) return false;
            ofs << bodyStr;
        }

        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model_ + ":generateContent?key=" + apiKey_;

#ifdef _WIN32
        std::string cmd = "curl -s -X POST \"" + url + "\" -H \"Content-Type: application/json\" -d @" + tmpIn + " -o " + tmpOut + " --connect-timeout 15 --max-time 30";
#else
        // Escape URL carefully - key may have special chars; put URL in single quotes after replacing '
        std::string safeUrl = url;
        // Use curl with file body
        std::string cmd = "curl -s -X POST '" + safeUrl + "' -H 'Content-Type: application/json' --data-binary @" + tmpIn + " -o " + tmpOut + " --connect-timeout 15 --max-time 30 2>/dev/null";
#endif

        int rc = std::system(cmd.c_str());
        (void)rc;

        std::ifstream ifs(tmpOut, std::ios::binary);
        if (!ifs) return false;
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string raw = ss.str();
        if (raw.empty()) return false;

        try {
            auto j = json::parse(raw);
            if (j.contains("error")) {
                std::cerr << "[Gemini] API error: " << j["error"].dump() << std::endl;
                return false;
            }
            // candidates[0].content.parts[0].text
            if (j.contains("candidates") && j["candidates"].is_array() && !j["candidates"].empty()) {
                auto& c0 = j["candidates"][0];
                if (c0.contains("content") && c0["content"].contains("parts") && c0["content"]["parts"].is_array()
                    && !c0["content"]["parts"].empty()) {
                    outText = c0["content"]["parts"][0].value("text", "");
                    return !outText.empty();
                }
            }
        } catch (...) {
            return false;
        }
        return false;
    }

    /** Fallback khi không có API key / API lỗi: phân loại & kiểm duyệt bằng từ khóa */
    static GeminiModerationResult fallbackAnalyze(const std::string& title, const std::string& content) {
        GeminiModerationResult r;
        r.ok = true;
        r.used_ai = false;
        r.confidence = 0.55;

        std::string text = title + " " + content;
        std::string lower;
        lower.reserve(text.size());
        for (unsigned char c : text) lower.push_back((char)std::tolower(c));

        // Chủ đề
        auto has = [&](const char* kw) { return lower.find(kw) != std::string::npos; };
        if (has("code") || has("lập trình") || has("lap trinh") || has("ai ") || has("c++") || has("python") || has("công nghệ") || has("cong nghe") || has("phần mềm") || has("software") || has("app ")) {
            r.category = "cong_nghe";
        } else if (has("học") || has("hoc ") || has("thi ") || has("đại học") || has("dai hoc") || has("bài tập") || has("bai tap") || has("sinh viên") || has("sinh vien") || has("trường") || has("truong")) {
            r.category = "hoc_tap";
        } else if (has("bóng đá") || has("bong da") || has("world cup") || has("thể thao") || has("the thao") || has("cầu lông") || has("tennis") || has("olympic") || has("match") || has("chung kết")) {
            r.category = "the_thao";
        } else if (has("phim") || has("nhạc") || has("nhac") || has("game") || has("giải trí") || has("giai tri") || has("ca sĩ") || has("ca si") || has("netflix") || has("mv ")) {
            r.category = "giai_tri";
        } else {
            r.category = "doi_song";
        }
        r.category_label = categoryLabel(r.category);

        // Kiểm duyệt từ khóa nhạy cảm (đơn giản)
        std::vector<std::pair<std::string, std::string>> bad = {
            {"địt", "insult"}, {"đụ", "insult"}, {"lồn", "insult"}, {"cặc", "insult"},
            {"dm ", "insult"}, {"đm ", "insult"}, {"vcl", "insult"},
            {"giết", "violence"}, {"giết chết", "violence"}, {"máu me", "violence"},
            {"sex", "sexual"}, {"xxx", "sexual"}, {"porn", "sexual"},
            {"kiếm tiền online", "scam"}, {"click vào link", "scam"}, {"đầu tư nhận", "scam"},
            {"mua ngay", "ads"}, {"sale sốc", "ads"}, {"chỉ hôm nay", "ads"}, {"inbox ngay", "ads"}
        };
        for (auto& p : bad) {
            if (lower.find(p.first) != std::string::npos) {
                r.flags.push_back(p.second);
            }
        }

        if (std::find(r.flags.begin(), r.flags.end(), "violence") != r.flags.end() ||
            std::find(r.flags.begin(), r.flags.end(), "sexual") != r.flags.end() ||
            std::find(r.flags.begin(), r.flags.end(), "scam") != r.flags.end()) {
            r.risk_level = "HIGH";
            r.action = "BLOCK";
            r.reason = "Phát hiện nội dung không phù hợp (fallback từ khóa)";
        } else if (std::find(r.flags.begin(), r.flags.end(), "insult") != r.flags.end()) {
            r.risk_level = "MEDIUM";
            r.action = "FLAG";
            r.reason = "Nghi ngờ ngôn từ xúc phạm";
        } else if (std::find(r.flags.begin(), r.flags.end(), "ads") != r.flags.end()) {
            r.risk_level = "LOW";
            r.action = "FLAG";
            r.reason = "Nghi ngờ nội dung quảng cáo";
        } else {
            r.risk_level = "SAFE";
            r.action = "ALLOW";
            r.reason = "Nội dung an toàn (fallback)";
        }
        return r;
    }
};
