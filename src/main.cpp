#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <map>
#include <mutex>
#include <random>
#include "httplib.h"
#include <nlohmann/json.hpp>
#include "Models.h"
#include "Database.h"
#include "SuggestService.h"
#include "SmtpService.h"
#include "GeminiService.h"

using json = nlohmann::json;

// ---- Phân quyền Admin / User ----
// Phiên làm việc của quản trị viên: token ngẫu nhiên do SERVER cấp khi đăng nhập,
// chỉ lưu trong bộ nhớ server. Mọi API /api/admin/* bắt buộc kèm token này
// (header X-Admin-Token), nên không thể giả mạo chỉ bằng cách gửi admin_id.
// (Token quản trị được lưu trong bảng admin_sessions — xem Database::issueAdminToken)
static bool requireAdmin(Database& db, const httplib::Request& req, const std::string& userId, httplib::Response& res) {
    const std::string token = req.get_header_value("X-Admin-Token");
    if (userId.empty() || !db.isUserAdmin(userId) || !db.verifyAdminToken(userId, token)) {
        res.status = 403;
        res.set_content(json({{"status", "error"}, {"message", "Chỉ quản trị viên đã đăng nhập mới được thực hiện thao tác này!"}}).dump(), "application/json; charset=utf-8");
        return false;
    }
    return true;
}

// Các API không thuộc chức năng mạng xã hội: Admin vẫn được dùng.
static bool isAdminExemptPath(const std::string& p) {
    return p.rfind("/api/admin/", 0) == 0 || p.rfind("/api/auth/", 0) == 0 ||
           p == "/api/users/heartbeat" || p == "/api/users/offline";
}

// Chặn tuyệt đối tài khoản Admin (là người thực hiện HOẶC là đối tượng) khỏi mọi API xã hội.
// Chạy ở tầng backend cho mọi request, không phụ thuộc việc ẩn nút ở frontend.
static bool requestInvolvesAdmin(Database& db, const httplib::Request& req, const json* parsedBody = nullptr) {
    const std::string& p = req.path;
    if (p.rfind("/api/", 0) != 0 || isAdminExemptPath(p)) return false;

    static const std::set<std::string> idKeys = {
        "user_id", "my_id", "target_id", "partner_id", "friend_id", "author_id", "sender_id",
        "receiver_id", "recipient_id", "requester_id", "current_user_id", "viewer_id",
        "operator_id", "member_id", "created_by", "reporter_id", "conversation_id",
        "user_ids", "members"
    };
    std::set<std::string> ids;
    for (const auto& k : idKeys) {
        auto range = req.params.equal_range(k);
        for (auto it = range.first; it != range.second; ++it) ids.insert(it->second);
    }
    if (parsedBody && parsedBody->is_object()) {
        const json& body = *parsedBody;
        auto addVal = [&ids](const json& v) {
            if (v.is_string()) ids.insert(v.get<std::string>());
            else if (v.is_number_integer()) ids.insert(std::to_string(v.get<long long>()));
        };
        for (const auto& k : idKeys) {
            if (!body.contains(k)) continue;
            const json& v = body[k];
            if (v.is_array()) { for (const auto& el : v) addVal(el); }
            else addVal(v);
        }
        if (p == "/api/users/update" && body.contains("id")) addVal(body["id"]);
    }
    for (const auto& id : ids) {
        if (!id.empty() && db.isUserAdmin(id)) return true;
    }
    return false;
}

static bool rejectIfAdmin(Database& db, const std::string& userId, httplib::Response& res, const std::string& msg = "Tài khoản quản trị viên không sử dụng chức năng mạng xã hội này.") {
    if (!userId.empty() && db.isUserAdmin(userId)) {
        res.status = 403;
        res.set_content(json({{"status", "error"}, {"message", msg}}).dump(), "application/json; charset=utf-8");
        return true;
    }
    return false;
}



static std::string getJsonStringSafe(const json& j, const std::string& key, const std::string& defVal = "") {
    if (!j.contains(key) || j[key].is_null()) return defVal;
    if (j[key].is_string()) return j[key].get<std::string>();
    if (j[key].is_number_integer()) return std::to_string(j[key].get<long long>());
    if (j[key].is_number_unsigned()) return std::to_string(j[key].get<unsigned long long>());
    if (j[key].is_number()) return std::to_string(j[key].get<double>());
    if (j[key].is_boolean()) return j[key].get<bool>() ? "true" : "false";
    return defVal;
}

static bool hasAcceptedLegalTerms(const json& body) {
    return body.contains("accept_terms") && body["accept_terms"].is_boolean() &&
           body["accept_terms"].get<bool>();
}

static size_t getBase64DataUrlSize(const std::string& value) {
    const size_t comma = value.find(',');
    if (comma == std::string::npos || value.substr(0, comma).find(";base64") == std::string::npos) return 0;
    const size_t encodedSize = value.size() - comma - 1;
    size_t padding = 0;
    if (encodedSize > 0 && value.back() == '=') ++padding;
    if (encodedSize > 1 && value[value.size() - 2] == '=') ++padding;
    const size_t decodedSize = encodedSize * 3 / 4;
    return decodedSize > padding ? decodedSize - padding : 0;
}

static bool validatePostMedia(const std::string& mediaType, const std::string& mediaUrl, std::string& error) {
    constexpr size_t imageLimit = 10ULL * 1024 * 1024;
    constexpr size_t videoLimit = 250ULL * 1024 * 1024;
    if (mediaType == "IMAGE" && !mediaUrl.empty()) {
        std::vector<std::string> imageUrls;
        if (mediaUrl.front() == '[') {
            try {
                const json parsed = json::parse(mediaUrl);
                if (!parsed.is_array()) {
                    error = "Danh sách hình ảnh không hợp lệ.";
                    return false;
                }
                for (const auto& item : parsed) {
                    if (!item.is_string()) {
                        error = "Danh sách hình ảnh không hợp lệ.";
                        return false;
                    }
                    imageUrls.push_back(item.get<std::string>());
                }
            } catch (const std::exception&) {
                error = "Danh sách hình ảnh không hợp lệ.";
                return false;
            }
        } else {
            imageUrls.push_back(mediaUrl);
        }
        if (imageUrls.empty() || imageUrls.size() > 3) {
            error = "Mỗi bài viết chỉ được tải lên tối đa 3 hình ảnh.";
            return false;
        }
        for (const auto& imageUrl : imageUrls) {
            if (getBase64DataUrlSize(imageUrl) > imageLimit) {
                error = "Mỗi hình ảnh không được vượt quá 10 MB.";
                return false;
            }
        }
    } else if (mediaType == "VIDEO" && getBase64DataUrlSize(mediaUrl) > videoLimit) {
        error = "Video không được vượt quá 250 MB.";
        return false;
    }
    return true;
}


// ============================================================================
//  BẢO MẬT TẦNG HTTP
//  - Xác thực phiên (cookie HttpOnly), buộc danh tính trong request == chủ phiên (chống IDOR/mạo danh)
//  - Rate limit theo IP / theo người dùng / theo nhóm API, khoá đăng nhập sai
//  - CSRF (kiểm tra Origin), header bảo mật + CSP, giới hạn kích thước body
//  - Làm sạch dữ liệu đầu vào ở một chỗ duy nhất (tên, văn bản, URL media)
// ============================================================================
static sec::RateLimiter gRate;
static sec::FailTracker gFails;
static bool gTrustProxy = false;
static bool gCookieSecure = false;
static size_t gMaxBodyBytes = 48ULL * 1024 * 1024;
static size_t gMaxVideoBytes = 30ULL * 1024 * 1024;
static size_t gMaxImageBytes = 8ULL * 1024 * 1024;
static std::set<std::string> gAllowedOrigins;
static std::string gGoogleClientId;

static void sendJsonError(httplib::Response& res, int status, const std::string& msg) {
    res.status = status;
    res.set_content(json({{"status", "error"}, {"message", msg}}).dump(), "application/json; charset=utf-8");
}

static void sendTooMany(httplib::Response& res, int retryAfterSec = 10, const std::string& msg = "Bạn thao tác quá nhanh. Vui lòng thử lại sau ít giây.") {
    res.set_header("Retry-After", std::to_string(std::max(1, retryAfterSec)));
    sendJsonError(res, 429, msg);
}

static int paramInt(const httplib::Request& req, const char* key, int def, int lo, int hi) {
    if (!req.has_param(key)) return def;
    return sec::clampInt(req.get_param_value(key), def, lo, hi);
}

