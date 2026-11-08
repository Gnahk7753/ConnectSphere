#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <algorithm>
#include <cmath>
#include <ctime>
#include "httplib.h"
#include "json.hpp"
#include "Models.h"
#include "Database.h"
#include "SuggestService.h"
#include "SmtpService.h"
#include "GeminiService.h"

using json = nlohmann::json;

static std::string getJsonStringSafe(const json& j, const std::string& key, const std::string& defVal = "") {
    if (!j.contains(key) || j[key].is_null()) return defVal;
    if (j[key].is_string()) return j[key].get<std::string>();
    if (j[key].is_number_integer()) return std::to_string(j[key].get<long long>());
    if (j[key].is_number_unsigned()) return std::to_string(j[key].get<unsigned long long>());
    if (j[key].is_number()) return std::to_string(j[key].get<double>());
    if (j[key].is_boolean()) return j[key].get<bool>() ? "true" : "false";
    return defVal;
}

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

    // Nạp cấu hình Gemini từ .env (không hardcode API key)
    GeminiService::instance().loadEnv(".env");

    Database db;
    std::string dbPath = "data/social_network.db";

    if (const char* path = std::getenv("DATABASE_PATH")) {
        dbPath = path;
    }
    
    std::filesystem::path parent =
        std::filesystem::path(dbPath).parent_path();
    
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    
    if (!db.init(dbPath)) {
        std::cerr << "Loi khoi tao Database!" << std::endl;
        return 1;
    }
    std::cout << "[Database] SQLite Database đã sẵn sàng tại: "
          << dbPath << std::endl;

    httplib::Server svr;
    svr.set_mount_point("/", "./static");

    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
    
        const char* allowedOrigin = std::getenv("ALLOWED_ORIGIN");
    
        if (allowedOrigin != nullptr && std::string(allowedOrigin).length() > 0) {
            res.set_header("Access-Control-Allow-Origin", allowedOrigin);
            res.set_header("Vary", "Origin");
        } else {
            // Chỉ dùng khi phát triển trên máy cá nhân
            res.set_header("Access-Control-Allow-Origin", "*");
        }
    
        res.set_header(
            "Access-Control-Allow-Methods",
            "GET, POST, PUT, DELETE, OPTIONS"
        );
    
        res.set_header(
            "Access-Control-Allow-Headers",
            "Content-Type, Authorization"
        );
    
        if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
    
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // -------------------------------------------------------------
    // 1. AUTHENTICATION & GMAIL OTP 6-DIGIT VERIFICATION
    // -------------------------------------------------------------
    svr.Post("/api/auth/send-otp", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
    
            std::string email = body.value("email", "");
            std::string purpose = body.value("purpose", "REGISTER");
    
            // Kiểm tra email cơ bản
            if (email.empty() ||
                email.find('@') == std::string::npos ||
                email.find('.') == std::string::npos) {
    
                res.status = 400;
                res.set_content(
                    json({
                        {"status", "error"},
                        {"message", "Địa chỉ email không hợp lệ!"}
                    }).dump(),
                    "application/json; charset=utf-8"
                );
                return;
            }
    
            // Chỉ chấp nhận các mục đích OTP hợp lệ
            if (purpose != "REGISTER" && purpose != "LOGIN") {
                res.status = 400;
                res.set_content(
                    json({
                        {"status", "error"},
                        {"message", "Mục đích xác thực không hợp lệ!"}
                    }).dump(),
                    "application/json; charset=utf-8"
                );
                return;
            }
    
            // Tạo OTP 6 chữ số
            std::string otpCode = SmtpService::generate6DigitOtp();
    
            // Lưu OTP với thời hạn 300 giây
            if (!db.saveOtp(email, otpCode, purpose, 300)) {
                res.status = 500;
                res.set_content(
                    json({
                        {"status", "error"},
                        {"message", "Không thể lưu mã xác thực!"}
                    }).dump(),
                    "application/json; charset=utf-8"
                );
                return;
            }
    
            // Gửi email OTP
            SmtpService::sendOtpEmail(
                email,
                otpCode,
                purpose == "REGISTER"
                    ? "Xác thực đăng ký tài khoản"
                    : "Xác thực đăng nhập"
            );
    
            // Phản hồi cho frontend, tuyệt đối không trả OTP
            json resp = {
                {"status", "ok"},
                {"message", "Mã xác thực đã được gửi đến email của bạn."}
            };
    
            res.set_content(
                resp.dump(),
                "application/json; charset=utf-8"
            );
    
        } catch (const nlohmann::json::exception&) {
            res.status = 400;
            res.set_content(
                json({
                    {"status", "error"},
                    {"message", "Dữ liệu JSON không hợp lệ!"}
                }).dump(),
                "application/json; charset=utf-8"
            );
    
        } catch (const std::exception&) {
            res.status = 500;
            res.set_content(
                json({
                    {"status", "error"},
                    {"message", "Không thể xử lý yêu cầu gửi OTP!"}
                }).dump(),
                "application/json; charset=utf-8"
            );
        }
    });


    svr.Post("/api/auth/register-with-otp", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string email = body.value("email", "");
            std::string code = body.value("code", "");
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            std::string fullname = body.value("fullname", "");

            if (email.empty() || code.empty() || username.empty() || password.empty() || fullname.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Vui lòng nhập đầy đủ các trường thông tin và mã 6 số!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (!db.verifyOtp(email, code, "REGISTER")) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Mã xác thực 6 chữ số không đúng hoặc đã hết hạn!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            User existing;
            if (db.getUserByUsernameOrEmail(username, existing)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Tên đăng nhập đã tồn tại trong hệ thống!"}}).dump(), "application/json; charset=utf-8");
                return;
            }
            if (db.getUserByUsernameOrEmail(email, existing)) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Email đã được sử dụng bởi tài khoản khác!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            User newUser;
            newUser.id = db.generateNewUserId();
            newUser.username = username;
            newUser.email = email;
            newUser.password_hash = password;
            newUser.fullname = fullname;
            newUser.avatar = "/img/default-avatar.svg";
            newUser.bio = "Chào mừng tôi đến với Mạng Xã Hội Mini!";
            newUser.role = "USER";
            newUser.status = "ACTIVE";

            if (db.createUser(newUser)) {
                res.set_content(json({
                    {"status", "ok"},
                    {"message", "Xác thực Gmail và đăng ký tài khoản thành công!"},
                    {"user", newUser.toJson()}
                }).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi lưu tài khoản vào cơ sở dữ liệu!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/auth/login", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string identifier = body.value("identifier", "");
            std::string password = body.value("password", "");

            User user;
            if (!db.getUserByUsernameOrEmail(identifier, user)) {
                res.status = 401;
                res.set_content(json({{"status", "error"}, {"message", "Tài khoản hoặc email không tồn tại!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (user.password_hash != password) {
                res.status = 401;
                res.set_content(json({{"status", "error"}, {"message", "Mật khẩu không chính xác!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (user.status == "LOCKED") {
                res.status = 403;
                res.set_content(json({{"status", "error"}, {"message", "Tài khoản của bạn đã bị khóa bởi Quản trị viên!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            db.updateHeartbeat(user.id);

            res.set_content(json({
                {"status", "ok"},
                {"message", "Đăng nhập thành công!"},
                {"user", user.toJson()}
            }).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/auth/google-login", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string email = body.value("email", "");
            std::string fullname = body.value("fullname", "");
            std::string avatar = body.value("avatar", "");

            if (email.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Thông tin tài khoản Google không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            User user;
            if (db.getUserByUsernameOrEmail(email, user)) {
                if (user.status == "LOCKED") {
                    res.status = 403;
                    res.set_content(json({{"status", "error"}, {"message", "Tài khoản Google này đã bị khóa!"}}).dump(), "application/json; charset=utf-8");
                    return;
                }
                db.updateHeartbeat(user.id);
                res.set_content(json({
                    {"status", "ok"},
                    {"message", "Đăng nhập Google thành công!"},
                    {"user", user.toJson()}
                }).dump(), "application/json; charset=utf-8");
            } else {
                User newUser;
                newUser.id = db.generateNewUserId();
                std::string baseUser = email.substr(0, email.find('@'));
                newUser.username = baseUser + "_" + std::to_string(std::abs((int)std::hash<std::string>{}(email)) % 1000);
                newUser.email = email;
                newUser.password_hash = "GOOGLE_AUTH";
                newUser.fullname = fullname.empty() ? baseUser : fullname;
                newUser.avatar = avatar.empty() ? "/img/default-avatar.svg" : avatar;
                newUser.bio = "Gia nhập qua tài khoản Google";
                newUser.google_id = "GOOG_" + std::to_string(std::abs((int)std::hash<std::string>{}(email)));
                newUser.role = "USER";
                newUser.status = "ACTIVE";

                if (db.createUser(newUser)) {
                    db.updateHeartbeat(newUser.id);
                    res.set_content(json({
                        {"status", "ok"},
                        {"message", "Tạo tài khoản Google mới thành công!"},
                        {"user", newUser.toJson()}
                    }).dump(), "application/json; charset=utf-8");
                } else {
                    res.status = 500;
                    res.set_content(json({{"status", "error"}, {"message", "Không thể tạo tài khoản từ Google!"}}).dump(), "application/json; charset=utf-8");
                }
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Heartbeat để xác định ai thực sự đang online (Hình 4)
    svr.Post("/api/users/heartbeat", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            if (!userId.empty()) {
                db.updateHeartbeat(userId);
            }
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        } catch (...) {
            res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Trạng thái đang soạn tin nhắn (Typing Indicator)
    svr.Post("/api/users/typing", [&db](const httplib::Request& req, httplib::Response& res) {
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

    svr.Get("/api/users/typing", [&db](const httplib::Request& req, httplib::Response& res) {
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
    svr.Get("/api/friends/suggest", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string filter = req.has_param("filter") ? req.get_param_value("filter") : "ALL";
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 8;

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

    svr.Get("/api/friends/full", [&db](const httplib::Request& req, httplib::Response& res) {
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
    svr.Post("/api/friends/request", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string myId = body.value("my_id", "");
            std::string targetId = body.value("target_id", "");

            if (myId.empty() || targetId.empty() || myId == targetId) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Dữ liệu yêu cầu kết bạn không hợp lệ!"}}).dump(), "application/json; charset=utf-8");
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Phản hồi lời mời kết bạn (ACCEPT / REJECT)
    svr.Post("/api/friends/respond", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Hủy gửi lời mời kết bạn (Cancel friend request)
    svr.Post("/api/friends/cancel", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Hủy kết bạn (Unfriend)
    svr.Post("/api/friends/unfriend", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string myId = getJsonStringSafe(body, "my_id");
            std::string friendId = getJsonStringSafe(body, "friend_id");
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Danh sách lời mời kết bạn gửi đến tôi
    svr.Get("/api/friends/requests", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        auto list = db.getPendingRequests(userId);
        json arr = json::array();
        for (const auto& u : list) arr.push_back(u.toJson());
        res.set_content(json({{"status", "ok"}, {"requests", arr}}).dump(), "application/json; charset=utf-8");
    });

    // Danh sách bạn bè kèm tin nhắn mới nhất và trạng thái online (Hình 4)
    svr.Get("/api/friends/contacts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        json contacts = db.getFriendsWithLastMessage(userId);
        res.set_content(json({{"status", "ok"}, {"contacts", contacts}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 3. XEM PROFILE CỦA BẠN BÈ & BẢN THÂN
    // -------------------------------------------------------------
    svr.Get("/api/users/profile", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string targetId = req.get_param_value("id");
        std::string myId = req.get_param_value("my_id");

        User user;
        if (db.getUserById(targetId, user) || db.getUserByToken(targetId, user)) {
            std::string realId = user.id;
            std::string relation = "NONE";
            if (!myId.empty() && myId != realId) {
                relation = db.getFriendshipRelation(myId, realId);
            }
            auto friends = db.getFriendsList(realId);

            res.set_content(json({
                {"status", "ok"},
                {"user", user.toJson()},
                {"relation", relation},
                {"friend_count", friends.size()}
            }).dump(), "application/json; charset=utf-8");
        } else {
            res.status = 404;
            res.set_content(json({{"status", "error"}, {"message", "Không tìm thấy người dùng!"}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/users/update", [&db](const httplib::Request& req, httplib::Response& res) {
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
                res.set_content(json({{"status", "ok"}, {"message", "Cập nhật hồ sơ thành công!"}, {"user", user.toJson()}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật dữ liệu!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/users/change-password", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = getJsonStringSafe(body, "user_id");
            std::string oldPass = getJsonStringSafe(body, "old_password");
            std::string newPass = getJsonStringSafe(body, "new_password");
            std::string confirmPass = getJsonStringSafe(body, "confirm_password");

            if (userId.empty() || oldPass.empty() || newPass.empty() || confirmPass.empty()) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Vui lòng nhập đầy đủ thông tin!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (newPass != confirmPass) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Mật khẩu xác nhận không khớp!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (newPass.length() < 6) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Mật khẩu mới phải có ít nhất 6 ký tự!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            User u;
            if (!db.getUserById(userId, u)) {
                res.status = 404;
                res.set_content(json({{"status", "error"}, {"message", "Người dùng không tồn tại!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            if (u.password_hash != oldPass) {
                res.status = 400;
                res.set_content(json({{"status", "error"}, {"message", "Mật khẩu cũ không chính xác!"}}).dump(), "application/json; charset=utf-8");
                return;
            }

            u.password_hash = newPass;
            if (db.updateUser(u)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đổi mật khẩu thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật mật khẩu trong CSDL!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/users/block", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 4. BÀI ĐĂNG (ẢNH, VIDEO, THẢ CẢM XÚC, CHIA SẺ, BÌNH LUẬN)
    // -------------------------------------------------------------
    svr.Get("/api/posts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string currentUserId = req.get_param_value("user_id");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 20;
        int offset = req.has_param("offset") ? std::stoi(req.get_param_value("offset")) : 0;

        auto posts = db.getFeedPosts(currentUserId, limit, offset);
        json arr = json::array();
        for (const auto& p : posts) arr.push_back(p.toJson());
        res.set_content(json({{"status", "ok"}, {"posts", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Post("/api/posts/create", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string authorId = body.value("author_id", "");
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
                        {"category", mod.category},
                        {"category_label", mod.category_label},
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
            std::string outId;
            if (db.createPostWithMedia(authorId, content, mediaType, mediaUrl, sharedPostId, sharedAuthorName, sharedContent,
                                       title, visibility, mod.category, mod.category_label, modStatus, mod.action, mod.reason, &outId)) {
                json resp = {
                    {"status", "ok"},
                    {"message", mod.action == "FLAG"
                        ? "Bài viết đã đăng nhưng được đánh dấu kiểm duyệt: " + mod.reason
                        : "Đã đăng bài viết thành công!"},
                    {"post_id", outId},
                    {"moderation", {
                        {"category", mod.category},
                        {"category_label", mod.category_label},
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // ĐĂNG LẠI BÀI VIẾT (REPOST)
    // -------------------------------------------------------------
    svr.Post("/api/posts/repost", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/posts/edit", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Thả cảm xúc (LIKE, LOVE, HAHA, WOW, SAD, ANGRY, NONE)
    svr.Post("/api/posts/reaction", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Danh sách người đã thả cảm xúc (Hình 2)
    svr.Get("/api/posts/reactions-list", [&db](const httplib::Request& req, httplib::Response& res) {
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

    svr.Post("/api/posts/comment", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            std::string authorId = body.value("author_id", "");
            std::string content = body.value("content", "");
            std::string replyToId = body.value("reply_to_id", "");
            std::string replyToAuthor = body.value("reply_to_author", "");

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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    };
    svr.Post("/api/comments/delete", deleteCommentHandler);
    svr.Delete("/api/comments/delete", deleteCommentHandler);

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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    };
    svr.Post("/api/posts/delete", deletePostHandler);
    svr.Delete("/api/posts/delete", deletePostHandler);

    // Lấy bài viết của riêng 1 người dùng (cho trang profile)
    svr.Get("/api/users/posts", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string authorId = req.get_param_value("author_id");
        std::string currentUserId = req.get_param_value("current_user_id");

        if (authorId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu author_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        User u;
        if (db.getUserByToken(authorId, u)) {
            authorId = u.id;
        }

        auto posts = db.getUserPosts(authorId, currentUserId);
        json arr = json::array();
        for (const auto& p : posts) arr.push_back(p.toJson());
        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"posts", arr}}).dump(), "application/json; charset=utf-8");
    });

    // Lấy chi tiết 1 bài viết kèm bình luận (cho Modal xem bình luận - Hình 2)
    svr.Get("/api/posts/detail", [&db](const httplib::Request& req, httplib::Response& res) {
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
    svr.Get("/api/messages", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        int beforeId = req.has_param("before_id") ? std::stoi(req.get_param_value("before_id")) : 0;
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 30; // Tăng mặc định lên 30

        if (myId.empty() || partnerId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu my_id hoặc partner_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }

        bool hasMore = false;
        auto msgs = db.getPaginatedMessages(myId, partnerId, beforeId, limit, hasMore);
        json arr = json::array();
        for (const auto& m : msgs) arr.push_back(m.toJson());

        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"has_more", hasMore}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Get("/api/messages/around", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        int targetId = 0;
        try { targetId = std::stoi(req.get_param_value("target_id")); } catch(...) {}
        bool isGroup = (req.get_param_value("is_group") == "true");

        if (isGroup) {
            auto msgs = db.getGroupMessagesAround(partnerId, targetId, 15);
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        } else {
            auto msgs = db.getMessagesAround(myId, partnerId, targetId, 15);
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Tìm kiếm tin nhắn không phân biệt chữ hoa, chữ thường, có dấu và không dấu
    svr.Get("/api/messages/search", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("my_id");
        std::string partnerId = req.get_param_value("partner_id");
        std::string query = req.get_param_value("q");
        bool isGroup = (req.get_param_value("is_group") == "true");

        if (query.empty()) {
            res.set_content(json({{"status", "ok"}, {"messages", json::array()}}).dump(), "application/json; charset=utf-8");
            return;
        }

        if (isGroup) {
            auto msgs = db.searchGroupMessages(partnerId, query, myId);
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        } else {
            auto msgs = db.searchMessages(myId, partnerId, query);
            json arr = json::array();
            for (const auto& m : msgs) arr.push_back(m.toJson());
            res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/messages/send", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string senderId = body.value("sender_id", "");
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

            Message outMsg;
            if (db.sendMessage(senderId, recipientId, content, "", outMsg, mediaType, mediaUrl, replyToText, replyToId)) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/messages/recall", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Post("/api/messages/delete", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/messages/react", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string reaction = body.value("reaction", "");
            if (db.reactMessage(msgId, reaction)) {
                res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi thả cảm xúc!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Get("/api/messages/read-status", [&db](const httplib::Request& req, httplib::Response& res) {
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

    svr.Post("/api/friends/nickname", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            std::string friendId = body.value("friend_id", "");
            std::string nickname = body.value("nickname", "");
            if (userId.empty() || friendId.empty()) { res.status = 400; res.set_content("{}", "application/json"); return; }
            db.setNickname(userId, friendId, nickname);
            res.set_content(json({{"status", "ok"}, {"message", "Đã đặt biệt danh!"}}).dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Get("/api/friends/nickname", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        std::string friendId = req.get_param_value("friend_id");
        std::string nick = db.getNickname(userId, friendId);
        res.set_content(json({{"status", "ok"}, {"nickname", nick}}).dump(), "application/json; charset=utf-8");
    });

    svr.Delete("/api/messages/clear", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string u1 = body.value("my_id", "");
            std::string u2 = body.value("partner_id", "");
            if (db.clearConversation(u1, u2)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa toàn bộ đoạn tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi xóa tin nhắn!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/messages/share-post", [&db](const httplib::Request& req, httplib::Response& res) {
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
                    res.set_content(json({{"status", "ok"}, {"message", "Đã chia sẻ bài viết vào nhóm!"}}).dump(), "application/json; charset=utf-8");
                } else {
                    res.status = 500;
                    res.set_content(json({{"status", "error"}, {"message", "Không thể chia sẻ bài viết vào nhóm!"}}).dump(), "application/json; charset=utf-8");
                }
            } else {
                Message msg;
                if (db.sendMessage(senderId, targetId, shareText, postId, msg)) {
                    res.set_content(json({{"status", "ok"}, {"message", "Đã chia sẻ bài viết vào hộp thoại trò chuyện!"}}).dump(), "application/json; charset=utf-8");
                } else {
                    res.status = 500;
                    res.set_content(json({{"status", "error"}, {"message", "Không thể chia sẻ bài viết!"}}).dump(), "application/json; charset=utf-8");
                }
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 6. TẠO NHÓM VÀ QUẢN LÝ NHÓM & NHẮN TIN NHÓM
    // -------------------------------------------------------------
    svr.Post("/api/groups/update-avatar", [&db](const httplib::Request& req, httplib::Response& res) {
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
            if (db.updateGroupAvatar(groupId, avatar)) {
                res.set_content(json({{"status", "ok"}, {"message", "Cập nhật avatar nhóm thành công!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật avatar!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/groups/update-name", [&db](const httplib::Request& req, httplib::Response& res) {
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

            if (!userId.empty()) {
                std::string role = db.getGroupRole(groupId, userId);
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/groups/create", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Get("/api/groups/my-groups", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        auto list = db.getUserGroups(userId);
        json arr = json::array();
        for (const auto& g : list) arr.push_back(g.toJson());
        res.set_content(json({{"status", "ok"}, {"groups", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Get("/api/groups/messages", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string groupId = req.get_param_value("group_id");
        std::string viewerId = req.get_param_value("viewer_id");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 30;
        if (groupId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu group_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        auto msgs = db.getGroupMessages(groupId, limit, viewerId);
        json arr = json::array();
        for (const auto& m : msgs) arr.push_back(m.toJson());
        res.set_content(json({{"status", "ok"}, {"count", arr.size()}, {"messages", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Post("/api/groups/messages/send", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // Group management endpoints
    svr.Get("/api/groups/members", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string groupId = req.get_param_value("group_id");
        if (groupId.empty()) { res.status = 400; res.set_content("{}", "application/json"); return; }
        json members = db.getGroupMembers(groupId);
        res.set_content(json({{"status", "ok"}, {"members", members}}).dump(), "application/json; charset=utf-8");
    });

    svr.Post("/api/groups/members/add", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string groupId = body.value("group_id", "");
            std::string userId = body.value("user_id", "");
            std::string requesterId = body.value("requester_id", "");
            if (groupId.empty() || userId.empty()) { res.status = 400; res.set_content("{}", "application/json"); return; }
            if (db.addGroupMember(groupId, userId, requesterId)) {
                // Send system message
                GroupMessage sysMsg;
                User addedUser; db.getUserById(userId, addedUser);
                db.sendGroupMessage(groupId, requesterId, addedUser.fullname + " đã được thêm vào nhóm", "", sysMsg);
                res.set_content(json({{"status", "ok"}, {"message", "Đã thêm thành viên!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể thêm thành viên!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Post("/api/groups/members/remove", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Post("/api/groups/leave", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Post("/api/groups/messages/recall", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Post("/api/groups/messages/delete", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.status = 400; res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });



    svr.Get("/api/groups/friends-to-add", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string myId = req.get_param_value("user_id");
        std::string groupId = req.get_param_value("group_id");
        json friends = db.getFriendsNotInGroup(myId, groupId);
        res.set_content(json({{"status", "ok"}, {"friends", friends}}).dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // 7. TÌM KIẾM THỜI GIAN THỰC & TRANG TÌM KIẾM CÓ BỘ LỌC (Hình 3 & 4)
    // -------------------------------------------------------------
    svr.Get("/api/search", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string keyword = req.has_param("q") ? req.get_param_value("q") : (req.has_param("keyword") ? req.get_param_value("keyword") : "");
        std::string filter = req.has_param("filter") ? req.get_param_value("filter") : "ALL";
        std::string currentUserId = req.get_param_value("user_id");

        if (keyword.empty()) {
            res.set_content(json({{"status", "ok"}, {"users", json::array()}, {"groups", json::array()}, {"posts", json::array()}}).dump(), "application/json; charset=utf-8");
            return;
        }

        json results = db.searchEverything(keyword, filter, currentUserId);
        results["status"] = "ok";
        res.set_content(results.dump(), "application/json; charset=utf-8");
    });

    // -------------------------------------------------------------
    // LỊCH SỬ TÌM KIẾM
    // -------------------------------------------------------------
    svr.Get("/api/search/history", [&db](const httplib::Request& req, httplib::Response& res) {
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

    svr.Post("/api/search/history", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
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
    svr.Delete("/api/search/history", deleteSearchHistoryHandler);
    svr.Post("/api/search/history/delete", deleteSearchHistoryHandler);

    // -------------------------------------------------------------
    // 8. THÔNG BÁO (HÌNH CÁI CHUÔNG - Hình 4)
    // -------------------------------------------------------------
    svr.Get("/api/notifications", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string userId = req.get_param_value("user_id");
        if (userId.empty()) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", "Thiếu user_id!"}}).dump(), "application/json; charset=utf-8");
            return;
        }
        auto list = db.getUserNotifications(userId);
        json arr = json::array();
        int unreadCount = 0;
        for (const auto& n : list) {
            if (n.is_read == 0) unreadCount++;
            arr.push_back(n.toJson());
        }
        res.set_content(json({{"status", "ok"}, {"unread_count", unreadCount}, {"notifications", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Post("/api/notifications/read", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/groups/messages/react", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int msgId = body.value("message_id", 0);
            std::string reaction = body.value("reaction", "");
            if (db.reactGroupMessage(msgId, reaction)) {
                res.set_content(json({{"status", "ok"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi thả cảm xúc tin nhắn nhóm!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 9. BÁO CÁO VI PHẠM TỚI ADMIN
    // -------------------------------------------------------------
    svr.Post("/api/reports/create", [&db](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    // -------------------------------------------------------------
    // 8. TÍNH NĂNG QUẢN TRỊ VIÊN (ADMIN DASHBOARD)
    // -------------------------------------------------------------
    svr.Get("/api/admin/stats", [&db](const httplib::Request& req, httplib::Response& res) {
        json stats = db.getAdminStats();
        res.set_content(json({{"status", "ok"}, {"stats", stats}}).dump(), "application/json; charset=utf-8");
    });

    svr.Get("/api/admin/users", [&db](const httplib::Request& req, httplib::Response& res) {
        auto list = db.getAllUsers();
        json arr = json::array();
        for (const auto& u : list) arr.push_back(u.toJson(true));
        res.set_content(json({{"status", "ok"}, {"users", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Post("/api/admin/toggle-lock", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string userId = body.value("user_id", "");
            bool nowLocked = false;
            if (db.toggleUserLock(userId, nowLocked)) {
                std::string msg = nowLocked ? "Đã khóa tài khoản thành công!" : "Đã mở khóa tài khoản thành công!";
                res.set_content(json({{"status", "ok"}, {"is_locked", nowLocked}, {"message", msg}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi cập nhật trạng thái tài khoản!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Get("/api/admin/reports", [&db](const httplib::Request& req, httplib::Response& res) {
        auto list = db.getAllReports();
        json arr = json::array();
        for (const auto& r : list) arr.push_back(r.toJson());
        res.set_content(json({{"status", "ok"}, {"reports", arr}}).dump(), "application/json; charset=utf-8");
    });

    svr.Post("/api/admin/resolve-report", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int reportId = body.value("report_id", 0);
            std::string action = body.value("action", "RESOLVED"); // RESOLVED hoặc DISMISSED
            if (db.resolveReport(reportId, action)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xử lý báo cáo!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Lỗi xử lý báo cáo!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    svr.Post("/api/admin/delete-post", [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string postId = body.value("post_id", "");
            if (db.deletePost(postId)) {
                res.set_content(json({{"status", "ok"}, {"message", "Đã xóa bài viết vi phạm!"}}).dump(), "application/json; charset=utf-8");
            } else {
                res.status = 500;
                res.set_content(json({{"status", "error"}, {"message", "Không thể xóa bài viết!"}}).dump(), "application/json; charset=utf-8");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json; charset=utf-8");
        }
    });

    int port = 8080;

    if (const char* envPort = std::getenv("PORT")) {
        try {
            port = std::stoi(envPort);
        }
        catch (...) {
            std::cerr << "PORT không hợp lệ!" << std::endl;
            return 1;
        }
    }
    
    std::cout
        << "[Server] Running on port: "
        << port << std::endl;
    
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Không thể khởi động server!" << std::endl;
        return 1;
    }
    
    return 0;
}