static std::string trimStr(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

static bool isIpText(const std::string& s) {
    if (s.empty() || s.size() > 45) return false;
    for (unsigned char c : s) if (!(std::isxdigit(c) || c == '.' || c == ':')) return false;
    return true;
}

// IP thật của client. Chỉ tin header của reverse proxy khi TRUST_PROXY=1 (nếu không, kẻ tấn công tự giả IP để né rate limit).
static std::string clientIp(const httplib::Request& req) {
    if (gTrustProxy) {
        const std::string cf = trimStr(req.get_header_value("CF-Connecting-IP"));
        if (isIpText(cf)) return cf;
        const std::string xff = req.get_header_value("X-Forwarded-For");
        if (!xff.empty()) {
            const size_t p = xff.rfind(',');
            const std::string last = trimStr(p == std::string::npos ? xff : xff.substr(p + 1));
            if (isIpText(last)) return last;
        }
    }
    return req.remote_addr;
}

static std::string getCookie(const httplib::Request& req, const std::string& name) {
    const std::string h = req.get_header_value("Cookie");
    size_t pos = 0;
    while (pos < h.size()) {
        size_t end = h.find(';', pos);
        if (end == std::string::npos) end = h.size();
        std::string part = trimStr(h.substr(pos, end - pos));
        const size_t eq = part.find('=');
        if (eq != std::string::npos && part.substr(0, eq) == name) return part.substr(eq + 1);
        pos = end + 1;
    }
    return "";
}

static std::string requestSessionToken(const httplib::Request& req) {
    std::string t = getCookie(req, "sid");
    if (t.empty()) {
        const std::string a = req.get_header_value("Authorization");
        if (a.rfind("Bearer ", 0) == 0) t = trimStr(a.substr(7));
    }
    return t;
}

static bool requestIsHttps(const httplib::Request& req) {
    return gCookieSecure || (gTrustProxy && req.get_header_value("X-Forwarded-Proto") == "https");
}

static void setSessionCookie(const httplib::Request& req, httplib::Response& res, const std::string& token) {
    std::string c = "sid=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" + std::to_string(Database::kSessionTtlSec);
    if (requestIsHttps(req)) c += "; Secure";
    res.set_header("Set-Cookie", c);
}

static void clearSessionCookie(const httplib::Request& req, httplib::Response& res) {
    std::string c = "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";
    if (requestIsHttps(req)) c += "; Secure";
    res.set_header("Set-Cookie", c);
}

static void applySecurityHeaders(const httplib::Request& req, httplib::Response& res) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("Referrer-Policy", "same-origin");
    res.set_header("Permissions-Policy", "camera=(), microphone=(), geolocation=(), payment=()");
    res.set_header("Cross-Origin-Opener-Policy", "same-origin-allow-popups");
    res.set_header("Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self' 'unsafe-inline' https://accounts.google.com; "
        "style-src 'self' 'unsafe-inline' https://accounts.google.com; "
        "img-src 'self' data: blob: https://*.giphy.com https://images.unsplash.com https://*.googleusercontent.com; "
        "media-src 'self' data: blob: https://*.giphy.com; "
        "connect-src 'self' https://api.giphy.com https://accounts.google.com; "
        "frame-src https://accounts.google.com; "
        "font-src 'self' data:; "
        "object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'");
    if (requestIsHttps(req)) res.set_header("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    if (req.path.rfind("/api/", 0) == 0) res.set_header("Cache-Control", "no-store");
}

static std::string originHost(const std::string& origin) {
    const size_t p = origin.find("://");
    return sec::toLowerAscii(p == std::string::npos ? origin : origin.substr(p + 3));
}

// CSRF: yêu cầu thay đổi dữ liệu phải đến từ chính trang web này (hoặc origin được cấu hình)
static bool originAllowed(const httplib::Request& req) {
    if (req.get_header_value("Sec-Fetch-Site") == "cross-site") {
        const std::string o = req.get_header_value("Origin");
        if (gAllowedOrigins.count(o) == 0) return false;
    }
    const std::string origin = req.get_header_value("Origin");
    if (origin.empty() || origin == "null") return origin.empty();  // không có Origin: client không phải trình duyệt
    if (gAllowedOrigins.count(origin)) return true;
    std::string host = req.get_header_value("Host");
    if (gTrustProxy && !req.get_header_value("X-Forwarded-Host").empty()) host = req.get_header_value("X-Forwarded-Host");
    return originHost(origin) == sec::toLowerAscii(trimStr(host));
}

enum class AuthMode { Public, Session, AdminToken, SessionOrAdmin };

static AuthMode authModeFor(const std::string& p) {
    if (p == "/api/auth/send-otp" || p == "/api/auth/register-with-otp" || p == "/api/auth/login" ||
        p == "/api/auth/google-login" || p == "/api/auth/config" || p == "/api/auth/logout") return AuthMode::Public;
    if (p.rfind("/api/admin/", 0) == 0) return AuthMode::AdminToken;
    if (p == "/api/users/heartbeat" || p == "/api/users/offline") return AuthMode::SessionOrAdmin;
    return AuthMode::Session;
}

static bool isHeavyGet(const std::string& p) {
    static const std::set<std::string> heavy = {
        "/api/posts", "/api/search", "/api/friends/suggest", "/api/messages/search", "/api/messages/search-all",
        "/api/groups/search", "/api/community-groups/discover", "/api/community-groups/feed", "/api/users/posts"};
    return heavy.count(p) > 0;
}

static bool isUploadPath(const std::string& p) {
    static const std::set<std::string> up = {
        "/api/posts/create", "/api/posts/edit", "/api/posts/repost", "/api/posts/comment", "/api/messages/send",
        "/api/groups/messages/send", "/api/community-groups/posts", "/api/community-groups/posts/comment",
        "/api/users/update", "/api/groups/update-avatar", "/api/groups/create", "/api/community-groups/create",
        "/api/messages/share-post"};
    return up.count(p) > 0;
}

// Khoá chứa DANH TÍNH NGƯỜI THAO TÁC: bắt buộc trùng chủ phiên. (Khoá chỉ ĐỐI TƯỢNG như target_id, partner_id, friend_id... không nằm ở đây.)
static bool isActorKey(const std::string& method, const std::string& path, const std::string& key) {
    static const std::set<std::string> always = {"my_id", "current_user_id", "sender_id", "requester_id", "operator_id",
                                                 "viewer_id", "reporter_id", "created_by"};
    // GET /api/users/typing: user_id là ĐỐI PHƯƠNG đang gõ, partner_id mới là người gọi
    if (method == "GET" && path == "/api/users/typing") return key == "partner_id";
    if (always.count(key)) return true;
    // user_id là ĐỐI TƯỢNG (không phải người thao tác) ở các route sau
    if (key == "user_id") return path != "/api/groups/members/add" && path != "/api/groups/members/remove" &&
                                 path != "/api/friends/full" && path != "/api/users/profile";
    if (key == "author_id") return path != "/api/users/posts";
    if (key == "id") return path == "/api/users/update";
    if (key == "admin_id") return path.rfind("/api/community-groups/", 0) == 0;
    return false;
}

struct RateRule { const char* cls; double cap; double refill; };

static RateRule userRateRule(const std::string& method, const std::string& p) {
    static const std::set<std::string> createClass = {"/api/posts/create", "/api/posts/repost", "/api/community-groups/posts",
        "/api/groups/create", "/api/community-groups/create", "/api/community-groups/posts/share-to-feed"};
    static const std::set<std::string> commentClass = {"/api/posts/comment", "/api/community-groups/posts/comment"};
    static const std::set<std::string> msgClass = {"/api/messages/send", "/api/groups/messages/send", "/api/messages/share-post"};
    static const std::set<std::string> socialClass = {"/api/friends/request", "/api/reports/create", "/api/users/block"};
    if (createClass.count(p)) return {"create", 6, 1.0 / 30};
    if (commentClass.count(p)) return {"comment", 20, 0.25};
    if (msgClass.count(p)) return {"msg", 40, 2.0};
    if (socialClass.count(p)) return {"social", 20, 0.2};
    if (p == "/api/users/update") return {"profile", 10, 1.0 / 60};
    if (p == "/api/users/change-password") return {"chgpw", 5, 1.0 / 120};
    if (method != "GET") return {"write", 60, 5.0};
    if (isHeavyGet(p)) return {"heavy", 12, 1.5};
    return {"read", 150, 25.0};
}

static std::string requestAdminId(const httplib::Request& req, const json& body) {
    std::string id = req.get_param_value("user_id");
    if (id.empty() && body.is_object() && body.contains("user_id") && body["user_id"].is_string()) id = body["user_id"].get<std::string>();
    return id;
}

// Kiểm tra một giá trị media (data URL / đường dẫn nội bộ / https trong danh sách trắng) và giới hạn dung lượng.
static bool validateMediaValue(const std::string& kindIn, const std::string& value, bool isAvatar, std::string& err) {
    if (value.empty()) return true;
    std::string kind = kindIn;
    if (isAvatar) kind = "IMAGE";
    if (kind != "IMAGE" && kind != "VIDEO" && kind != "AUDIO" && kind != "FILE") { err = "Loại tệp đính kèm không hợp lệ."; return false; }
    std::vector<std::string> items;
    if (kind == "IMAGE" && !isAvatar && value.front() == '[') {
        const json arr = json::parse(value, nullptr, false);
        if (!arr.is_array() || arr.empty() || arr.size() > 3) { err = "Danh sách hình ảnh không hợp lệ (tối đa 3 ảnh)."; return false; }
        for (const auto& it : arr) {
            if (!it.is_string()) { err = "Danh sách hình ảnh không hợp lệ."; return false; }
            items.push_back(it.get<std::string>());
        }
    } else {
        items.push_back(value);
    }
    for (const auto& it : items) {
        if (!sec::isSafeMediaUrl(it, kind)) { err = "Tệp đính kèm không hợp lệ hoặc không được phép."; return false; }
        const size_t sz = getBase64DataUrlSize(it);
        const size_t lim = isAvatar ? 3ULL * 1024 * 1024 : (kind == "VIDEO" ? gMaxVideoBytes : gMaxImageBytes);
        if (sz > lim) { err = "Tệp quá lớn (tối đa " + std::to_string(lim / (1024 * 1024)) + " MB)."; return false; }
    }
    return true;
}

// Tầng bảo vệ chạy SAU khi httplib đã đọc xong body (pre-routing chạy trước nên không thấy body).
// Trả false nếu đã trả lỗi cho client.
static bool guardApi(Database& db, const httplib::Request& req, httplib::Response& res) {
    const std::string& path = req.path;
    const AuthMode mode = authModeFor(path);
    const bool unsafe = req.method != "GET" && req.method != "HEAD";

    if (unsafe && !originAllowed(req)) {
        sendJsonError(res, 403, "Yêu cầu bị từ chối (nguồn gốc không hợp lệ).");
        return false;
    }
    if (unsafe && req.body.size() > (isUploadPath(path) ? gMaxBodyBytes : 256 * 1024)) {
        sendJsonError(res, 413, "Dữ liệu gửi lên quá lớn.");
        return false;
    }

    json body;
    bool hasBody = false;
    if (!req.body.empty() && req.body[0] == '{') {
        body = json::parse(req.body, nullptr, false);
        hasBody = body.is_object();
    }

    std::string uid;
    if (mode == AuthMode::Session || mode == AuthMode::SessionOrAdmin) {
        const std::string token = requestSessionToken(req);
        bool ok = !token.empty() && db.getSessionUser(token, uid);
        if (!ok && mode == AuthMode::SessionOrAdmin) {
            const std::string aid = requestAdminId(req, body);
            if (!aid.empty() && db.verifyAdminToken(aid, req.get_header_value("X-Admin-Token"))) { uid = aid; ok = true; }
        }
        if (!ok) {
            clearSessionCookie(req, res);
            sendJsonError(res, 401, "Phiên đăng nhập đã hết hạn hoặc không hợp lệ. Vui lòng đăng nhập lại.");
            return false;
        }
    }

    if (!uid.empty()) {
        const RateRule rr = userRateRule(req.method, path);
        if (!gRate.allow(std::string("u:") + rr.cls + ":" + uid, rr.cap, rr.refill)) {
            sendTooMany(res, static_cast<int>(1.0 / std::max(rr.refill, 0.01)) + 1);
            return false;
        }
    }

    if (mode == AuthMode::Session || mode == AuthMode::SessionOrAdmin) {
        // Danh tính người thao tác trong query/body phải là chủ phiên (không thể mạo danh bằng cách đổi my_id/user_id...)
        for (const auto& kv : req.params) {
            if (isActorKey(req.method, path, kv.first) && !kv.second.empty() && kv.second != uid) {
                sendJsonError(res, 403, "Danh tính yêu cầu không khớp với phiên đăng nhập.");
                return false;
            }
        }
        if (hasBody) {
            for (auto it = body.begin(); it != body.end(); ++it) {
                if (!isActorKey(req.method, path, it.key())) continue;
                if (it.value().is_null()) continue;
                std::string v;
                if (it.value().is_string()) v = it.value().get<std::string>();
                else { sendJsonError(res, 400, "Dữ liệu danh tính không hợp lệ."); return false; }
                if (!v.empty() && v != uid) {
                    sendJsonError(res, 403, "Danh tính yêu cầu không khớp với phiên đăng nhập.");
                    return false;
                }
            }
        }
        // Admin không tham gia chức năng mạng xã hội (cả làm đối tượng): kiểm tra trên cả query lẫn body
        if (mode == AuthMode::Session && requestInvolvesAdmin(db, req, hasBody ? &body : nullptr)) {
            sendJsonError(res, 403, "Tài khoản quản trị viên không tham gia các chức năng mạng xã hội.");
            return false;
        }
        // Kiểm tra quyền xem tài nguyên chỉ có ID (không có tham số danh tính)
        if (path == "/api/groups/members" && !db.isGroupMember(req.get_param_value("group_id"), uid)) {
            sendJsonError(res, 403, "Bạn không phải thành viên của nhóm này.");
            return false;
        }
        if (path == "/api/community-groups/posts/comments" && !db.canViewCommunityPost(req.get_param_value("post_id"), uid)) {
            sendJsonError(res, 403, "Bạn không có quyền xem nội dung này.");
            return false;
        }
    }

    // Làm sạch dữ liệu đầu vào tại một chỗ: tên/tiêu đề 1 dòng, văn bản dài, URL media
    if (hasBody && mode != AuthMode::AdminToken && mode != AuthMode::Public) {
        static const std::map<std::string, size_t> lineFields = {
            {"fullname", 80}, {"name", 100}, {"nickname", 50}, {"hometown", 100}, {"school", 120}, {"workplace", 120},
            {"residence", 120}, {"interests", 300}, {"title", 200}, {"gender", 20}, {"birthday", 20}, {"category_label", 60}};
        static const std::map<std::string, size_t> textFields = {
            {"content", 10000}, {"bio", 500}, {"description", 1000}, {"thoughts", 2000}, {"comment", 2000},
            {"reason", 1000}, {"note", 1000}, {"detail", 1000}};
        bool changed = false;
        std::string mediaType;
        if (body.contains("media_type") && body["media_type"].is_string()) mediaType = body["media_type"].get<std::string>();
        for (auto it = body.begin(); it != body.end(); ++it) {
            if (!it.value().is_string()) continue;
            const std::string& key = it.key();
            const std::string val = it.value().get<std::string>();
            auto lf = lineFields.find(key);
            auto tf = textFields.find(key);
            if (lf != lineFields.end()) {
                const std::string clean = sec::sanitizeLine(val, lf->second);
                if (clean != val) { it.value() = clean; changed = true; }
            } else if (tf != textFields.end()) {
                if (sec::utf8Length(val) > tf->second) {
                    sendJsonError(res, 400, "Nội dung quá dài (tối đa " + std::to_string(tf->second) + " ký tự).");
                    return false;
                }
                const std::string clean = sec::sanitizeText(val, tf->second);
                if (clean != val) { it.value() = clean; changed = true; }
            } else if (key == "media_url" || key == "avatar") {
                std::string err;
                if (!validateMediaValue(key == "avatar" ? "IMAGE" : mediaType, val, key == "avatar", err)) {
                    sendJsonError(res, 400, err);
                    return false;
                }
            }
        }
        if (changed) const_cast<httplib::Request&>(req).body = body.dump();  // httplib truyền Request không-const; chỉ khai báo const
    }
    return true;
}

// ID chủ phiên hiện tại (rỗng nếu chưa đăng nhập). Dùng khi handler cần quyền hạn mà client không (hoặc không bắt buộc) gửi danh tính.
static std::string sessionUid(Database& db, const httplib::Request& req) {
    std::string uid;
    const std::string tok = requestSessionToken(req);
    if (!tok.empty() && db.getSessionUser(tok, uid)) return uid;
    return "";
}

// Bọc mọi route: guardApi chạy trước handler gốc
struct GuardedRouter {
    httplib::Server& svr;
    Database& db;
    template <class F> void Get(const std::string& p, F f) { svr.Get(p, wrap(f)); }
    template <class F> void Post(const std::string& p, F f) { svr.Post(p, wrap(f)); }
    template <class F> void Put(const std::string& p, F f) { svr.Put(p, wrap(f)); }
    template <class F> void Delete(const std::string& p, F f) { svr.Delete(p, wrap(f)); }

private:
    template <class F> httplib::Server::Handler wrap(F f) {
        Database* d = &db;
        return [d, f](const httplib::Request& rq, httplib::Response& rs) {
            if (!guardApi(*d, rq, rs)) return;
            f(rq, rs);
        };
    }
};


int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "============================================================" << std::endl;
    std::cout << "  MẠNG XÃ HỘI MINI C++ (Web Platform & SQL Database)" << std::endl;
    std::cout << "  Xác thực Gmail OTP 6 số | Thuật toán gợi ý bạn bè BFS" << std::endl;
    std::cout << "  Gemini AI kiểm duyệt | News Feed Ranking" << std::endl;
    std::cout << "============================================================" << std::endl;

    // Nạp .env vào môi trường tiến trình (biến môi trường thật được ưu tiên), rồi cấu hình bảo mật
    sec::loadDotEnv(".env");
    gTrustProxy = sec::envFlag("TRUST_PROXY");
    gCookieSecure = sec::envFlag("COOKIE_SECURE");
    gGoogleClientId = sec::envOr("GOOGLE_CLIENT_ID");
    gMaxBodyBytes = static_cast<size_t>(std::max<long long>(1, sec::envInt("MAX_BODY_MB", 48))) * 1024 * 1024;
    gMaxVideoBytes = static_cast<size_t>(std::max<long long>(1, sec::envInt("MAX_VIDEO_MB", 30))) * 1024 * 1024;
    gMaxImageBytes = static_cast<size_t>(std::max<long long>(1, sec::envInt("MAX_IMAGE_MB", 8))) * 1024 * 1024;
    {
        std::stringstream ss(sec::envOr("ALLOWED_ORIGINS"));
        std::string o;
        while (std::getline(ss, o, ',')) { o = trimStr(o); if (!o.empty()) gAllowedOrigins.insert(o); }
    }

    // Nạp cấu hình Gemini từ .env (không hardcode API key)
    GeminiService::instance().loadEnv(".env");

    Database db;
    if (!db.init("data/social_network.db")) {
        std::cerr << "Lỗi: Không thể khởi tạo SQLite Database!" << std::endl;
        return 1;
    }
    std::cout << "[Database] SQLite Database đã sẵn sàng tại data/social_network.db" << std::endl;

    if (db.hasWeakAdminPassword() && sec::envFlag("PRODUCTION")) {
        std::cerr << "[BẢO MẬT] PRODUCTION=1 nhưng admin vẫn dùng mật khẩu mặc định. Đặt ADMIN_PASSWORD trong .env rồi chạy lại." << std::endl;
        return 2;
    }
    db.purgeExpiredSessions();

    httplib::Server svr;
    svr.set_payload_max_length(gMaxBodyBytes);
    // Chống slowloris / giữ kết nối: timeout ngắn, giới hạn số request trên một kết nối, hàng đợi có trần
    svr.set_read_timeout(15, 0);
    svr.set_write_timeout(15, 0);
    svr.set_keep_alive_max_count(100);
    svr.set_keep_alive_timeout(5);
    const size_t workerThreads = static_cast<size_t>(std::max<long long>(4, sec::envInt("WORKER_THREADS", 16)));
    svr.new_task_queue = [workerThreads] { return new httplib::ThreadPool(workerThreads, 512); };

    svr.set_mount_point("/", "./static");
    // Luôn kiểm tra lại file tĩnh (HTML/CSS/JS) với server để cập nhật giao diện ngay, tránh dùng bản cache cũ
    svr.set_file_request_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-cache");
    });
    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr) {
        sendJsonError(res, 500, "Lỗi máy chủ. Vui lòng thử lại sau.");
    });

    // Chạy TRƯỚC khi đọc body: header bảo mật, CORS mặc định same-origin, rate limit theo IP, chặn body quá lớn
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        applySecurityHeaders(req, res);
        const std::string ip = clientIp(req);
        const bool isApi = req.path.rfind("/api/", 0) == 0;

        const std::string origin = req.get_header_value("Origin");
        if (!origin.empty() && gAllowedOrigins.count(origin)) {  // CORS chỉ mở cho origin được cấu hình rõ ràng
            res.set_header("Access-Control-Allow-Origin", origin);
            res.set_header("Access-Control-Allow-Credentials", "true");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, X-Admin-Token");
            res.set_header("Vary", "Origin");
        }
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        if (!isApi && req.method != "GET" && req.method != "HEAD") {
            sendJsonError(res, 405, "Phương thức không được hỗ trợ.");
            return httplib::Server::HandlerResponse::Handled;
        }
        if (!isApi && (req.path.find("/.") != std::string::npos || req.path.find("..") != std::string::npos)) {
            res.status = 404;
            return httplib::Server::HandlerResponse::Handled;
        }

        const bool heavy = isApi && req.method == "GET" && isHeavyGet(req.path);
        if (!gRate.allow("ip:" + ip, 600, 150, heavy ? 5.0 : 1.0)) {
            sendTooMany(res, 5);
            return httplib::Server::HandlerResponse::Handled;
        }
        if (isApi && req.method != "GET" && req.method != "HEAD") {
            if (!gRate.allow("ipw:" + ip, 200, 30)) { sendTooMany(res, 5); return httplib::Server::HandlerResponse::Handled; }
            size_t cl = 0;
            const std::string clh = req.get_header_value("Content-Length");
            if (!clh.empty() && clh.size() < 15) { try { cl = std::stoull(clh); } catch (...) { cl = 0; } }
            const size_t limit = isUploadPath(req.path) ? gMaxBodyBytes : 256 * 1024;
            if (cl > limit) {
                sendJsonError(res, 413, "Dữ liệu gửi lên quá lớn.");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    GuardedRouter R{svr, db};

    // Cấu hình công khai cho frontend (Client ID của Google KHÔNG phải bí mật)
    R.Get("/api/auth/config", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({{"status", "ok"}, {"google_client_id", gGoogleClientId}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 1. AUTHENTICATION & GMAIL OTP 6-DIGIT VERIFICATION
    // -------------------------------------------------------------
    R.Post("/api/auth/send-otp", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const std::string ip = clientIp(req);
            if (!gRate.allow("otp-ip:" + ip, 5, 1.0 / 120)) { sendTooMany(res, 120, "Bạn yêu cầu mã quá nhiều lần. Vui lòng thử lại sau vài phút."); return; }
            auto body = json::parse(req.body);
            if (!hasAcceptedLegalTerms(body)) {
                sendJsonError(res, 400, "Vui lòng đồng ý với Chính sách quyền riêng tư và Điều khoản sử dụng trước khi tiếp tục.");
                return;
            }
            const std::string email = sec::toLowerAscii(trimStr(getJsonStringSafe(body, "email")));
            const std::string purpose = getJsonStringSafe(body, "purpose", "REGISTER");

            if (!sec::isValidEmail(email)) { sendJsonError(res, 400, "Địa chỉ email không hợp lệ!"); return; }
            if (purpose != "REGISTER") { sendJsonError(res, 400, "Mục đích xác thực không hợp lệ!"); return; }
            if (!gRate.allow("otp-mail:" + email, 3, 1.0 / 60)) { sendTooMany(res, 60, "Vui lòng đợi khoảng 1 phút trước khi yêu cầu mã mới."); return; }

            User existing;
            if (db.getUserByUsernameOrEmail(email, existing)) { sendJsonError(res, 400, "Email này đã được đăng ký!"); return; }

            const std::string otpCode = SmtpService::generate6DigitOtp();
            if (!db.saveOtp(email, otpCode, purpose, 300)) { sendJsonError(res, 500, "Không thể tạo mã xác thực. Vui lòng thử lại."); return; }
            if (!SmtpService::sendOtpEmail(email, otpCode, "Xác thực đăng ký tài khoản")) {
                sendJsonError(res, 503, "Không gửi được mã OTP. Kiểm tra cấu hình SMTP_EMAIL, SMTP_PASSWORD và Gmail App Password trên máy chủ.");
                return;
            }
            json resp = {{"status", "ok"}, {"message", "Mã xác thực 6 chữ số đã được gửi tới: " + email}};
            // CHỈ dùng khi phát triển cục bộ. Trả mã OTP về client = ai cũng đăng ký được bằng email của người khác.
            if (sec::envFlag("DEV_SHOW_OTP") && !sec::envFlag("PRODUCTION")) resp["code_for_user"] = otpCode;
            res.set_content(resp.dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Yêu cầu không hợp lệ.");
        }
    });

    R.Post("/api/auth/register-with-otp", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const std::string ip = clientIp(req);
            // Chống tạo hàng loạt tài khoản clone từ một địa chỉ IP: tối đa MAX_REGISTER_PER_IP_PER_DAY tài khoản / 24 giờ / IP (mặc định 10)
            const double maxPerDay = static_cast<double>(sec::envInt("MAX_REGISTER_PER_IP_PER_DAY", 10));
            if (!gRate.allow("regtry-ip:" + ip, 20, 20.0 / 3600)) { sendTooMany(res, 600); return; }
            if (!gRate.peek("reg-ip:" + ip, maxPerDay, maxPerDay / 86400.0)) { sendTooMany(res, 3600, "Địa chỉ này đã đăng ký quá nhiều tài khoản hôm nay. Vui lòng thử lại sau."); return; }
            auto body = json::parse(req.body);
            if (!hasAcceptedLegalTerms(body)) {
                sendJsonError(res, 400, "Vui lòng đồng ý với Chính sách quyền riêng tư và Điều khoản sử dụng trước khi tiếp tục.");
                return;
            }
            const std::string email = sec::toLowerAscii(trimStr(getJsonStringSafe(body, "email")));
            const std::string code = trimStr(getJsonStringSafe(body, "code"));
            const std::string username = trimStr(getJsonStringSafe(body, "username"));
            const std::string password = getJsonStringSafe(body, "password");
            const std::string fullname = sec::sanitizeLine(getJsonStringSafe(body, "fullname"), 80);

            if (email.empty() || code.empty() || username.empty() || password.empty() || fullname.empty()) {
                sendJsonError(res, 400, "Vui lòng nhập đầy đủ các trường thông tin và mã 6 số!");
                return;
            }
            if (!sec::isValidEmail(email)) { sendJsonError(res, 400, "Địa chỉ email không hợp lệ!"); return; }
            if (!sec::isValidUsername(username)) { sendJsonError(res, 400, "Tên đăng nhập chỉ gồm chữ, số, dấu _ hoặc . (3-30 ký tự)!"); return; }
            {
                static const std::set<std::string> reserved = {"admin", "administrator", "root", "system", "support", "moderator", "mod", "staff", "quantri"};
                if (reserved.count(sec::toLowerAscii(username))) { sendJsonError(res, 400, "Tên đăng nhập này không được phép sử dụng!"); return; }
            }
            if (sec::utf8Length(fullname) < 2) { sendJsonError(res, 400, "Họ tên không hợp lệ!"); return; }
            std::string why;
            if (!sec::isStrongPassword(password, &why)) { sendJsonError(res, 400, why); return; }

            double retry = 0;
            if (gFails.isLocked("otp:" + email, &retry)) { sendTooMany(res, static_cast<int>(retry) + 1, "Nhập sai mã quá nhiều lần. Vui lòng thử lại sau."); return; }
            if (!db.verifyOtp(email, code, "REGISTER")) {
                gFails.recordFail("otp:" + email, 10, 900, 900);
                sendJsonError(res, 400, "Mã xác thực 6 chữ số không đúng hoặc đã hết hạn!");
                return;
            }

            User existing;
            if (db.getUserByUsernameOrEmail(username, existing)) { sendJsonError(res, 400, "Tên đăng nhập đã tồn tại trong hệ thống!"); return; }
            if (db.getUserByUsernameOrEmail(email, existing)) { sendJsonError(res, 400, "Email đã được sử dụng bởi tài khoản khác!"); return; }

            User newUser;
            newUser.id = db.generateNewUserId();
            newUser.username = username;
            newUser.email = email;
            newUser.password_hash = sec::hashPassword(password);
            newUser.fullname = fullname;
            newUser.avatar = "/img/default-avatar.svg";
            newUser.bio = "Chào mừng tôi đến với Mạng Xã Hội Mini!";
            newUser.role = "USER";
            newUser.status = "ACTIVE";

            if (!db.createUser(newUser)) { sendJsonError(res, 500, "Lỗi lưu tài khoản vào cơ sở dữ liệu!"); return; }
            gRate.allow("reg-ip:" + ip, maxPerDay, maxPerDay / 86400.0);  // chỉ tính vào hạn mức khi đăng ký thành công
            const std::string token = db.createUserSession(newUser.id, ip, req.get_header_value("User-Agent"));
            if (!token.empty()) setSessionCookie(req, res, token);
            res.set_content(json({{"status", "ok"}, {"message", "Xác thực Gmail và đăng ký tài khoản thành công!"}, {"user", newUser.toJson(true)}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Yêu cầu không hợp lệ.");
        }
    });

    R.Post("/api/auth/login", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const std::string ip = clientIp(req);
            auto body = json::parse(req.body);
            if (!hasAcceptedLegalTerms(body)) {
                sendJsonError(res, 400, "Vui lòng đồng ý với Chính sách quyền riêng tư và Điều khoản sử dụng trước khi tiếp tục.");
                return;
            }
            const std::string identifier = trimStr(getJsonStringSafe(body, "identifier"));
            const std::string password = getJsonStringSafe(body, "password");
            if (identifier.empty() || password.empty() || identifier.size() > 254 || password.size() > 128) {
                sendJsonError(res, 400, "Vui lòng nhập tài khoản và mật khẩu hợp lệ!");
                return;
            }
            const std::string idLower = sec::toLowerAscii(identifier);
            const std::string kIpAcct = "lg:" + ip + "|" + idLower, kIp = "lgip:" + ip, kAcct = "lgacct:" + idLower;
            double retry = 0;
            if (gFails.isLocked(kIpAcct, &retry) || gFails.isLocked(kIp, &retry) || gFails.isLocked(kAcct, &retry)) {
                sendTooMany(res, static_cast<int>(retry) + 1, "Đăng nhập sai quá nhiều lần. Vui lòng thử lại sau " + std::to_string(static_cast<int>(retry / 60) + 1) + " phút.");
                return;
            }
            // Mỗi lần băm PBKDF2 tốn CPU nên giới hạn thêm theo IP (chống làm quá tải bằng cách spam đăng nhập)
            if (!gRate.allow("login-ip:" + ip, 20, 20.0 / 300)) { sendTooMany(res, 15); return; }

            User user;
            const bool found = db.getUserByUsernameOrEmail(identifier, user);
            static const std::string dummyHash = sec::hashPassword("dummy-password-for-timing");
            const bool ok = found ? sec::verifyPassword(password, user.password_hash) : (sec::verifyPassword(password, dummyHash), false);
            if (!ok) {
                gFails.recordFail(kIpAcct, 5, 900, 900);
                gFails.recordFail(kIp, 30, 900, 900);
                gFails.recordFail(kAcct, 40, 3600, 900);
                // Cùng một thông báo cho "sai tài khoản" và "sai mật khẩu" để không lộ tài khoản nào tồn tại
                sendJsonError(res, 401, "Tài khoản hoặc mật khẩu không chính xác!");
                return;
            }
            gFails.reset(kIpAcct);
            if (user.status == "LOCKED") { sendJsonError(res, 403, "Tài khoản của bạn đã bị khóa bởi Quản trị viên!"); return; }
            if (sec::needsRehash(user.password_hash)) db.updatePassword(user.id, sec::hashPassword(password));

            db.updateHeartbeat(user.id);
            json loginResp = {{"status", "ok"}, {"message", "Đăng nhập thành công!"}, {"user", user.toJson(true)}};
            if (user.role == "ADMIN") {
                loginResp["admin_token"] = db.issueAdminToken(user.id);  // admin chỉ dùng token quản trị, không có phiên mạng xã hội
            } else {
                const std::string token = db.createUserSession(user.id, ip, req.get_header_value("User-Agent"));
                if (token.empty()) { sendJsonError(res, 500, "Không thể tạo phiên đăng nhập."); return; }
                setSessionCookie(req, res, token);
            }
            res.set_content(loginResp.dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Yêu cầu không hợp lệ.");
        }
    });

    R.Post("/api/auth/logout", [&db](const httplib::Request& req, httplib::Response& res) {
        db.deleteUserSession(requestSessionToken(req));
        const std::string adminTok = req.get_header_value("X-Admin-Token");
        if (!adminTok.empty()) db.revokeAdminToken(adminTok);
        clearSessionCookie(req, res);
        res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
    });

    // Đăng nhập Google: server TỰ xác minh ID token với Google (bản cũ tin email do client gửi lên -> ai cũng đăng nhập được tài khoản bất kỳ)
    R.Post("/api/auth/google-login", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            if (gGoogleClientId.empty()) { sendJsonError(res, 501, "Đăng nhập bằng Google chưa được bật trên máy chủ (thiếu GOOGLE_CLIENT_ID)."); return; }
            const std::string ip = clientIp(req);
            if (!gRate.allow("google-ip:" + ip, 10, 10.0 / 300)) { sendTooMany(res, 30); return; }
            auto body = json::parse(req.body);
            if (!hasAcceptedLegalTerms(body)) {
                sendJsonError(res, 400, "Vui lòng đồng ý với Chính sách quyền riêng tư và Điều khoản sử dụng trước khi tiếp tục.");
                return;
            }
            const std::string credential = getJsonStringSafe(body, "credential");
            if (credential.size() < 20 || credential.size() > 4096) { sendJsonError(res, 400, "Thông tin tài khoản Google không hợp lệ!"); return; }
            for (unsigned char c : credential) {
                if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-')) { sendJsonError(res, 400, "Thông tin tài khoản Google không hợp lệ!"); return; }
            }
            const std::string tmp = "google_" + sec::randomToken(8) + ".json";
            const std::string cmd = "curl -s --max-time 10 \"https://oauth2.googleapis.com/tokeninfo?id_token=" + credential + "\" -o " + tmp;
            std::system(cmd.c_str());
            std::string raw;
            { std::ifstream f(tmp, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); raw = ss.str(); }
            std::remove(tmp.c_str());
            const json tok = json::parse(raw, nullptr, false);
            if (!tok.is_object() || getJsonStringSafe(tok, "aud") != gGoogleClientId ||
                (getJsonStringSafe(tok, "iss") != "accounts.google.com" && getJsonStringSafe(tok, "iss") != "https://accounts.google.com") ||
                getJsonStringSafe(tok, "email_verified") != "true") {
                sendJsonError(res, 401, "Không xác minh được tài khoản Google!");
                return;
            }
            long long exp = 0;
            try { exp = std::stoll(getJsonStringSafe(tok, "exp", "0")); } catch (...) {}
            if (exp <= sec::epochSeconds()) { sendJsonError(res, 401, "Phiên Google đã hết hạn, vui lòng thử lại!"); return; }
            const std::string email = sec::toLowerAscii(getJsonStringSafe(tok, "email"));
            if (!sec::isValidEmail(email)) { sendJsonError(res, 400, "Email Google không hợp lệ!"); return; }

            User user;
            bool isNew = false;
            if (db.getUserByUsernameOrEmail(email, user)) {
                if (user.role == "ADMIN") { sendJsonError(res, 403, "Tài khoản quản trị viên chỉ đăng nhập bằng mật khẩu quản trị."); return; }
                if (user.status == "LOCKED") { sendJsonError(res, 403, "Tài khoản này đã bị khóa!"); return; }
            } else {
                isNew = true;
                std::string base;
                for (char c : email.substr(0, email.find('@'))) if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') base += c;
                if (base.size() < 3) base += "user";
                base = base.substr(0, 22);
                user = User();
                user.id = db.generateNewUserId();
                user.username = base + "_" + sec::randomDigits6().substr(0, 4);
                user.email = email;
                user.password_hash = "!";  // không có mật khẩu: chỉ đăng nhập qua Google
                user.fullname = sec::sanitizeLine(getJsonStringSafe(tok, "name", base), 80);
                if (user.fullname.empty()) user.fullname = base;
                const std::string pic = getJsonStringSafe(tok, "picture");
                user.avatar = (!pic.empty() && sec::isSafeMediaUrl(pic, "IMAGE")) ? pic : "/img/default-avatar.svg";
                user.bio = "Gia nhập qua tài khoản Google";
                user.google_id = getJsonStringSafe(tok, "sub");
                user.role = "USER";
                user.status = "ACTIVE";
                if (!db.createUser(user)) { sendJsonError(res, 500, "Không thể tạo tài khoản từ Google!"); return; }
            }
            db.updateHeartbeat(user.id);
            const std::string token = db.createUserSession(user.id, ip, req.get_header_value("User-Agent"));
            if (token.empty()) { sendJsonError(res, 500, "Không thể tạo phiên đăng nhập."); return; }
            setSessionCookie(req, res, token);
            res.set_content(json({{"status", "ok"}, {"message", isNew ? "Tạo tài khoản Google mới thành công!" : "Đăng nhập Google thành công!"}, {"user", user.toJson(true)}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Yêu cầu không hợp lệ.");
        }
    });

    // Heartbeat để xác định ai thực sự đang online (mỗi tab có session_id riêng)
    R.Post("/api/users/heartbeat", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string sessionId = body.value("session_id", "");
            User u;
            if (!userId.empty() && db.getUserById(userId, u) && u.status != "LOCKED") {
                PresenceRegistry::instance().touch(u.id, sessionId);
                db.updateHeartbeat(u.id);
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (...) {
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Đăng xuất / đóng tab: gỡ session ngay -> người dùng chuyển sang OFFLINE tức thì
    // (all=true: gỡ mọi session của tài khoản, dùng khi đăng xuất)
    R.Post("/api/users/offline", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string sessionId = body.value("session_id", "");
            bool all = body.value("all", false);
            User u;
            if (!userId.empty() && db.getUserById(userId, u)) {
                PresenceRegistry::instance().remove(u.id, all ? "" : sessionId);
                if (all && u.role == "ADMIN" && db.verifyAdminToken(u.id, req.get_header_value("X-Admin-Token"))) {
                    db.revokeAdminToken(req.get_header_value("X-Admin-Token"));
                }
                db.updateHeartbeat(u.id); // ghi nhận thời điểm hoạt động cuối
            }
        } catch (...) {}
        res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
    });

    // Danh sách bạn bè đang ONLINE (frontend poll vài giây/lần để cập nhật chấm xanh)
    R.Get("/api/presence/friends", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        db.syncPendingDeliveriesForUser(userId);
        json online = json::array();
        for (const auto& fid : db.getFriendsList(userId)) {
            if (PresenceRegistry::instance().isOnline(fid)) online.push_back(fid);
        }
        res.set_header("Cache-Control", "no-store");
        res.set_content(json({{"status", "ok"}, {"online_ids", online}}).dump(), "application/json; charset=utf-8");
    });

    // Trạng thái đang soạn tin nhắn (Typing Indicator)
    R.Post("/api/users/typing", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string partnerId = body.value("partner_id", "");
            bool isTyping = body.value("is_typing", false);
            if (!userId.empty() && !partnerId.empty()) {
                db.setTypingStatus(userId, partnerId, isTyping);
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (...) {
            res.status = 400;
            res.set_content(json({{"status", "error"}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/users/typing", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string partnerId = req.get_param_value("partner_id");
        if (userId.empty() || partnerId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        bool isTyping = db.getTypingStatus(userId, partnerId);
        res.set_content(json({{"status", "ok"}, {"is_typing", isTyping}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 2. GỢI Ý KẾT BẠN (BFS GRAPH) & LỜI MỜI KẾT BẠN
    // -------------------------------------------------------------
    R.Get("/api/friends/suggest", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string filter = req.has_param("filter") ? req.get_param_value("filter") : "ALL";
        int limit = paramInt(req, "limit", 8, 1, 100);

        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        std::vector<Suggestion> list = SuggestService::getSuggestions(db, userId, limit, filter);
        json arr = json::array();
        for (const auto& s : list) {
            arr.push_back(s.toJson());
        }

        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"suggestions", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Get("/api/friends/full", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        auto friends = db.getFriendsFull(userId);
        json arr = json::array();
        for (const auto& u : friends) {
            arr.push_back(u.toJson());
        }
        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"friends", arr}}).dump(), "application/json; charset=utf-8");
    });

    // Gửi lời mời kết bạn (Cần bên kia chấp nhận mới thành bạn bè)
    R.Post("/api/friends/request", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string myId = body.value("my_id", "");
            std::string targetId = body.value("target_id", "");

            if (myId.empty() || targetId.empty() || myId == targetId) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu kết bạn không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.areUsersBlockedEitherWay(myId, targetId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không thể gửi lời mời kết bạn vì bạn hoặc người nhận đã chặn nhau."}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.isUserAdmin(myId) || db.isUserAdmin(targetId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Tài khoản quản trị viên không tham gia kết bạn."}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.sendFriendRequest(myId, targetId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã gửi lời mời kết bạn thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể gửi lời mời kết bạn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Phản hồi lời mời kết bạn (ACCEPT / REJECT)
    R.Post("/api/friends/respond", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string receiverId = body.value("my_id", "");
            std::string requesterId = body.value("target_id", "");
            std::string action = body.value("action", "ACCEPT"); // ACCEPT hoặc REJECT

            if (db.respondFriendRequest(receiverId, requesterId, action)) {
                std::string msg = (action == "ACCEPT") ? "Đã chấp nhận kết bạn!" : "Đã từ chối lời mời kết bạn!";
                res.set_content(json({{"status", "ok"}, {"message", msg}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi xử lý lời mời kết bạn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Hủy gửi lời mời kết bạn (Cancel friend request)
    R.Post("/api/friends/cancel", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string requesterId = getJsonStringSafe(body, "my_id");
            std::string receiverId = getJsonStringSafe(body, "target_id");
            if (requesterId.empty() || receiverId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu params!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.cancelFriendRequest(requesterId, receiverId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã hủy lời mời kết bạn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi hủy lời mời kết bạn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Hủy kết bạn (Unfriend)
    R.Post("/api/friends/unfriend", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string myId = getJsonStringSafe(body, "my_id");
            std::string friendId = getJsonStringSafe(body, "friend_id", getJsonStringSafe(body, "target_id"));
            if (myId.empty() || friendId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu params!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.removeFriend(myId, friendId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã hủy kết bạn thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi hủy kết bạn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Danh sách lời mời kết bạn gửi đến tôi
    R.Get("/api/friends/requests", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        auto list = db.getPendingRequests(userId);
        json arr = json::array();
        for (const auto& u : list) arr.push_back(u.toJson());
        res.set_content(json({{"status", "ok"}, {"requests", arr}}).dump(), "application/json; charset=utf-8");
    });

    // Lời mời kết bạn mình đã gửi
    R.Get("/api/friends/requests/sent", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        auto list = db.getSentFriendRequests(userId);
        json arr = json::array();
        for (const auto& u : list) arr.push_back(u.toJson());
        res.set_content(json({{"status", "ok"}, {"requests", arr}}).dump(), "application/json; charset=utf-8");
    });

    // Danh sách bạn bè kèm tin nhắn mới nhất và trạng thái online (Hình 4)
    R.Get("/api/friends/contacts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        json contacts = db.getFriendsWithLastMessage(userId);
        res.set_content(json({{"status", "ok"}, {"contacts", contacts}}).dump(), "application/json; charset=utf-8");
    });

    R.Get("/api/messages/pending", [&db](const httplib::Request& req, httplib::Response& res) {
        const std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        const json contacts = db.getPendingMessageContacts(userId);
        res.set_content(json({{"status", "ok"}, {"contacts", contacts}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 3. XEM PROFILE CỦA BẠN BÈ & BẢN THÂN
    // -------------------------------------------------------------
    R.Get("/api/users/profile", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string targetId = req.get_param_value("id");
        std::string myId = req.get_param_value("my_id");

        User user;
        if ((db.getUserById(targetId, user) || db.getUserByToken(targetId, user)) && user.role != "ADMIN") {
            std::string realId = user.id;
            std::string relation = "NONE";
            if (!myId.empty() && myId != realId) {
                relation = db.getFriendshipRelation(myId, realId);
            }
            auto friends = db.getFriendsList(realId);

            res.set_content(json({
                {"status", "ok"},
                {"user", user.toJson(!myId.empty() && myId == realId)},
                {"relation", relation},
                {"friend_count", friends.size()}
            }).dump(), "application/json; charset=utf-8");
        } else {
            res.status = 404;
            res.set_content(json({{"status", "error"}, {"message", "Không tìm thấy người dùng!"}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/users/update", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("id", "");
            User user;
            if (!db.getUserById(userId, user)) {
                res.status = 404;
                res.set_content(json({{"status", "error"}, {"message", "Người dùng không tồn tại!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (body.contains("fullname")) user.fullname = body["fullname"];
            if (body.contains("bio")) user.bio = body["bio"];
            if (body.contains("hometown")) user.hometown = body["hometown"];
            if (body.contains("school")) user.school = body["school"];
            if (body.contains("workplace")) user.workplace = body["workplace"];
            if (body.contains("interests")) user.interests = body["interests"];
            if (body.contains("avatar")) user.avatar = body["avatar"];
            if (body.contains("gender")) user.gender = body["gender"];
            if (body.contains("birthday")) user.birthday = body["birthday"];
            if (body.contains("residence")) user.residence = body["residence"];

            if (db.updateUser(user)) {
                res.set_content(json({{"status", "ok"}, {"message", "Cập nhật hồ sơ thành công!"}, {"user", user.toJson(true)}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật dữ liệu!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/users/change-password", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            const std::string userId = getJsonStringSafe(body, "user_id");   // đã được guard đối chiếu với chủ phiên
            const std::string oldPass = getJsonStringSafe(body, "old_password");
            const std::string newPass = getJsonStringSafe(body, "new_password");
            const std::string confirmPass = getJsonStringSafe(body, "confirm_password");

            if (userId.empty() || oldPass.empty() || newPass.empty() || confirmPass.empty()) { sendJsonError(res, 400, "Vui lòng nhập đầy đủ thông tin!"); return; }
            if (newPass != confirmPass) { sendJsonError(res, 400, "Mật khẩu xác nhận không khớp!"); return; }
            std::string why;
            if (!sec::isStrongPassword(newPass, &why)) { sendJsonError(res, 400, why); return; }

            const std::string failKey = "chgpw:" + userId;
            double retry = 0;
            if (gFails.isLocked(failKey, &retry)) { sendTooMany(res, static_cast<int>(retry) + 1, "Nhập sai mật khẩu cũ quá nhiều lần. Vui lòng thử lại sau."); return; }

            User u;
            if (!db.getUserById(userId, u)) { sendJsonError(res, 404, "Người dùng không tồn tại!"); return; }
            if (!sec::verifyPassword(oldPass, u.password_hash)) {
                gFails.recordFail(failKey, 5, 900, 900);
                sendJsonError(res, 400, "Mật khẩu cũ không chính xác!");
                return;
            }
            gFails.reset(failKey);
            // Bản cũ gọi updateUser() — hàm đó không ghi cột mật khẩu nên đổi mật khẩu chưa từng có tác dụng.
            if (!db.updatePassword(userId, sec::hashPassword(newPass))) { sendJsonError(res, 500, "Lỗi cập nhật mật khẩu trong CSDL!"); return; }
            // Đăng xuất mọi thiết bị khác, giữ lại phiên hiện tại
            db.deleteUserSessions(userId, requestSessionToken(req));
            res.set_content(json({{"status", "ok"}, {"message", "Đổi mật khẩu thành công!"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Yêu cầu không hợp lệ.");
        }
    });

    R.Post("/api/users/block", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string myId = body.value("my_id", "");
            std::string targetId = body.value("target_id", "");
            if (db.blockUser(myId, targetId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã chặn người dùng thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể chặn người dùng!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/users/unblock", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string myId = body.value("my_id", "");
            std::string targetId = body.value("target_id", "");
            if (myId.empty() || targetId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu thông tin!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.unblockUser(myId, targetId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã gỡ chặn người dùng!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể gỡ chặn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/users/blocked", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("user_id");
        if (myId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        json list = db.getBlockedUsers(myId);
        res.set_content(json({{"status", "ok"}, {"users", list}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 4. BÀI ĐĂNG (ẢNH, VIDEO, THẢ CẢM XÚC, CHIA SẺ, BÌNH LUẬN)
    // -------------------------------------------------------------
    R.Get("/api/posts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string currentUserId = req.get_param_value("user_id");
        int limit = paramInt(req, "limit", 20, 1, 100);
        int offset = paramInt(req, "offset", 0, 0, 1000000);

        auto posts = db.getFeedPosts(currentUserId, limit, offset);
        json arr = json::array();
        for (const auto& p : posts) arr.push_back(p.toJson());
        res.set_content(json({{"status", "ok"}, {"posts", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/posts/create", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string authorId = body.value("author_id", "");
            if (rejectIfAdmin(db, authorId, res, "Tài khoản quản trị viên không được đăng bài viết.")) return;
            std::string content = body.value("content", "");
            std::string title = body.value("title", "");
            std::string visibility = body.value("visibility", "PUBLIC");
            std::string mediaType = body.value("media_type", "NONE");
            std::string mediaUrl = body.value("media_url", "");
            std::string sharedPostId = body.value("shared_post_id", "");
            std::string sharedAuthorName = body.value("shared_author_name", "");
            std::string sharedContent = body.value("shared_content", "");

            if (title.length() > 100) title = title.substr(0, 100);
            if (content.length() > 1000) content = content.substr(0, 1000);

            std::string mediaError;
            if (!validatePostMedia(mediaType, mediaUrl, mediaError)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", mediaError}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (authorId.empty() || (content.empty() && mediaUrl.empty() && sharedPostId.empty() && title.empty())) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Nội dung bài viết không được để trống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            // --- Gemini AI: phân loại chủ đề + kiểm duyệt ---
            auto mod = GeminiService::instance().analyzePost(title, content);
            if (mod.action == "BLOCK") {
                res.status = 400;
                res.set_content(json({
                    {"status", "error"},
                    {"message", "Bài viết bị từ chối đăng do nội dung không phù hợp: " + mod.reason},
                    {"moderation", {
                        {"risk_level", mod.risk_level},
                        {"action", mod.action},
                        {"reason", mod.reason},
                        {"flags", mod.flags},
                        {"used_ai", mod.used_ai}
                    }}
                }).dump(), "application/json; charset=utf-8");
                return;
            }

            std::string modStatus = (mod.action == "FLAG") ? "FLAGGED" : "SAFE";
            std::string categoryTags;
            for (const auto& category : mod.categories) {
                if (!categoryTags.empty()) categoryTags += '|';
                categoryTags += category;
            }
            std::string outId;
            if (db.createPostWithMedia(authorId, content, mediaType, mediaUrl, sharedPostId, sharedAuthorName, sharedContent,
                                       title, visibility, categoryTags, mod.category_label, modStatus, mod.action, mod.reason, &outId)) {
                json resp = {
                    {"status", "ok"},
                    {"message", mod.action == "FLAG"
                        ? "Bài viết đã đăng nhưng được đánh dấu kiểm duyệt: " + mod.reason
                        : "Đã đăng bài viết thành công!"},
                    {"post_id", outId},
                    {"moderation", {
                        {"risk_level", mod.risk_level},
                        {"action", mod.action},
                        {"reason", mod.reason},
                        {"used_ai", mod.used_ai}
                    }}
                };
                res.set_content(resp.dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể lưu bài viết!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // ĐĂNG LẠI BÀI VIẾT (REPOST)
    // -------------------------------------------------------------
    R.Post("/api/posts/repost", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string postId = body.value("post_id", "");
            std::string thoughts = body.value("thoughts", "");
            std::string visibility = body.value("visibility", "PUBLIC");

            if (userId.empty() || postId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id hoặc post_id."}}).dump(), "application/json; charset=utf-8");
                return;
            }

            std::string newPostId;
            std::string errMessage;
            if (db.repostPost(userId, postId, thoughts, visibility, newPostId, errMessage)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã đăng lại bài viết thành công!"}, {"post_id", newPostId}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", errMessage}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/posts/edit", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = getJsonStringSafe(body, "post_id");
            std::string authorId = getJsonStringSafe(body, "author_id");
            std::string content = getJsonStringSafe(body, "content");
            std::string title = getJsonStringSafe(body, "title");
            std::string visibility = body.value("visibility", "PUBLIC");

            if (postId.empty() || authorId.empty() || content.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Nội dung bài viết không được để trống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.updatePost(postId, authorId, content, title, visibility)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã cập nhật bài viết thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể cập nhật bài viết!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Thả cảm xúc (LIKE, LOVE, HAHA, WOW, SAD, ANGRY, NONE)
    R.Post("/api/posts/reaction", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string userId = body.value("user_id", "");
            std::string reactionType = body.value("type", "LIKE");

            if (db.setReaction(postId, userId, reactionType)) {
                res.set_content(json({{"status", "ok"}, {"reaction", reactionType}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể cập nhật cảm xúc!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Danh sách người đã thả cảm xúc (Hình 2)
    R.Get("/api/posts/reactions-list", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string postId = req.get_param_value("post_id");
        std::string myId = req.get_param_value("user_id");
        if (postId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu post_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        json data = db.getPostReactionsDetail(postId, myId);
        res.set_content(data.dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/comments/reaction", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto body = json::parse(req.body);
            const std::string commentId = body.value("comment_id", "");
            const std::string userId = body.value("user_id", "");
            const std::string type = body.value("type", "");
            static const std::set<std::string> allowedTypes = {"LIKE", "LOVE", "HAHA", "WOW", "SAD", "ANGRY", "NONE"};
            if (commentId.empty() || userId.empty() || !allowedTypes.count(type)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Dữ liệu cảm xúc bình luận không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.setCommentReaction(commentId, userId, type)) {
                res.set_content(json({{"status", "ok"}, {"reaction", type}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể cập nhật cảm xúc bình luận!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/comments/reactions-list", [&db](const httplib::Request& req, httplib::Response& res) {
        const std::string commentId = req.get_param_value("comment_id");
        if (commentId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu comment_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        const std::string userId = req.get_param_value("user_id");
        res.set_content(db.getCommentReactionsDetail(commentId, userId).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/posts/comment", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string authorId = body.value("author_id", "");
            std::string content = body.value("content", "");
            std::string mediaUrl = body.value("media_url", "");
            std::string mediaType = body.value("media_type", "");
            std::string replyToId = body.value("reply_to_id", "");
            std::string replyToAuthor = body.value("reply_to_author", "");

            // Hỗ trợ bình luận GIF: lưu dạng __GIF__:<url>
            if (content.empty() && !mediaUrl.empty()) {
                content = std::string("__GIF__:") + mediaUrl;
            } else if (!mediaUrl.empty() && (mediaType == "IMAGE" || mediaType == "GIF")) {
                if (content.empty()) content = std::string("__GIF__:") + mediaUrl;
                else content = content + "\n__GIF__:" + mediaUrl;
            }

            if (content.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Bình luận không được để trống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            Comment newComment;
            if (db.addComment(postId, authorId, content, replyToId, replyToAuthor, newComment)) {
                User author;
                db.getUserById(authorId, author);
                newComment.author_name = author.fullname;
                newComment.author_avatar = author.avatar;

                res.set_content(json({{"status", "ok"}, {"comment", newComment.toJson()}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể lưu bình luận!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Xóa bình luận (Chỉ tác giả bình luận hoặc tác giả bài đăng mới được quyền xóa)
    auto deleteCommentHandler = [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string commentId = body.value("comment_id", "");
            std::string userId = body.value("user_id", "");

            if (commentId.empty() || userId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu comment_id hoặc user_id!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.deleteComment(commentId, userId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa bình luận thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền xóa bình luận này!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    };
    R.Post("/api/comments/delete", deleteCommentHandler);
    R.Delete("/api/comments/delete", deleteCommentHandler);

    // Xóa bài viết (Chỉ tác giả bài viết hoặc Admin mới được quyền xóa)
    auto deletePostHandler = [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string userId = body.value("user_id", "");

            if (postId.empty() || userId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu post_id hoặc user_id!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            Post p;
            if (!db.getPostById(postId, userId, p)) {
                res.status = 404;
                res.set_content(json({{"status", "error"}, {"message", "Bài viết không tồn tại!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            User u;
            db.getUserById(userId, u);
            if (p.author_id != userId && u.role != "ADMIN") {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền xóa bài viết này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.deletePost(postId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa bài viết thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa bài viết!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    };
    R.Post("/api/posts/delete", deletePostHandler);
    R.Delete("/api/posts/delete", deletePostHandler);

    // Lấy bài viết của riêng 1 người dùng (cho trang profile)
    R.Get("/api/users/posts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string authorId = req.get_param_value("author_id");
        std::string currentUserId = req.get_param_value("current_user_id");
        int limit = paramInt(req, "limit", 5, 1, 100);
        int offset = paramInt(req, "offset", 0, 0, 1000000);

        if (authorId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu author_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        User u;
        if (db.getUserByToken(authorId, u)) {
            authorId = u.id;
        }

        auto posts = db.getUserPosts(authorId, currentUserId, limit, offset);
        json arr = json::array();
        for (const auto& p : posts) arr.push_back(p.toJson());
        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"posts", arr}}).dump(), "application/json; charset=utf-8");
    });

    // Lấy chi tiết 1 bài viết kèm bình luận (cho Modal xem bình luận - Hình 2)
    R.Get("/api/posts/detail", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string postId = req.get_param_value("id");
        std::string currentUserId = req.get_param_value("current_user_id");

        if (postId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu id bài viết!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        Post post;
        if (db.getPostById(postId, currentUserId, post)) {
            res.set_content(json({{"status", "ok"}, {"post", post.toJson()}}).dump(), "application/json; charset=utf-8");
        } else {
            res.status = 404;
            res.set_content(json({{"status", "error"}, {"message", "Không tìm thấy bài viết!"}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 5. CHAT (30 TIN NHẮN, TẢI TIN CŨ, XÓA ĐOẠN CHAT)
    // -------------------------------------------------------------
    R.Get("/api/messages", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        int beforeId = paramInt(req, "before_id", 0, 0, 2147483647);
        int limit = paramInt(req, "limit", 30, 1, 100); // Tăng mặc định lên 30

        if (myId.empty() || partnerId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu my_id hoặc partner_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        if (!db.canAccessDirectConversation(myId, partnerId)) {
            res.status = 403;
            res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền truy cập cuộc trò chuyện này!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        bool hasMore = false;
        auto msgs = db.getPaginatedMessages(myId, partnerId, beforeId, limit, hasMore);
        json arr = json::array();
        for (const auto& m : msgs) arr.push_back(m.toJson());

        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"has_more", hasMore}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/chats/preferences", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto body = json::parse(req.body);
            const std::string userId = body.value("user_id", "");
            const std::string conversationId = body.value("conversation_id", "");
            const std::string action = body.value("action", "");
            const bool isGroup = body.value("is_group", false);
            if (userId.empty() || conversationId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu thông tin cuộc trò chuyện!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            bool authorized = false;
            if (isGroup) {
                authorized = !db.getGroupRole(conversationId, userId).empty();
            } else {
                const auto contacts = db.getFriendsWithLastMessage(userId);
                authorized = std::any_of(contacts.begin(), contacts.end(), [&](const json& contact) {
                    return contact.value("id", "") == conversationId;
                });
            }
            if (!authorized) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền cập nhật cuộc trò chuyện này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            bool ok = false;
            if (action == "archive") {
                ok = db.setConversationArchived(userId, conversationId, isGroup, true);
            } else if (action == "unarchive") {
                ok = db.setConversationArchived(userId, conversationId, isGroup, false);
            } else if (action == "mark_unread") {
                ok = db.setConversationForcedUnread(userId, conversationId, isGroup, true);
            } else if (action == "mark_read") {
                ok = db.setConversationForcedUnread(userId, conversationId, isGroup, false);
            } else {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thao tác cuộc trò chuyện không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (!ok) {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể cập nhật cuộc trò chuyện!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/messages/around", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        int targetId = 0;
        try { targetId = std::stoi(req.get_param_value("target_id")); } catch(...) {}
        bool isGroup = (req.get_param_value("is_group") == "true");

        if (isGroup) {
            if (!db.isGroupMember(partnerId, myId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không phải thành viên của nhóm này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            auto msgs = db.getGroupMessagesAround(partnerId, targetId, 15, myId);
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        } else {
            if (!db.canAccessDirectConversation(myId, partnerId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền truy cập cuộc trò chuyện này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            auto msgs = db.getMessagesAround(myId, partnerId, targetId, 15);
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Tìm kiếm tin nhắn không phân biệt chữ hoa, chữ thường, có dấu và không dấu
    R.Get("/api/messages/search", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        std::string query = req.get_param_value("q");
        bool isGroup = (req.get_param_value("is_group") == "true");
        int limit = 10;
        int offset = 0;
        try {
            limit = paramInt(req, "limit", limit, 1, 100);
            offset = paramInt(req, "offset", offset, 0, 1000000);
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Tham số phân trang không hợp lệ."}}).dump(), "application/json; charset=utf-8");
            return;
        }
        limit = std::max(1, std::min(limit, 50));
        offset = std::max(offset, 0);

        if (query.empty()) {
            res.set_content(json({{"status", "ok"}, {"count", 0}, {"has_more", false}, {"messages", json::array()}}).dump(), "application/json; charset=utf-8");
            return;
        }

        if (isGroup) {
            auto msgs = db.searchGroupMessages(partnerId, query, myId, limit + 1, offset);
            const bool hasMore = static_cast<int>(msgs.size()) > limit;
            if (hasMore) msgs.pop_back();
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"has_more", hasMore}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        } else {
            auto msgs = db.searchMessages(myId, partnerId, query, limit + 1, offset);
            const bool hasMore = static_cast<int>(msgs.size()) > limit;
            if (hasMore) msgs.pop_back();
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"has_more", hasMore}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/messages/search-all", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string query = req.get_param_value("q");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        json matches = db.searchAllMessages(userId, query);
        if (matches.is_object() && matches.contains("error")) {
            res.status = 500;
            res.set_content(json({{"status", "error"}, {"message", matches["error"]}}).dump(), "application/json; charset=utf-8");
            return;
        }
        res.set_content(json({{"status", "ok"}, {"count", matches.size()}, {"messages", matches}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/messages/send", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string senderId = body.value("sender_id", "");
            if (rejectIfAdmin(db, senderId, res, "Tài khoản quản trị viên không sử dụng tin nhắn cá nhân.")) return;
            std::string recipientId = body.value("recipient_id", "");
            std::string content = body.value("content", "");
            std::string mediaType = body.value("media_type", "NONE");
            std::string mediaUrl = body.value("media_url", "");
            std::string replyToText = body.value("reply_to_text", "");
            int replyToId = body.value("reply_to_id", 0);

            if (senderId.empty() || recipientId.empty() || (content.empty() && mediaUrl.empty())) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Nội dung tin nhắn không được để trống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.areUsersBlockedEitherWay(senderId, recipientId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không thể gửi tin nhắn vì bạn hoặc người nhận đã chặn nhau."}}).dump(), "application/json; charset=utf-8");
                return;
            }

            Message outMsg;
            if (db.sendMessage(senderId, recipientId, content, "", outMsg, mediaType, mediaUrl, replyToText, replyToId)) {
                db.setConversationArchived(senderId, recipientId, false, false);
                db.setConversationForcedUnread(senderId, recipientId, false, false);
                // Đánh dấu chưa đọc cho người nhận để hiện in đậm + dấu chấm xanh
                db.setConversationArchived(recipientId, senderId, false, false);
                db.setConversationForcedUnread(recipientId, senderId, false, true);
                User sender;
                db.getUserById(senderId, sender);
                outMsg.sender_name = sender.fullname;
                outMsg.sender_avatar = sender.avatar;
                db.updateHeartbeat(senderId);

                res.set_content(json({{"status", "ok"}, {"message", outMsg.toJson()}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể gửi tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/messages/receipts/read", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto body = json::parse(req.body);
            const std::string userId = body.value("user_id", "");
            const bool isGroup = body.value("is_group", false);
            const std::string conversationId = body.value("conversation_id", "");
            if (userId.empty() || conversationId.empty() || !body.contains("message_ids") ||
                !body["message_ids"].is_array() || body["message_ids"].empty() ||
                body["message_ids"].size() > 100) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Danh sách tin nhắn cần đánh dấu đã xem không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            std::vector<int> messageIds;
            for (const auto& value : body["message_ids"]) {
                if (!value.is_number_integer()) continue;
                const auto messageId = value.get<long long>();
                if (messageId > 0 && messageId <= std::numeric_limits<int>::max()) {
                    messageIds.push_back(static_cast<int>(messageId));
                }
            }
            std::sort(messageIds.begin(), messageIds.end());
            messageIds.erase(std::unique(messageIds.begin(), messageIds.end()), messageIds.end());
            if (messageIds.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Không có ID tin nhắn hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            int markedCount = 0;
            bool ok = false;
            if (isGroup) {
                ok = db.markGroupMessagesRead(conversationId, userId, messageIds, markedCount);
            } else {
                if (!db.canAccessDirectConversation(userId, conversationId)) {
                    res.status = 403;
                    res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền truy cập cuộc trò chuyện này!"}}).dump(), "application/json; charset=utf-8");
                    return;
                }
                ok = db.markDirectMessagesRead(userId, conversationId, messageIds, markedCount);
            }
            if (!ok) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không phải thành viên của cuộc trò chuyện này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            res.set_content(json({{"status", "ok"}, {"marked_count", markedCount}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Lấy thông tin chi tiết người xem / người nhận của tin nhắn (Popup Hình 2)
    R.Get("/api/messages/receipts", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string userId = req.get_param_value("user_id");
            std::string msgIdStr = req.get_param_value("message_id");
            std::string isGroupStr = req.get_param_value("is_group");
            bool isGroup = (isGroupStr == "1" || isGroupStr == "true");
            int messageId = sec::clampInt(msgIdStr, 0, 0, 2147483647);

            if (userId.empty() || messageId <= 0) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu tham số hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (!db.canUserViewMessageReceipts(userId, messageId, isGroup)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Bạn không có quyền xem thông tin tin nhắn này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            json receipts = db.getMessageReceiptDetails(messageId, isGroup);
            json messageDetails = json::object();
            if (isGroup) {
                GroupMessage gm;
                if (db.getGroupMessageById(messageId, gm)) {
                    messageDetails = gm.toJson();
                }
            } else {
                Message dm;
                if (db.getDirectMessageById(messageId, dm)) {
                    messageDetails = dm.toJson();
                }
            }

            res.set_content(json({
                {"status", "ok"},
                {"message_id", messageId},
                {"is_group", isGroup},
                {"message", messageDetails},
                {"receipts", receipts}
            }).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/messages/recall", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string senderId = body.value("sender_id", "");
            if (db.recallMessage(msgId, senderId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã thu hồi tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không thể thu hồi tin nhắn này!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });



    R.Post("/api/messages/delete", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string userId = body.value("user_id", "");
            if (db.deleteMessageForMe(msgId, userId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500; res.set_content(json({{"status", "error"}, {"message", "Không thể xóa tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/messages/react", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string userId = body.value("user_id", "");
            std::string reaction = body.value("reaction", "");
            if (db.reactMessage(msgId, userId, reaction)) {
                res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi thả cảm xúc!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/messages/read-status", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        if (myId.empty() || partnerId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu params!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        bool isRead = db.getLastMessageReadStatus(myId, partnerId);
        res.set_content(json({{"status", "ok"}, {"is_read", isRead}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/friends/nickname", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string friendId = body.value("friend_id", "");
            std::string nickname = body.value("nickname", "");
            if (userId.empty() || friendId.empty()) { res.status = 400; res.set_content("{}", "application/json"); return; }
            db.setNickname(userId, friendId, nickname);
            res.set_content(json({{"status", "ok"}, {"message", "Đã đặt biệt danh!"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });



    R.Get("/api/friends/nickname", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string friendId = req.get_param_value("friend_id");
        std::string nick = db.getNickname(userId, friendId);
        res.set_content(json({{"status", "ok"}, {"nickname", nick}}).dump(), "application/json; charset=utf-8");
    });

    R.Delete("/api/messages/clear", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string u1 = body.value("my_id", "");
            std::string u2 = body.value("partner_id", "");
            if (db.clearConversation(u1, u2)) {
                db.setConversationForcedUnread(u1, u2, false, false);
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa toàn bộ đoạn tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi xóa tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/messages/share-post", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string senderId = body.value("sender_id", "");
            std::string targetType = body.value("target_type", "USER"); // "USER" hoặc "GROUP"
            std::string targetId = body.value("target_id", "");
            std::string postId = body.value("post_id", "");
            std::string comment = body.value("comment", "");

            if (senderId.empty() || targetId.empty() || postId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Dữ liệu chia sẻ không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            std::string shareText = comment.empty() ? "Đã chia sẻ một bài viết" : comment;

            if (targetType == "GROUP") {
                GroupMessage gMsg;
                if (db.sendGroupMessage(targetId, senderId, shareText, postId, gMsg)) {
                    db.setConversationArchived(senderId, targetId, true, false);
                    db.setConversationForcedUnread(senderId, targetId, true, false);
                    res.set_content(json({{"status", "ok"}, {"message", "Đã chia sẻ bài viết vào nhóm!"}}).dump(), "application/json; charset=utf-8");
                } else {
                    res.status = 500;
                    res.set_content(json({{"status", "error"}, {"message", "Không thể chia sẻ bài viết vào nhóm!"}}).dump(), "application/json; charset=utf-8");
                }
            } else {
                Message msg;
                if (db.sendMessage(senderId, targetId, shareText, postId, msg)) {
                    db.setConversationArchived(senderId, targetId, false, false);
                    db.setConversationForcedUnread(senderId, targetId, false, false);
                    res.set_content(json({{"status", "ok"}, {"message", "Đã chia sẻ bài viết vào hộp thoại trò chuyện!"}}).dump(), "application/json; charset=utf-8");
                } else {
                    res.status = 500;
                    res.set_content(json({{"status", "error"}, {"message", "Không thể chia sẻ bài viết!"}}).dump(), "application/json; charset=utf-8");
                }
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 6. TẠO NHÓM VÀ QUẢN LÝ NHÓM & NHẮN TIN NHÓM
    // -------------------------------------------------------------
    R.Post("/api/groups/update-avatar", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string userId = body.value("user_id", "");
            std::string avatar = body.value("avatar", "");
            if (groupId.empty() || avatar.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu dữ liệu!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            {
                // BẢO MẬT: bản cũ không kiểm tra quyền nào -> bất kỳ ai cũng đổi được ảnh của mọi nhóm
                const std::string actor = sessionUid(db, req);
                if (actor.empty() || db.getGroupRole(groupId, actor).empty()) {
                    res.status = 403;
                    res.set_content(json({{"status", "error"}, {"message", "Bạn không phải thành viên nhóm này!"}}).dump(), "application/json; charset=utf-8");
                    return;
                }
            }
            if (db.updateGroupAvatar(groupId, avatar)) {
                res.set_content(json({{"status", "ok"}, {"message", "Cập nhật avatar nhóm thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật avatar!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/groups/update-name", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = getJsonStringSafe(body, "group_id");
            std::string newName = getJsonStringSafe(body, "name");
            std::string userId = getJsonStringSafe(body, "user_id");

            if (groupId.empty() || newName.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Tên nhóm không được để trống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            {
                // BẢO MẬT: bản cũ chỉ kiểm tra khi client tự gửi user_id -> bỏ trường này là đổi tên được nhóm bất kỳ
                const std::string actor = sessionUid(db, req);
                std::string role = actor.empty() ? "" : db.getGroupRole(groupId, actor);
                if (role.empty()) {
                    res.status = 403;
                    res.set_content(json({{"status", "error"}, {"message", "Bạn không phải thành viên nhóm này!"}}).dump(), "application/json; charset=utf-8");
                    return;
                }
            }

            if (db.updateGroupName(groupId, newName)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã đổi tên nhóm thành công!"}, {"name", newName}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật tên nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/groups/create", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string description = body.value("description", "");
            std::string createdBy = body.value("created_by", "");
            std::vector<std::string> memberIds;
            if (body.contains("members") && body["members"].is_array()) {
                for (const auto& m : body["members"]) memberIds.push_back(m.get<std::string>());
            }

            if (name.empty() || createdBy.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Tên nhóm không được để trống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            Group group;
            if (db.createGroup(name, description, createdBy, memberIds, group)) {
                res.set_content(json({{"status", "ok"}, {"message", "Tạo nhóm thành công!"}, {"group", group.toJson()}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi tạo nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/groups/my-groups", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        auto list = db.getUserGroups(userId);
        json arr = json::array();
        for (const auto& g : list) {
            json group = g.toJson();
            group["is_archived"] = db.isConversationArchived(userId, g.id, true);
            const bool forced = db.isConversationForcedUnread(userId, g.id, true);
            const int unreadCnt = db.getGroupUnreadCount(g.id, userId);
            group["unread_count"] = unreadCnt;
            group["is_unread"] = forced || unreadCnt > 0;
            arr.push_back(std::move(group));
        }
        res.set_content(json({{"status", "ok"}, {"groups", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Get("/api/groups/messages", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string groupId = req.get_param_value("group_id");
        std::string viewerId = req.get_param_value("viewer_id");
        int limit = paramInt(req, "limit", 30, 1, 100);
        if (groupId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu group_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        if (!db.isGroupMember(groupId, viewerId)) {
            res.status = 403;
            res.set_content(json({{"status", "error"}, {"message", "Bạn không phải thành viên của nhóm này!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        auto msgs = db.getGroupMessages(groupId, limit, viewerId);
        json arr = json::array();
        for (const auto& m : msgs) arr.push_back(m.toJson());
        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/groups/messages/send", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string senderId = body.value("sender_id", "");
            std::string content = body.value("content", "");
            std::string sharedPostId = body.value("shared_post_id", "");
            std::string mediaType = body.value("media_type", "NONE");
            std::string mediaUrl = body.value("media_url", "");
            std::string replyToText = body.value("reply_to_text", "");
            int replyToId = body.value("reply_to_id", 0);

            if (groupId.empty() || senderId.empty() || (content.empty() && sharedPostId.empty() && mediaUrl.empty())) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Nội dung tin nhắn không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            GroupMessage outMsg;
            if (db.sendGroupMessage(groupId, senderId, content, sharedPostId, outMsg, mediaType, mediaUrl, replyToText, replyToId)) {
                db.setConversationArchived(senderId, groupId, true, false);
                db.setConversationForcedUnread(senderId, groupId, true, false);
                User sender;
                db.getUserById(senderId, sender);
                outMsg.sender_name = sender.fullname;
                outMsg.sender_avatar = sender.avatar;
                db.updateHeartbeat(senderId);

                res.set_content(json({{"status", "ok"}, {"message", outMsg.toJson()}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể gửi tin nhắn vào nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Group management endpoints
    R.Get("/api/groups/members", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string groupId = req.get_param_value("group_id");
        if (groupId.empty()) { res.status = 400; res.set_content("{}", "application/json"); return; }
        json members = db.getGroupMembers(groupId);
        res.set_content(json({{"status", "ok"}, {"members", members}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/groups/members/add", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string requesterId = body.value("requester_id", "");
            std::vector<std::string> userIds = body.value("user_ids", std::vector<std::string>{});
            if (userIds.empty()) {
                const std::string userId = body.value("user_id", "");
                if (!userId.empty()) userIds.push_back(userId);
            }
            if (groupId.empty() || requesterId.empty() || userIds.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Vui lòng chọn ít nhất một thành viên!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            std::vector<std::string> addedNames;
            if (!db.addGroupMembers(groupId, userIds, requesterId, addedNames)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Không thể thêm thành viên. Hãy kiểm tra thành viên đã chọn!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            GroupMessage systemMessage;
            const std::string inviteContent = "__SYS_GROUP_INVITE__:" + json(addedNames).dump();
            if (!db.sendGroupMessage(groupId, requesterId, inviteContent, "", systemMessage, "SYSTEM")) {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Đã thêm thành viên nhưng không thể gửi thông báo vào nhóm!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            res.set_content(json({{"status", "ok"}, {"message", "Đã mời thành viên vào nhóm!"}, {"added_names", addedNames}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });



    R.Post("/api/groups/members/remove", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string userId = body.value("user_id", body.value("member_id", ""));
            std::string requesterId = body.value("requester_id", body.value("operator_id", ""));
            if (db.removeGroupMember(groupId, userId, requesterId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa thành viên!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa thành viên!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });



    R.Post("/api/groups/leave", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string userId = body.value("user_id", "");
            if (db.leaveGroup(groupId, userId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã rời khỏi nhóm!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể rời nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });



    R.Post("/api/groups/messages/recall", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string senderId = body.value("sender_id", "");
            if (db.recallGroupMessage(msgId, senderId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã thu hồi tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không thể thu hồi tin nhắn này!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });



    R.Post("/api/groups/messages/delete", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string userId = body.value("user_id", "");
            if (db.deleteGroupMessageForMe(msgId, userId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/groups/messages/clear-for-me", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            const auto body = json::parse(req.body);
            const std::string groupId = body.value("group_id", "");
            const std::string userId = body.value("user_id", "");
            if (groupId.empty() || userId.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu thông tin nhóm hoặc người dùng!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (!db.clearGroupConversationForUser(groupId, userId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa tin nhắn nhóm cho tài khoản này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            db.setConversationForcedUnread(userId, groupId, true, false);
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });


    R.Get("/api/groups/friends-to-add", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("user_id");
        std::string groupId = req.get_param_value("group_id");
        json friends = db.getFriendsNotInGroup(myId, groupId);
        res.set_content(json({{"status", "ok"}, {"friends", friends}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 7. TÌM KIẾM THỜI GIAN THỰC & TRANG TÌM KIẾM CÓ BỘ LỌC (Hình 3 & 4)
    // -------------------------------------------------------------
    R.Get("/api/search", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string keyword = req.has_param("q") ? req.get_param_value("q") : (req.has_param("keyword") ? req.get_param_value("keyword") : "");
        std::string filter = req.has_param("filter") ? req.get_param_value("filter") : "ALL";
        std::string currentUserId = req.get_param_value("user_id");

        if (keyword.empty()) {
            res.set_content(json({{"status", "ok"}, {"users", json::array()}, {"groups", json::array()}, {"posts", json::array()}}).dump(), "application/json; charset=utf-8");
            return;
        }

        json results = db.searchEverything(keyword, filter, currentUserId);
        if (results.contains("posts") && results["posts"].is_array()) {
            for (auto& searchResult : results["posts"]) {
                Post post;
                const std::string postId = searchResult.value("id", "");
                if (!postId.empty() && db.getPostById(postId, currentUserId, post)) {
                    searchResult = post.toJson();
                }
            }
        }
        results["status"] = "ok";
        res.set_content(results.dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // LỊCH SỬ TÌM KIẾM
    // -------------------------------------------------------------
    R.Get("/api/search/history", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        auto history = db.getSearchHistory(userId);
        json arr = json::array();
        for (const auto& item : history) {
            arr.push_back(item.toJson());
        }
        res.set_content(json({{"status", "ok"}, {"history", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/search/history", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string query = body.value("query", "");
            if (userId.empty() || query.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu thông tin user_id hoặc query"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            bool ok = db.addSearchHistory(userId, query);
            res.set_content(json({{"status", ok ? "ok" : "error"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    auto deleteSearchHistoryHandler = [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string idStr = req.get_param_value("id");
        bool clearAll = (req.get_param_value("all") == "true") || idStr.empty();

        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        bool ok = false;
        if (clearAll) {
            ok = db.clearSearchHistory(userId);
        } else {
            int id = 0;
            try { id = std::stoi(idStr); } catch(...) {}
            ok = db.deleteSearchHistoryItem(userId, id);
        }
        res.set_content(json({{"status", ok ? "ok" : "error"}}).dump(), "application/json; charset=utf-8");
    };
    R.Delete("/api/search/history", deleteSearchHistoryHandler);
    R.Post("/api/search/history/delete", deleteSearchHistoryHandler);

    // -------------------------------------------------------------
    // 8. THÔNG BÁO (HÌNH CÁI CHUÔNG - Hình 4)
    // -------------------------------------------------------------
    R.Get("/api/notifications", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        int offset = 0;
        if (req.has_param("offset")) {
            try { offset = std::max(0, std::stoi(req.get_param_value("offset"))); } catch (...) { offset = 0; }
        }
        constexpr int pageSize = 15;
        auto list = db.getUserNotifications(userId, pageSize, offset);
        json arr = json::array();
        for (const auto& n : list) {
            arr.push_back(n.toJson());
        }
        res.set_content(json({
            {"status", "ok"},
            {"unread_count", db.getUnreadNotificationCount(userId)},
            {"notifications", arr},
            {"offset", offset},
            {"has_more", arr.size() == pageSize}
        }).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/notifications/read", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            if (body.contains("notification_id")) {
                int notifId = body.value("notification_id", 0);
                if (notifId > 0 && !userId.empty()) {
                    db.markSingleNotificationRead(notifId, userId);
                }
            } else if (!userId.empty()) {
                db.markNotificationsRead(userId);
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/notifications/delete", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            int notifId = body.value("notification_id", 0);
            if (userId.empty() || notifId <= 0) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu thông tin thông báo!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (!db.deleteNotification(notifId, userId)) {
                res.status = 404;
                res.set_content(json({{"status", "error"}, {"message", "Không tìm thấy thông báo!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/groups/messages/react", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string userId = body.value("user_id", "");
            std::string reaction = body.value("reaction", "");
            if (db.reactGroupMessage(msgId, userId, reaction)) {
                res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi thả cảm xúc tin nhắn nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 9. BÁO CÁO VI PHẠM TỚI ADMIN
    // -------------------------------------------------------------
    R.Post("/api/reports/create", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string reporterId = getJsonStringSafe(body, "reporter_id");
            std::string reportedUserId = getJsonStringSafe(body, "reported_user_id");
            std::string reportedPostId = getJsonStringSafe(body, "reported_post_id");
            std::string reportedCommentId = getJsonStringSafe(body, "reported_comment_id");
            std::string reason = getJsonStringSafe(body, "reason");

            if (reporterId.empty() || reason.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Vui lòng nhập lý do báo cáo!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (db.createReport(reporterId, reportedUserId, reportedPostId, reportedCommentId, reason)) {
                res.set_content(json({{"status", "ok"}, {"message", "Báo cáo của bạn đã được gửi tới Quản trị viên xử lý!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi gửi báo cáo!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 8. TÍNH NĂNG QUẢN TRỊ VIÊN (ADMIN DASHBOARD)
    // -------------------------------------------------------------
    R.Get("/api/admin/stats", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string adminId = req.get_param_value("admin_id");
        if (!requireAdmin(db, req, adminId, res)) return;
        json stats = db.getAdminStats();
        res.set_content(json({{"status", "ok"}, {"stats", stats}}).dump(), "application/json; charset=utf-8");
    });

    R.Get("/api/admin/users", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string adminId = req.get_param_value("admin_id");
        if (!requireAdmin(db, req, adminId, res)) return;
        auto list = db.getAllUsers();
        json arr = json::array();
        for (const auto& u : list) arr.push_back(u.toJson(true));
        res.set_content(json({{"status", "ok"}, {"users", arr}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/admin/toggle-lock", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string adminId = body.value("admin_id", "");
            std::string userId = body.value("user_id", "");
            if (!requireAdmin(db, req, adminId, res)) return;
            if (db.isUserAdmin(userId)) {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không thể khóa tài khoản quản trị viên!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            bool nowLocked = false;
            if (db.toggleUserLock(userId, nowLocked)) {
                db.logAdminAction(adminId, nowLocked ? "LOCK_USER" : "UNLOCK_USER", "user", userId, "");
                std::string msg = nowLocked ? "Đã khóa tài khoản thành công!" : "Đã mở khóa tài khoản thành công!";
                res.set_content(json({{"status", "ok"}, {"is_locked", nowLocked}, {"message", msg}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật trạng thái tài khoản!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/admin/reports", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string adminId = req.get_param_value("admin_id");
        if (!requireAdmin(db, req, adminId, res)) return;
        // Phân trang: mặc định 15 báo cáo/lần, cuộn xuống cuối danh sách thì tải tiếp (offset)
        int limit = 15, offset = 0;
        try { if (req.has_param("limit")) limit = std::max(1, std::min(200, std::stoi(req.get_param_value("limit")))); } catch (...) {}
        try { if (req.has_param("offset")) offset = std::max(0, std::stoi(req.get_param_value("offset"))); } catch (...) {}
        std::string keyword = req.has_param("q") ? req.get_param_value("q") : "";
        // Lấy dư 1 bản ghi để biết còn báo cáo phía sau hay không
        auto list = db.getReportsPaged(limit + 1, offset, keyword);
        bool hasMore = (int)list.size() > limit;
        if (hasMore) list.pop_back();
        json arr = json::array();
        for (const auto& r : list) arr.push_back(r.toJson());
        res.set_content(json({{"status", "ok"}, {"reports", arr}, {"offset", offset}, {"limit", limit}, {"has_more", hasMore}})
                            .dump(-1, ' ', false, json::error_handler_t::replace), "application/json; charset=utf-8");
    });

    R.Post("/api/admin/resolve-report", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string adminId = body.value("admin_id", "");
            int reportId = body.value("report_id", 0);
            std::string action = body.value("action", "RESOLVED"); // RESOLVED / DISMISSED / IN_PROGRESS
            std::string note = body.value("note", "");
            if (!requireAdmin(db, req, adminId, res)) return;
            if (action != "IN_PROGRESS" && action != "RESOLVED" && action != "DISMISSED") {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Trạng thái xử lý không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            std::string reporterId;
            if (db.resolveReportEx(reportId, action, adminId, note, reporterId)) {
                db.logAdminAction(adminId, "RESOLVE_REPORT", "report", std::to_string(reportId), action + " | " + note);
                // Thông báo kết quả cho người gửi khiếu nại
                if (!reporterId.empty() && (action == "RESOLVED" || action == "DISMISSED")) {
                    std::string content = action == "RESOLVED"
                        ? ("Khiếu nại #" + std::to_string(reportId) + " đã được chấp nhận xử lý." + (note.empty() ? "" : (" Ghi chú: " + note)))
                        : ("Khiếu nại #" + std::to_string(reportId) + " đã bị từ chối." + (note.empty() ? "" : (" Lý do: " + note)));
                    db.addNotificationInternal(reporterId, adminId, "REPORT_RESULT", content, "/");
                }
                res.set_content(json({{"status", "ok"}, {"message", "Đã xử lý báo cáo!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi xử lý báo cáo!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/admin/delete-post", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string adminId = body.value("admin_id", "");
            std::string postId = body.value("post_id", "");
            if (!requireAdmin(db, req, adminId, res)) return;
            if (db.deletePost(postId)) {
                db.logAdminAction(adminId, "DELETE_POST", "post", postId, "");
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa bài viết vi phạm!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa bài viết!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Xóa (ẩn danh) tài khoản vi phạm
    R.Post("/api/admin/delete-user", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string adminId = body.value("admin_id", "");
            std::string userId = body.value("user_id", "");
            if (!requireAdmin(db, req, adminId, res)) return;
            if (userId.empty() || db.isUserAdmin(userId)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa tài khoản này!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.deleteUserAccount(userId)) {
                db.logAdminAction(adminId, "DELETE_USER", "user", userId, "soft-delete");
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa tài khoản vi phạm!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa tài khoản!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Danh sách bài viết cho trang quản trị (chỉ Admin)
    R.Get("/api/admin/posts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string adminId = req.get_param_value("admin_id");
        if (!requireAdmin(db, req, adminId, res)) return;
        int limit = 100;
        try { if (req.has_param("limit")) limit = std::max(1, std::min(500, std::stoi(req.get_param_value("limit")))); } catch (...) {}
        auto posts = db.getFeedPosts("", limit, 0);
        json arr = json::array();
        for (const auto& p : posts) arr.push_back(p.toJson());
        res.set_content(json({{"status", "ok"}, {"posts", arr}}).dump(-1, ' ', false, json::error_handler_t::replace), "application/json; charset=utf-8");
    });

    // Phân loại khiếu nại
    R.Post("/api/admin/classify-report", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string adminId = body.value("admin_id", "");
            int reportId = body.value("report_id", 0);
            std::string category = body.value("category", "");
            if (!requireAdmin(db, req, adminId, res)) return;
            static const std::set<std::string> allowed = {"SPAM", "HARASSMENT", "INAPPROPRIATE", "FAKE_ACCOUNT", "TECHNICAL", "OTHER"};
            if (!allowed.count(category)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Loại khiếu nại không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.classifyReport(reportId, category)) {
                db.logAdminAction(adminId, "CLASSIFY_REPORT", "report", std::to_string(reportId), category);
                res.set_content(json({{"status", "ok"}, {"message", "Đã phân loại khiếu nại!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 404;
                res.set_content(json({{"status", "error"}, {"message", "Không tìm thấy khiếu nại!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/admin/audit-logs", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string adminId = req.get_param_value("admin_id");
        if (!requireAdmin(db, req, adminId, res)) return;
        res.set_content(json({{"status", "ok"}, {"logs", db.getAdminAuditLogs(80)}}).dump(-1, ' ', false, json::error_handler_t::replace), "application/json; charset=utf-8");
    });

    // ===================== COMMUNITY GROUPS APIs =====================
    R.Post("/api/community-groups/create", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("name", "");
            std::string description = body.value("description", "");
            std::string avatar = body.value("avatar", "");
            std::string privacy = body.value("privacy", "PUBLIC");
            std::string createdBy = body.value("created_by", "");
            if (name.empty() || createdBy.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thiếu tên nhóm hoặc người tạo!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            json group;
            if (db.createCommunityGroup(name, description, avatar, privacy, createdBy, group)) {
                res.set_content(json({{"status", "ok"}, {"group", group}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể tạo nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/community-groups/my", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        res.set_content(json({{"status", "ok"}, {"groups", db.getMyCommunityGroups(userId)}}).dump(), "application/json; charset=utf-8");
    });

    R.Get("/api/community-groups/discover", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string q = req.get_param_value("q");
        res.set_content(json({{"status", "ok"}, {"groups", db.discoverCommunityGroups(userId, q)}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/community-groups/join", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string userId = body.value("user_id", "");
            if (db.joinCommunityGroup(groupId, userId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã tham gia nhóm!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể tham gia nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/community-groups/leave", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string userId = body.value("user_id", "");
            if (db.leaveCommunityGroup(groupId, userId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã rời nhóm!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể rời nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/community-groups/feed", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string groupId = req.get_param_value("group_id");
        int limit = paramInt(req, "limit", 20, 1, 100);
        int offset = paramInt(req, "offset", 0, 0, 1000000);
        res.set_content(json({{"status", "ok"}, {"posts", db.getCommunityGroupFeed(userId, groupId, limit, offset)}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/community-groups/posts", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string authorId = body.value("author_id", "");
            std::string title = trimStr(body.value("title", ""));
            std::string content = trimStr(body.value("content", ""));
            std::string mediaType = body.value("media_type", "NONE");
            std::string mediaUrl = body.value("media_url", "");
            if (title.empty() && content.empty() && mediaType == "NONE") {
                sendJsonError(res, 400, "Nhập tiêu đề, nội dung hoặc thêm ảnh/video.");
                return;
            }
            std::string postId, status;
            if (db.createCommunityGroupPost(groupId, authorId, title, content, mediaType, mediaUrl, postId, status)) {
                res.set_content(json({{"status", "ok"}, {"post_id", postId}, {"approval_status", status},
                    {"message", status == "APPROVED" ? "Đã đăng bài!" : "Bài viết đang chờ admin phê duyệt!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể đăng bài!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/community-groups/posts/approve", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string adminId = body.value("admin_id", "");
            bool approve = body.value("approve", true);
            if (db.approveCommunityGroupPost(postId, adminId, approve)) {
                res.set_content(json({{"status", "ok"}, {"message", approve ? "Đã duyệt bài!" : "Đã từ chối bài!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Không có quyền duyệt!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/community-groups/posts/delete", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            const std::string postId = body.value("post_id", "");
            const std::string userId = body.value("user_id", "");
            if (postId.empty() || userId.empty()) {
                sendJsonError(res, 400, "Thiếu post_id hoặc user_id.");
                return;
            }
            if (!db.deleteCommunityGroupPost(postId, userId)) {
                sendJsonError(res, 403, "Bạn không có quyền xóa bài viết này hoặc bài viết không còn tồn tại.");
                return;
            }
            res.set_content(json({{"status", "ok"}, {"message", "Đã xóa bài viết."}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Dữ liệu yêu cầu không hợp lệ.");
        }
    });

    R.Post("/api/community-groups/posts/edit", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            const std::string postId = body.value("post_id", "");
            const std::string userId = body.value("user_id", "");
            const std::string title = trimStr(body.value("title", ""));
            const std::string content = trimStr(body.value("content", ""));
            if (postId.empty() || userId.empty() || (title.empty() && content.empty())) {
                sendJsonError(res, 400, "Nhập tiêu đề hoặc nội dung bài viết.");
                return;
            }
            if (!db.editCommunityGroupPost(postId, userId, title, content)) {
                sendJsonError(res, 403, "Bạn không có quyền sửa bài viết này hoặc bài viết không còn tồn tại.");
                return;
            }
            res.set_content(json({{"status", "ok"}, {"message", "Đã cập nhật bài viết."}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Dữ liệu yêu cầu không hợp lệ.");
        }
    });

    R.Get("/api/community-groups/posts/pending", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string groupId = req.get_param_value("group_id");
        std::string adminId = req.get_param_value("admin_id");
        res.set_content(json({{"status", "ok"}, {"posts", db.getCommunityGroupPendingPosts(groupId, adminId)}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/community-groups/posts/reaction", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string userId = body.value("user_id", "");
            std::string type = body.value("type", "LIKE");
            if (db.reactCommunityGroupPost(postId, userId, type)) {
                res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Không thể thả cảm xúc (bài chưa duyệt hoặc lỗi)."}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Get("/api/community-groups/posts/comments", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string postId = req.get_param_value("post_id");
        std::string userId = req.get_param_value("user_id");
        res.set_content(json({{"status", "ok"}, {"comments", db.getCommunityGroupPostComments(postId, userId)}}).dump(), "application/json; charset=utf-8");
    });

    R.Post("/api/community-groups/posts/comments/reaction", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            const std::string commentId = body.value("comment_id", "");
            const std::string userId = body.value("user_id", "");
            const std::string type = body.value("type", "LIKE");
            if (!db.reactCommunityGroupComment(commentId, userId, type)) {
                sendJsonError(res, 400, "Không thể cập nhật cảm xúc bình luận.");
                return;
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception&) {
            sendJsonError(res, 400, "Dữ liệu yêu cầu không hợp lệ.");
        }
    });

    R.Post("/api/community-groups/posts/comment", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string authorId = body.value("author_id", "");
            std::string content = body.value("content", "");
            std::string mediaUrl = body.value("media_url", "");
            if (content.empty() && !mediaUrl.empty()) content = std::string("__GIF__:") + mediaUrl;
            json out;
            if (db.commentCommunityGroupPost(postId, authorId, content, out)) {
                res.set_content(json({{"status", "ok"}, {"comment", out}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Không thể bình luận (bài chưa duyệt?)."}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    R.Post("/api/community-groups/posts/share-to-feed", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string userId = body.value("user_id", "");
            std::string thoughts = body.value("thoughts", "");
            std::string outId;
            if (db.shareCommunityGroupPostToFeed(postId, userId, thoughts, outId)) {
                res.set_content(json({{"status", "ok"}, {"post_id", outId}, {"message", "Đã chia sẻ ra Bảng tin!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Không thể chia sẻ bài này."}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu không hợp lệ hoặc đã xảy ra lỗi."}}).dump(), "application/json; charset=utf-8");
        }
    });

    const int port = static_cast<int>(sec::envInt("PORT", 8080));
    const std::string bindHost = sec::envOr("BIND_HOST", "0.0.0.0");
    std::cout << "[Server] C++ Web Server đang khởi động tại: http://" << bindHost << ":" << port
              << (gTrustProxy ? "  (TRUST_PROXY=1)" : "") << (sec::envFlag("PRODUCTION") ? "  [PRODUCTION]" : "  [dev]") << std::endl;
    while (!svr.listen(bindHost, port)) {
        std::cout << "[Server] Cổng " << port << " đang bận hoặc đang giải phóng (TimeWait). Đang thử lại sau 2 giây..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return 0;
}
