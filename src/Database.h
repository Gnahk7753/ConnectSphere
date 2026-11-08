#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <cmath>
#include <cctype>
#include <sqlite3.h>
#include "Models.h"

class Database {
private:
    sqlite3* db = nullptr;
    std::mutex dbMutex;

    struct TypingEntry {
        bool isTyping = false;
        std::chrono::steady_clock::time_point lastUpdated;
    };
    std::unordered_map<std::string, TypingEntry> typingMap;
    std::mutex typingMutex;

    static std::string getCurrentTimeStr() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

public:
    Database() {}
    ~Database() {
        close();
    }

    bool init(const std::string& dbPath = "data/social_network.db") {
        std::lock_guard<std::mutex> lock(dbMutex);
        int rc = sqlite3_open(dbPath.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << "Không thể mở SQLite DB: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

        createTables();
        seedDataIfEmpty();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(dbMutex);
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
    }

private:
    void executeSimple(const std::string& sql) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL Error: " << (errMsg ? errMsg : "Unknown") << " in query: " << sql << std::endl;
            sqlite3_free(errMsg);
        }
    }

    void createTables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS users (
                id TEXT PRIMARY KEY,
                username TEXT UNIQUE NOT NULL,
                email TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                fullname TEXT NOT NULL,
                avatar TEXT DEFAULT '',
                bio TEXT DEFAULT '',
                hometown TEXT DEFAULT '',
                school TEXT DEFAULT '',
                workplace TEXT DEFAULT '',
                interests TEXT DEFAULT '',
                role TEXT DEFAULT 'USER',
                status TEXT DEFAULT 'ACTIVE',
                google_id TEXT DEFAULT '',
                last_active_at TEXT DEFAULT '',
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS friendships (
                user_id_1 TEXT NOT NULL,
                user_id_2 TEXT NOT NULL,
                status TEXT NOT NULL, -- 'FRIEND', 'BLOCKED'
                created_at TEXT NOT NULL,
                PRIMARY KEY (user_id_1, user_id_2)
            );

            CREATE TABLE IF NOT EXISTS friend_requests (
                requester_id TEXT NOT NULL,
                receiver_id TEXT NOT NULL,
                status TEXT NOT NULL, -- 'PENDING', 'ACCEPTED', 'REJECTED'
                created_at TEXT NOT NULL,
                PRIMARY KEY (requester_id, receiver_id)
            );

            CREATE TABLE IF NOT EXISTS posts (
                id TEXT PRIMARY KEY,
                author_id TEXT NOT NULL,
                content TEXT NOT NULL,
                media_type TEXT DEFAULT 'NONE', -- 'NONE', 'IMAGE', 'VIDEO'
                media_url TEXT DEFAULT '',
                shared_post_id TEXT DEFAULT '',
                shared_author_name TEXT DEFAULT '',
                shared_content TEXT DEFAULT '',
                created_at TEXT NOT NULL,
                FOREIGN KEY (author_id) REFERENCES users(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS reactions (
                post_id TEXT NOT NULL,
                user_id TEXT NOT NULL,
                type TEXT NOT NULL, -- 'LIKE', 'LOVE', 'HAHA', 'WOW', 'SAD', 'ANGRY'
                created_at TEXT NOT NULL,
                PRIMARY KEY (post_id, user_id),
                FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS comments (
                id TEXT PRIMARY KEY,
                post_id TEXT NOT NULL,
                author_id TEXT NOT NULL,
                content TEXT NOT NULL,
                reply_to_id TEXT DEFAULT '',
                reply_to_author TEXT DEFAULT '',
                created_at TEXT NOT NULL,
                FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
                FOREIGN KEY (author_id) REFERENCES users(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conversation_id TEXT NOT NULL,
                sender_id TEXT NOT NULL,
                recipient_id TEXT NOT NULL,
                content TEXT NOT NULL,
                shared_post_id TEXT DEFAULT '',
                is_revoked INTEGER DEFAULT 0,
                is_read INTEGER DEFAULT 0,
                created_at TEXT NOT NULL,
                FOREIGN KEY (sender_id) REFERENCES users(id),
                FOREIGN KEY (recipient_id) REFERENCES users(id)
            );

            CREATE TABLE IF NOT EXISTS groups (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                avatar TEXT DEFAULT '',
                created_by TEXT NOT NULL,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS group_members (
                group_id TEXT NOT NULL,
                user_id TEXT NOT NULL,
                role TEXT DEFAULT 'MEMBER',
                joined_at TEXT NOT NULL,
                PRIMARY KEY (group_id, user_id)
            );

            CREATE TABLE IF NOT EXISTS group_messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                group_id TEXT NOT NULL,
                sender_id TEXT NOT NULL,
                content TEXT NOT NULL,
                shared_post_id TEXT DEFAULT '',
                created_at TEXT NOT NULL,
                FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE,
                FOREIGN KEY (sender_id) REFERENCES users(id)
            );

            CREATE TABLE IF NOT EXISTS notifications (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                actor_id TEXT NOT NULL,
                type TEXT NOT NULL,
                content TEXT NOT NULL,
                link TEXT DEFAULT '',
                is_read INTEGER DEFAULT 0,
                created_at TEXT NOT NULL,
                FOREIGN KEY (user_id) REFERENCES users(id)
            );

            CREATE TABLE IF NOT EXISTS reports (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                reporter_id TEXT NOT NULL,
                reported_user_id TEXT DEFAULT '',
                reported_post_id TEXT DEFAULT '',
                reported_comment_id TEXT DEFAULT '',
                reason TEXT NOT NULL,
                status TEXT DEFAULT 'PENDING',
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS otp_codes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                email TEXT NOT NULL,
                code TEXT NOT NULL,
                purpose TEXT NOT NULL,
                expires_at INTEGER NOT NULL,
                is_used INTEGER DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS search_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id TEXT NOT NULL,
                query TEXT NOT NULL,
                created_at TEXT NOT NULL,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            );

            CREATE INDEX IF NOT EXISTS idx_messages_conv ON messages(conversation_id, id);
            CREATE INDEX IF NOT EXISTS idx_posts_time ON posts(created_at DESC);
            CREATE INDEX IF NOT EXISTS idx_search_hist_user ON search_history(user_id, created_at DESC);
        )";
        executeSimple(sql);

        // Đảm bảo các cột mới luôn tồn tại trên database hiện hữu
        auto tryAlter = [this](const char* sql) {
            sqlite3_exec(db, sql, nullptr, nullptr, nullptr); // ignore error silently
        };
        tryAlter("ALTER TABLE messages ADD COLUMN shared_post_id TEXT DEFAULT '';");
        tryAlter("ALTER TABLE messages ADD COLUMN media_type TEXT DEFAULT 'NONE';");
        tryAlter("ALTER TABLE messages ADD COLUMN media_url TEXT DEFAULT '';");
        tryAlter("ALTER TABLE group_messages ADD COLUMN shared_post_id TEXT DEFAULT '';");
        tryAlter("ALTER TABLE group_messages ADD COLUMN media_type TEXT DEFAULT 'NONE';");
        tryAlter("ALTER TABLE group_messages ADD COLUMN media_url TEXT DEFAULT '';");
        tryAlter("ALTER TABLE group_messages ADD COLUMN is_revoked INTEGER DEFAULT 0;");
        tryAlter("ALTER TABLE messages ADD COLUMN reaction TEXT DEFAULT '';");
        tryAlter("ALTER TABLE messages ADD COLUMN reply_to_text TEXT DEFAULT '';");
        tryAlter("ALTER TABLE messages ADD COLUMN reply_to_id INTEGER DEFAULT 0;");
        tryAlter("ALTER TABLE group_messages ADD COLUMN reaction TEXT DEFAULT '';");
        tryAlter("ALTER TABLE group_messages ADD COLUMN reply_to_text TEXT DEFAULT '';");
        tryAlter("ALTER TABLE group_messages ADD COLUMN reply_to_id INTEGER DEFAULT 0;");
        tryAlter("ALTER TABLE comments ADD COLUMN reply_to_id TEXT DEFAULT '';");
        tryAlter("ALTER TABLE comments ADD COLUMN reply_to_author TEXT DEFAULT '';");
        tryAlter("ALTER TABLE reports ADD COLUMN reported_comment_id TEXT DEFAULT '';");
        tryAlter("ALTER TABLE posts ADD COLUMN title TEXT DEFAULT '';");
        tryAlter("ALTER TABLE posts ADD COLUMN visibility TEXT DEFAULT 'PUBLIC';");
        tryAlter("ALTER TABLE posts ADD COLUMN category TEXT DEFAULT 'doi_song';");
        tryAlter("ALTER TABLE posts ADD COLUMN category_label TEXT DEFAULT 'Đời sống';");
        tryAlter("ALTER TABLE posts ADD COLUMN moderation_status TEXT DEFAULT 'SAFE';");
        tryAlter("ALTER TABLE posts ADD COLUMN moderation_action TEXT DEFAULT 'ALLOW';");
        tryAlter("ALTER TABLE posts ADD COLUMN moderation_reason TEXT DEFAULT '';");
        tryAlter("ALTER TABLE users ADD COLUMN gender TEXT DEFAULT '';");
        tryAlter("ALTER TABLE users ADD COLUMN birthday TEXT DEFAULT '';");
        tryAlter("ALTER TABLE users ADD COLUMN residence TEXT DEFAULT '';");
        tryAlter("ALTER TABLE users ADD COLUMN user_token TEXT DEFAULT '';");
        executeSimple("UPDATE users SET user_token = lower(hex(randomblob(8))) WHERE user_token IS NULL OR user_token = '';");
        // Table for chat nicknames
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS user_nicknames (
                user_id TEXT NOT NULL,
                friend_id TEXT NOT NULL,
                nickname TEXT NOT NULL,
                PRIMARY KEY(user_id, friend_id)
            );
        )");
        // Table for group message delete-for-me tracking
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS group_messages_deleted (
                user_id TEXT NOT NULL,
                message_id INTEGER NOT NULL,
                PRIMARY KEY(user_id, message_id)
            );
        )");
    }

    void seedDataIfEmpty() {
        sqlite3_stmt* stmt = nullptr;
        const char* checkSql = "SELECT COUNT(*) FROM users;";
        int count = 0;
        if (sqlite3_prepare_v2(db, checkSql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        if (count >= 10) return;

        // Xóa bảng cũ để nạp lại đầy đủ 10 tài khoản và 10 bài đăng
        executeSimple("DELETE FROM reports;");
        executeSimple("DELETE FROM notifications;");
        executeSimple("DELETE FROM group_messages;");
        executeSimple("DELETE FROM group_members;");
        executeSimple("DELETE FROM groups;");
        executeSimple("DELETE FROM messages;");
        executeSimple("DELETE FROM comments;");
        executeSimple("DELETE FROM reactions;");
        executeSimple("DELETE FROM posts;");
        executeSimple("DELETE FROM friend_requests;");
        executeSimple("DELETE FROM friendships;");
        executeSimple("DELETE FROM users;");

        std::string nowTime = getCurrentTimeStr();

        // 1. Quản trị viên Admin
        executeSimple("INSERT INTO users (id, username, email, password_hash, fullname, bio, hometown, school, workplace, interests, role, status, last_active_at, created_at, gender, birthday, residence, user_token) "
                      "VALUES ('ACC000000', 'admin', 'admin@mangxahoi.vn', 'Admin@123', 'Quản Trị Viên Hệ Thống', 'Ban quản trị và giám sát hệ thống Mạng Xã Hội Mini', 'Hà Nội', 'Đại học Bách Khoa Hà Nội', 'Admin Team', 'Lập trình;Bảo mật;Thuật toán', 'ADMIN', 'ACTIVE', '" + nowTime + "', '2026-10-01 00:00:00', 'Nam', '1995-01-01', 'Hà Nội', 'adm_token_999999');");

        // 2. 9 Tài khoản mẫu
        std::vector<std::vector<std::string>> sampleUsers = {
            {"ACC000001", "khoanm", "khoanm@gmail.com", "Khoa@123", "Nguyễn Minh Khoa", "Sống chậm lại và tận hưởng từng khoảnh khắc cuộc sống.", "Đà Nẵng", "Đại học Bách Khoa Hà Nội", "Tiki", "Đọc sách;Nghe nhạc;Lập trình;Bóng đá", "Nam", "2001-04-12", "Hà Nội", "504258007eab0d25"},
            {"ACC000002", "lanph", "lanph@gmail.com", "Lan@123", "Phạm Hương Lan", "Yêu cái đẹp, thích nhiếp ảnh và những quán cà phê góc phố.", "Hà Nội", "Đại học Ngoại Thương", "VNG Corporation", "Nhiếp ảnh;Du lịch;Cà phê;Nghe nhạc", "Nữ", "2002-08-20", "Hà Nội", "b7d2109f3e4a5c81"},
            {"ACC000003", "tuananh", "tuananh@gmail.com", "Tuan@123", "Vũ Tuấn Anh", "Kỹ sư phần mềm đam mê thuật toán và trí tuệ nhân tạo.", "Hải Phòng", "Đại học Bách Khoa Hà Nội", "FPT Software", "Lập trình;Chơi game;Bóng rổ;Đọc sách", "Nam", "2000-11-05", "Hà Nội", "c8e3210a4f5b6d92"},
            {"ACC000004", "ngocanh", "ngocanh@gmail.com", "Anh@123", "Phạm Ngọc Anh", "Design is thinking made visual. Yêu nghệ thuật và màu sắc.", "Đà Nẵng", "Đại học Kiến trúc Hà Nội", "Shopee", "Hội họa;Nhiếp ảnh;Du lịch", "Nữ", "2002-03-15", "Đà Nẵng", "d9f4321b5a6c7e03"},
            {"ACC000005", "hieutran", "hieutran@gmail.com", "Hieu@123", "Trần Minh Hiếu", "Chạy bộ mỗi sáng và kết nối với những người bạn tích cực.", "TP. Hồ Chí Minh", "Đại học Kinh tế TP.HCM", "MoMo", "Chạy bộ;Bóng đá;Đọc sách", "Nam", "1999-07-28", "TP. Hồ Chí Minh", "e0a5432c6b7d8f14"},
            {"ACC000006", "myduyen", "myduyen@gmail.com", "Duyen@123", "Ngô Mỹ Duyên", "Cuộc đời là những chuyến đi khám phá ẩm thực và văn hóa.", "Cần Thơ", "Đại học Cần Thơ", "Grab", "Du lịch;Nấu ăn;Yoga;Cà phê", "Nữ", "2001-09-09", "Cần Thơ", "f1b6543d7c8e9a25"},
            {"ACC000007", "hoangnam", "hoangnam@gmail.com", "Nam@123", "Lê Hoàng Nam", "Đam mê bóng đá, bơi lội và công nghệ viễn thông mới.", "Hà Nội", "Đại học Bách Khoa Hà Nội", "Viettel Telecom", "Bóng đá;Bơi lội;Xem phim", "Nam", "2000-02-18", "Hà Nội", "a2c7654e8d9f0b36"},
            {"ACC000008", "thuha", "thuha@gmail.com", "Ha@123", "Đỗ Thu Hà", "Thích sự yên bình, viết lách, sách và những giai điệu acoustic.", "Đà Nẵng", "Đại học Ngoại Thương", "VinFast", "Nghe nhạc;Viết lách;Du lịch", "Nữ", "2003-05-22", "Đà Nẵng", "b3d8765f9e0a1c47"},
            {"ACC000009", "quanghuy", "quanghuy@gmail.com", "Huy@123", "Bùi Quang Huy", "Lập trình viên C++, đam mê eSports và thể thao đối kháng.", "Hải Phòng", "Đại học Hàng hải Việt Nam", "CMC Global", "Chơi game;Cầu lông;Lập trình", "Nam", "2001-12-30", "Hải Phòng", "c4e9876a0f1b2d58"}
        };

        for (const auto& u : sampleUsers) {
            std::string sql = "INSERT INTO users (id, username, email, password_hash, fullname, bio, hometown, school, workplace, interests, role, status, last_active_at, created_at, gender, birthday, residence, user_token) VALUES ('"
                + u[0] + "', '" + u[1] + "', '" + u[2] + "', '" + u[3] + "', '" + u[4] + "', '" + u[5] + "', '"
                + u[6] + "', '" + u[7] + "', '" + u[8] + "', '" + u[9] + "', 'USER', 'ACTIVE', '" + nowTime + "', '2026-10-05 10:00:00', '"
                + u[10] + "', '" + u[11] + "', '" + u[12] + "', '" + u[13] + "');";
            executeSimple(sql);
        }

        // 3. Đồ thị bạn bè
        std::vector<std::pair<std::string, std::string>> friendships = {
            {"ACC000001", "ACC000002"}, {"ACC000001", "ACC000003"}, {"ACC000001", "ACC000004"}, {"ACC000001", "ACC000005"},
            {"ACC000002", "ACC000004"}, {"ACC000002", "ACC000006"}, {"ACC000002", "ACC000008"},
            {"ACC000003", "ACC000005"}, {"ACC000003", "ACC000007"}, {"ACC000003", "ACC000009"},
            {"ACC000004", "ACC000008"}, {"ACC000005", "ACC000007"}, {"ACC000006", "ACC000008"}, {"ACC000007", "ACC000009"}
        };

        for (const auto& f : friendships) {
            executeSimple("INSERT OR IGNORE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES ('" + f.first + "', '" + f.second + "', 'FRIEND', '" + nowTime + "');");
            executeSimple("INSERT OR IGNORE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES ('" + f.second + "', '" + f.first + "', 'FRIEND', '" + nowTime + "');");
        }

        // Lời mời kết bạn mẫu
        executeSimple("INSERT OR IGNORE INTO friend_requests (requester_id, receiver_id, status, created_at) VALUES ('ACC000007', 'ACC000001', 'PENDING', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO friend_requests (requester_id, receiver_id, status, created_at) VALUES ('ACC000008', 'ACC000001', 'PENDING', '" + nowTime + "');");

        // 4. 10 Bài đăng mẫu
        executeSimple(R"(
            INSERT INTO posts (id, author_id, title, content, media_type, media_url, created_at, visibility) VALUES
            ('POST000001', 'ACC000001', 'Ra mắt Mạng Xã Hội Mini nền tảng C++ Web siêu tốc', 'Chào mừng mọi người đến với Mạng Xã Hội Mini phiên bản C++ Web! Tốc độ mượt mà, hỗ trợ gửi hình ảnh, video và thuật toán BFS gợi ý bạn bè chuẩn xác! 🚀✨', 'NONE', '', '2026-11-04 08:30:00', 'PUBLIC'),
            ('POST000002', 'ACC000002', 'Cà phê sáng mùa thu Hà Nội', 'Hôm nay trời Hà Nội se lạnh, nhâm nhi một tách cà phê trứng và ngắm phố phường thật tuyệt vời. ☕🍂 Chúc cả nhà một ngày làm việc tràn đầy năng lượng và sáng tạo!', 'IMAGE', 'https://images.unsplash.com/photo-1501339847302-ac426a4a7cbb?auto=format&fit=crop&w=800&q=80', '2026-11-04 09:15:00', 'PUBLIC'),
            ('POST000003', 'ACC000003', 'Tối ưu hóa hiệu năng cấu trúc dữ liệu Graph trong C++', 'Vừa hoàn thành module tính toán BFS cho thuật toán gợi ý bạn bè tầng 2 và tầng 3. Tận dụng Adjacency List kết hợp unordered_set giúp tốc độ duyệt nhanh hơn 10x so với ma trận kề truyền thống! 💻⚡', 'NONE', '', '2026-11-04 10:20:00', 'PUBLIC'),
            ('POST000004', 'ACC000004', 'Bộ thiết kế UI/UX mới cho ứng dụng di động', 'Mới hoàn thiện xong thiết kế cho dự án mới với tone màu xanh chủ đạo kết hợp typography tối giản. Mọi người cho mình xin một vài góp ý để hoàn thiện hơn nhé! 🎨📱', 'IMAGE', 'https://images.unsplash.com/photo-1507238691740-187a5b1d37b8?auto=format&fit=crop&w=800&q=80', '2026-11-04 11:45:00', 'PUBLIC'),
            ('POST000005', 'ACC000005', 'Hoàn thành cự ly chạy 10km sáng Chủ Nhật', 'Chạy bộ dọc cung đường bến Bạch Đằng sáng sớm không khí thật trong lành. 10km hoàn thành trong 52 phút, vượt mục tiêu tuần này! Đã có ai dậy sớm rèn luyện sức khỏe chưa nào? 🏃‍♂️🔥', 'IMAGE', 'https://images.unsplash.com/photo-1452626038306-9aae5e071dd3?auto=format&fit=crop&w=800&q=80', '2026-11-04 13:10:00', 'PUBLIC'),
            ('POST000006', 'ACC000006', 'Thử tài làm bánh mì hoa cúc tại nhà', 'Mẻ bánh mì hoa cúc thơm phức mùi bơ sữa vừa ra lò. Thớ bánh mềm dai chuẩn vị Pháp, thành công ngoài mong đợi trong những ngày cuối tuần thư giãn. 🍞🧈✨', 'IMAGE', 'https://images.unsplash.com/photo-1509440159596-0249088772ff?auto=format&fit=crop&w=800&q=80', '2026-11-04 14:35:00', 'PUBLIC'),
            ('POST000007', 'ACC000007', 'Trận cầu đỉnh cao cuối tuần', 'Trận bóng đá giao hữu giữa hai chi nhánh diễn ra vô cùng kịch tính đến những phút bù giờ cuối cùng. Đội nhà lội ngược dòng thắng nghẹt thở 4-3! Cảm ơn anh em đã cháy hết mình! ⚽🥅🏆', 'NONE', '', '2026-11-04 15:50:00', 'PUBLIC'),
            ('POST000008', 'ACC000008', 'Góc làm việc nhỏ và những cuốn sách yêu thích', 'Dành trọn buổi chiều để nghiền ngẫm cuốn "Tâm lý học về tiền" và "Sức mạnh của thói quen". Không gian làm việc ngăn nắp luôn là nguồn cảm hứng lớn nhất giúp mình tập trung cao độ. 📚🌿☕', 'IMAGE', 'https://images.unsplash.com/photo-1499750310107-5fef28a66643?auto=format&fit=crop&w=800&q=80', '2026-11-04 16:40:00', 'PUBLIC'),
            ('POST000009', 'ACC000009', 'Trải nghiệm lập trình giao diện Web bằng C++', 'Nhiều người nghĩ C++ chỉ để viết embedded hay game engine, nhưng khi kết hợp httplib với kiến trúc RESTful và SQLite, ta có một web server siêu nhẹ, khởi động chỉ mất 10ms và tiêu tốn chưa tới 15MB RAM! 🚀💻', 'NONE', '', '2026-11-04 17:15:00', 'PUBLIC'),
            ('POST000010', 'ACC000000', 'Thông báo nâng cấp hệ sinh thái Mạng Xã Hội Mini v2.0', 'Ban Quản Trị xin thông báo hệ thống đã hoàn tất nâng cấp các tính năng: Hộp thoại sở thích 283 lựa chọn chuẩn, Phân loại đoạn chat theo bạn bè & nhóm, Khung tìm kiếm thời gian thực thông minh và Báo cáo vi phạm tự động. Chúc các bạn có trải nghiệm tuyệt vời! 🛡️🌐🎉', 'NONE', '', '2026-11-04 18:00:00', 'PUBLIC');
        )");

        // 5. Cảm xúc phong phú
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000001', 'ACC000002', 'LOVE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000001', 'ACC000003', 'HAHA', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000001', 'ACC000004', 'LIKE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000002', 'ACC000001', 'LOVE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000002', 'ACC000004', 'WOW', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000002', 'ACC000006', 'LIKE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000003', 'ACC000001', 'LIKE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000003', 'ACC000009', 'WOW', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000003', 'ACC000007', 'HAHA', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000004', 'ACC000002', 'LOVE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000005', 'ACC000001', 'LOVE', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000006', 'ACC000008', 'WOW', '" + nowTime + "');");
        executeSimple("INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES ('POST000010', 'ACC000001', 'LOVE', '" + nowTime + "');");

        // 6. Bình luận mẫu
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, created_at) VALUES ('CMT000001', 'POST000001', 'ACC000002', 'Giao diện web trực quan, chế độ Dark Mode xịn sò quá bạn ơi! 😍', '2026-11-04 08:45:00');");
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000002', 'POST000001', 'ACC000003', 'Chuẩn luôn, chạy C++ Backend phản hồi tức thì luôn bạn ơi!', 'CMT000001', 'Phạm Hương Lan', '2026-11-04 09:05:00');");
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, created_at) VALUES ('CMT000003', 'POST000001', 'ACC000005', 'Chúc mừng team, giao diện nhìn rất hiện đại và tinh tế.', '', '', '2026-11-04 09:20:00');");

        executeSimple("INSERT INTO comments (id, post_id, author_id, content, created_at) VALUES ('CMT000004', 'POST000002', 'ACC000004', 'Quán cà phê góc phố nào thế nàng ơi, nhìn ấm cúng quá! ☕', '', '', '2026-11-04 09:30:00');");
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000005', 'POST000002', 'ACC000002', 'Góc Đinh Tiên Hoàng nhìn ra hồ Hoàn Kiếm đấy cậu ơi.', 'CMT000004', 'Phạm Ngọc Anh', '2026-11-04 09:45:00');");

        executeSimple("INSERT INTO comments (id, post_id, author_id, content, created_at) VALUES ('CMT000006', 'POST000003', 'ACC000001', 'Tuyệt vời bác ơi, độ phức tạp O(V + E) chạy mượt mà ngay cả với hàng triệu quan hệ!', '', '', '2026-11-04 10:35:00');");
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, created_at) VALUES ('CMT000007', 'POST000003', 'ACC000009', 'Cho xin link repo tham khảo với bro ơi, mình cũng đang nghiên cứu.', '', '', '2026-11-04 10:50:00');");

        // 7. Nhóm mẫu & tin nhắn nhóm
        executeSimple("INSERT INTO groups (id, name, description, created_by, created_at) VALUES ('GRP000001', 'Cộng Đồng Lập Trình C++ Việt Nam', 'Nơi giao lưu chia sẻ thuật toán và phát triển ứng dụng', 'ACC000001', '" + nowTime + "');");
        executeSimple("INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES ('GRP000001', 'ACC000001', 'ADMIN', '" + nowTime + "');");
        executeSimple("INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES ('GRP000001', 'ACC000002', 'MEMBER', '" + nowTime + "');");
        executeSimple("INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES ('GRP000001', 'ACC000003', 'MEMBER', '" + nowTime + "');");
        executeSimple("INSERT INTO group_messages (group_id, sender_id, content, created_at) VALUES ('GRP000001', 'ACC000001', 'Chào mừng các bạn đến với nhóm C++ Việt Nam!', '" + nowTime + "');");
        executeSimple("INSERT INTO group_messages (group_id, sender_id, content, created_at) VALUES ('GRP000001', 'ACC000002', 'Xin chào cả nhóm nhé!', '" + nowTime + "');");

        // 8. Tin nhắn 1-on-1 mẫu
        executeSimple("INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_read, created_at) VALUES ('ACC000001_ACC000002', 'ACC000001', 'ACC000002', 'Chào Lan, hôm nay bạn có ghé quán cà phê quen không?', 1, '" + nowTime + "');");
        executeSimple("INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_read, created_at) VALUES ('ACC000001_ACC000002', 'ACC000002', 'ACC000001', 'Chào Khoa, mình đang ngồi đây này, góc hồ Hoàn Kiếm đẹp lắm!', 0, '" + nowTime + "');");

        // 9. Thông báo mẫu
        executeSimple("INSERT INTO notifications (user_id, actor_id, type, content, link, is_read, created_at) VALUES ('ACC000001', 'ACC000002', 'LIKE', 'Phạm Hương Lan đã thả tim bài viết của bạn.', '/#post-POST000001', 0, '" + nowTime + "');");
        executeSimple("INSERT INTO notifications (user_id, actor_id, type, content, link, is_read, created_at) VALUES ('ACC000001', 'ACC000007', 'FRIEND_REQUEST', 'Lê Hoàng Nam đã gửi lời mời kết bạn.', 'friends.html?tab=requests', 0, '" + nowTime + "');");
    }

public:
    // --- USER PROFILE & HEARTBEAT ---
    std::string generateUserToken() {
        static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        std::string s;
        s.reserve(16);
        for (int i = 0; i < 16; ++i) {
            s += alphanum[rand() % (sizeof(alphanum) - 1)];
        }
        return s;
    }

    bool getUserById(const std::string& id, User& user) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token FROM users WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.id = (const char*)sqlite3_column_text(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            user.email = (const char*)sqlite3_column_text(stmt, 2);
            user.password_hash = (const char*)sqlite3_column_text(stmt, 3);
            user.fullname = (const char*)sqlite3_column_text(stmt, 4);
            user.avatar = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            user.bio = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            user.hometown = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            user.school = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            user.workplace = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            user.interests = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
            user.role = (const char*)sqlite3_column_text(stmt, 11);
            user.status = (const char*)sqlite3_column_text(stmt, 12);
            user.google_id = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
            user.last_active_at = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            user.created_at = (const char*)sqlite3_column_text(stmt, 15);
            user.gender = sqlite3_column_text(stmt, 16) ? (const char*)sqlite3_column_text(stmt, 16) : "";
            user.birthday = sqlite3_column_text(stmt, 17) ? (const char*)sqlite3_column_text(stmt, 17) : "";
            user.residence = sqlite3_column_text(stmt, 18) ? (const char*)sqlite3_column_text(stmt, 18) : "";
            user.user_token = sqlite3_column_text(stmt, 19) ? (const char*)sqlite3_column_text(stmt, 19) : "";
            found = true;
        }
        sqlite3_finalize(stmt);
        return found;
    }

    bool getUserByToken(const std::string& token, User& user) {
        if (token.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token FROM users WHERE user_token = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.id = (const char*)sqlite3_column_text(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            user.email = (const char*)sqlite3_column_text(stmt, 2);
            user.password_hash = (const char*)sqlite3_column_text(stmt, 3);
            user.fullname = (const char*)sqlite3_column_text(stmt, 4);
            user.avatar = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            user.bio = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            user.hometown = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            user.school = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            user.workplace = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            user.interests = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
            user.role = (const char*)sqlite3_column_text(stmt, 11);
            user.status = (const char*)sqlite3_column_text(stmt, 12);
            user.google_id = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
            user.last_active_at = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            user.created_at = (const char*)sqlite3_column_text(stmt, 15);
            user.gender = sqlite3_column_text(stmt, 16) ? (const char*)sqlite3_column_text(stmt, 16) : "";
            user.birthday = sqlite3_column_text(stmt, 17) ? (const char*)sqlite3_column_text(stmt, 17) : "";
            user.residence = sqlite3_column_text(stmt, 18) ? (const char*)sqlite3_column_text(stmt, 18) : "";
            user.user_token = sqlite3_column_text(stmt, 19) ? (const char*)sqlite3_column_text(stmt, 19) : "";
            found = true;
        }
        sqlite3_finalize(stmt);
        return found;
    }

    bool getUserByUsernameOrEmail(const std::string& identifier, User& user) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token FROM users WHERE username = ? OR email = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, identifier.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, identifier.c_str(), -1, SQLITE_STATIC);
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.id = (const char*)sqlite3_column_text(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            user.email = (const char*)sqlite3_column_text(stmt, 2);
            user.password_hash = (const char*)sqlite3_column_text(stmt, 3);
            user.fullname = (const char*)sqlite3_column_text(stmt, 4);
            user.avatar = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            user.bio = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            user.hometown = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            user.school = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            user.workplace = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            user.interests = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
            user.role = (const char*)sqlite3_column_text(stmt, 11);
            user.status = (const char*)sqlite3_column_text(stmt, 12);
            user.google_id = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
            user.last_active_at = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            user.created_at = (const char*)sqlite3_column_text(stmt, 15);
            user.gender = sqlite3_column_text(stmt, 16) ? (const char*)sqlite3_column_text(stmt, 16) : "";
            user.birthday = sqlite3_column_text(stmt, 17) ? (const char*)sqlite3_column_text(stmt, 17) : "";
            user.residence = sqlite3_column_text(stmt, 18) ? (const char*)sqlite3_column_text(stmt, 18) : "";
            user.user_token = sqlite3_column_text(stmt, 19) ? (const char*)sqlite3_column_text(stmt, 19) : "";
            found = true;
        }
        sqlite3_finalize(stmt);
        return found;
    }

    void updateHeartbeat(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string nowTime = getCurrentTimeStr();
        const char* sql = "UPDATE users SET last_active_at = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, nowTime.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    std::string generateNewUserId() {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT COUNT(*) FROM users;";
        sqlite3_stmt* stmt = nullptr;
        int count = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        std::stringstream ss;
        ss << "ACC" << std::setw(6) << std::setfill('0') << (count + 1);
        return ss.str();
    }

    bool createUser(const User& user) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "INSERT INTO users (id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        std::string timeStr = user.created_at.empty() ? getCurrentTimeStr() : user.created_at;
        std::string token = user.user_token.empty() ? generateUserToken() : user.user_token;

        sqlite3_bind_text(stmt, 1, user.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user.username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user.email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user.password_hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, user.fullname.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, user.avatar.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, user.bio.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, user.hometown.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, user.school.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, user.workplace.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, user.interests.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 12, user.role.empty() ? "USER" : user.role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 13, user.status.empty() ? "ACTIVE" : user.status.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 14, user.google_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 15, timeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 16, timeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 17, user.gender.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 18, user.birthday.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 19, user.residence.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 20, token.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool updateUser(const User& user) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE users SET fullname = ?, bio = ?, hometown = ?, school = ?, workplace = ?, interests = ?, avatar = ?, gender = ?, birthday = ?, residence = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, user.fullname.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, user.bio.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user.hometown.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user.school.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, user.workplace.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, user.interests.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, user.avatar.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, user.gender.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, user.birthday.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, user.residence.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, user.id.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    std::vector<User> getAllUsers() {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<User> list;
        const char* sql = "SELECT id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token FROM users;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            User u;
            u.id = (const char*)sqlite3_column_text(stmt, 0);
            u.username = (const char*)sqlite3_column_text(stmt, 1);
            u.email = (const char*)sqlite3_column_text(stmt, 2);
            u.password_hash = (const char*)sqlite3_column_text(stmt, 3);
            u.fullname = (const char*)sqlite3_column_text(stmt, 4);
            u.avatar = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            u.bio = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            u.hometown = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            u.school = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            u.workplace = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            u.interests = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
            u.role = (const char*)sqlite3_column_text(stmt, 11);
            u.status = (const char*)sqlite3_column_text(stmt, 12);
            u.google_id = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
            u.last_active_at = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            u.created_at = (const char*)sqlite3_column_text(stmt, 15);
            u.gender = sqlite3_column_text(stmt, 16) ? (const char*)sqlite3_column_text(stmt, 16) : "";
            u.birthday = sqlite3_column_text(stmt, 17) ? (const char*)sqlite3_column_text(stmt, 17) : "";
            u.residence = sqlite3_column_text(stmt, 18) ? (const char*)sqlite3_column_text(stmt, 18) : "";
            u.user_token = sqlite3_column_text(stmt, 19) ? (const char*)sqlite3_column_text(stmt, 19) : "";
            list.push_back(u);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    // --- OTP CODES ---
    bool saveOtp(const std::string& email, const std::string& code, const std::string& purpose, int expirySeconds = 300) {
        std::lock_guard<std::mutex> lock(dbMutex);
        long long expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + expirySeconds;

        const char* invalidateSql = "UPDATE otp_codes SET is_used = 1 WHERE email = ? AND purpose = ?;";
        sqlite3_stmt* invStmt = nullptr;
        if (sqlite3_prepare_v2(db, invalidateSql, -1, &invStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(invStmt, 1, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(invStmt, 2, purpose.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(invStmt);
            sqlite3_finalize(invStmt);
        }

        const char* insertSql = "INSERT INTO otp_codes (email, code, purpose, expires_at, is_used) VALUES (?, ?, ?, ?, 0);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, purpose.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, expiresAt);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool verifyOtp(const std::string& email, const std::string& code, const std::string& purpose) {
        std::lock_guard<std::mutex> lock(dbMutex);
        long long now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        const char* sql = "SELECT id FROM otp_codes WHERE email = ? AND code = ? AND purpose = ? AND is_used = 0 AND expires_at > ? ORDER BY id DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, purpose.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, now);

        int otpId = 0;
        bool valid = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            otpId = sqlite3_column_int(stmt, 0);
            valid = true;
        }
        sqlite3_finalize(stmt);

        if (valid && otpId > 0) {
            const char* updateSql = "UPDATE otp_codes SET is_used = 1 WHERE id = ?;";
            sqlite3_stmt* uStmt = nullptr;
            if (sqlite3_prepare_v2(db, updateSql, -1, &uStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(uStmt, 1, otpId);
                sqlite3_step(uStmt);
                sqlite3_finalize(uStmt);
            }
        }
        return valid;
    }

    // --- FRIENDSHIP & REQUESTS ---
    std::string getFriendshipRelation(const std::string& myId, const std::string& targetId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* fSql = "SELECT status FROM friendships WHERE user_id_1 = ? AND user_id_2 = ?;";
        sqlite3_stmt* fStmt = nullptr;
        if (sqlite3_prepare_v2(db, fSql, -1, &fStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(fStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(fStmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(fStmt) == SQLITE_ROW) {
                std::string st = (const char*)sqlite3_column_text(fStmt, 0);
                sqlite3_finalize(fStmt);
                return st;
            }
            sqlite3_finalize(fStmt);
        }

        const char* rSql = "SELECT requester_id, receiver_id FROM friend_requests WHERE ((requester_id = ? AND receiver_id = ?) OR (requester_id = ? AND receiver_id = ?)) AND status = 'PENDING';";
        sqlite3_stmt* rStmt = nullptr;
        if (sqlite3_prepare_v2(db, rSql, -1, &rStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(rStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(rStmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(rStmt, 3, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(rStmt, 4, myId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(rStmt) == SQLITE_ROW) {
                std::string req = (const char*)sqlite3_column_text(rStmt, 0);
                sqlite3_finalize(rStmt);
                if (req == myId) return "PENDING_SENT";
                else return "PENDING_RECEIVED";
            }
            sqlite3_finalize(rStmt);
        }
        return "NONE";
    }

    bool sendFriendRequest(const std::string& requesterId, const std::string& receiverId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        const char* sql = "INSERT OR REPLACE INTO friend_requests (requester_id, receiver_id, status, created_at) VALUES (?, ?, 'PENDING', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, requesterId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, receiverId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Thêm thông báo cho receiver
        addNotificationInternal(receiverId, requesterId, "FRIEND_REQUEST", "đã gửi cho bạn một lời mời kết bạn.", "/friends.html");
        return (rc == SQLITE_DONE);
    }

    bool respondFriendRequest(const std::string& receiverId, const std::string& requesterId, const std::string& action) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();

        if (action == "ACCEPT") {
            const char* delSql = "DELETE FROM friend_requests WHERE requester_id = ? AND receiver_id = ?;";
            sqlite3_stmt* dStmt = nullptr;
            if (sqlite3_prepare_v2(db, delSql, -1, &dStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(dStmt, 1, requesterId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(dStmt, 2, receiverId.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(dStmt);
                sqlite3_finalize(dStmt);
            }

            const char* inSql = "INSERT OR REPLACE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES (?, ?, 'FRIEND', ?);";
            sqlite3_stmt* s1 = nullptr;
            if (sqlite3_prepare_v2(db, inSql, -1, &s1, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s1, 1, requesterId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(s1, 2, receiverId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(s1, 3, timeStr.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(s1);
                sqlite3_finalize(s1);
            }
            sqlite3_stmt* s2 = nullptr;
            if (sqlite3_prepare_v2(db, inSql, -1, &s2, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s2, 1, receiverId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(s2, 2, requesterId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(s2, 3, timeStr.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(s2);
                sqlite3_finalize(s2);
            }

            addNotificationInternal(requesterId, receiverId, "FRIEND_ACCEPT", "đã chấp nhận lời mời kết bạn của bạn.", "/profile.html?id=" + receiverId);
            return true;
        } else {
            const char* delSql = "DELETE FROM friend_requests WHERE requester_id = ? AND receiver_id = ?;";
            sqlite3_stmt* dStmt = nullptr;
            if (sqlite3_prepare_v2(db, delSql, -1, &dStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(dStmt, 1, requesterId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(dStmt, 2, receiverId.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(dStmt);
                sqlite3_finalize(dStmt);
            }
            return true;
        }
    }

    std::vector<User> getPendingRequests(const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<User> list;
        const char* sql = R"(
            SELECT u.id, u.username, u.email, u.fullname, u.avatar, u.bio, u.hometown, u.school, u.workplace, u.interests, r.created_at
            FROM friend_requests r
            JOIN users u ON r.requester_id = u.id
            WHERE r.receiver_id = ? AND r.status = 'PENDING';
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            User u;
            u.id = (const char*)sqlite3_column_text(stmt, 0);
            u.username = (const char*)sqlite3_column_text(stmt, 1);
            u.email = (const char*)sqlite3_column_text(stmt, 2);
            u.fullname = (const char*)sqlite3_column_text(stmt, 3);
            u.avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            u.bio = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            u.hometown = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            u.school = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            u.workplace = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            u.interests = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            u.created_at = (const char*)sqlite3_column_text(stmt, 10);
            list.push_back(u);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    std::string resolveUserId(const std::string& inputIdOrToken) {
        if (inputIdOrToken.rfind("ACC", 0) == 0) return inputIdOrToken;
        const char* sql = "SELECT id FROM users WHERE user_token = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        std::string realId = inputIdOrToken;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, inputIdOrToken.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                realId = (const char*)sqlite3_column_text(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        return realId;
    }

    std::vector<std::string> getFriendsList(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string realId = resolveUserId(userId);
        std::vector<std::string> friends;
        const char* sql = "SELECT user_id_2 FROM friendships WHERE user_id_1 = ? AND status = 'FRIEND';";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return friends;

        sqlite3_bind_text(stmt, 1, realId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            friends.push_back((const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
        return friends;
    }

    std::vector<User> getFriendsFull(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string realId = resolveUserId(userId);
        std::vector<User> list;
        const char* sql = R"(
            SELECT u.id, u.username, u.email, u.fullname, u.avatar, u.bio, u.hometown, u.school, u.workplace, u.interests, u.role, u.status, u.last_active_at, u.created_at, u.gender, u.birthday, u.residence, u.user_token
            FROM friendships f
            JOIN users u ON f.user_id_2 = u.id
            WHERE f.user_id_1 = ? AND f.status = 'FRIEND';
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, realId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            User u;
            u.id = (const char*)sqlite3_column_text(stmt, 0);
            u.username = (const char*)sqlite3_column_text(stmt, 1);
            u.email = (const char*)sqlite3_column_text(stmt, 2);
            u.fullname = (const char*)sqlite3_column_text(stmt, 3);
            u.avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            u.bio = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            u.hometown = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            u.school = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            u.workplace = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            u.interests = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            u.role = (const char*)sqlite3_column_text(stmt, 10);
            u.status = (const char*)sqlite3_column_text(stmt, 11);
            u.last_active_at = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            u.created_at = (const char*)sqlite3_column_text(stmt, 13);
            u.gender = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            u.birthday = sqlite3_column_text(stmt, 15) ? (const char*)sqlite3_column_text(stmt, 15) : "";
            u.residence = sqlite3_column_text(stmt, 16) ? (const char*)sqlite3_column_text(stmt, 16) : "";
            u.user_token = sqlite3_column_text(stmt, 17) ? (const char*)sqlite3_column_text(stmt, 17) : "";
            list.push_back(u);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    bool removeFriend(const std::string& myId, const std::string& friendId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM friendships WHERE (user_id_1 = ? AND user_id_2 = ?) OR (user_id_1 = ? AND user_id_2 = ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, friendId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, friendId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, myId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool cancelFriendRequest(const std::string& requesterId, const std::string& receiverId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM friend_requests WHERE requester_id = ? AND receiver_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, requesterId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, receiverId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool blockUser(const std::string& myId, const std::string& targetId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        const char* delSql = "DELETE FROM friendships WHERE (user_id_1 = ? AND user_id_2 = ?) OR (user_id_1 = ? AND user_id_2 = ?);";
        sqlite3_stmt* dStmt = nullptr;
        if (sqlite3_prepare_v2(db, delSql, -1, &dStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(dStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(dStmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(dStmt, 3, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(dStmt, 4, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(dStmt);
            sqlite3_finalize(dStmt);
        }

        const char* inSql = "INSERT OR REPLACE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES (?, ?, 'BLOCKED', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, inSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        return true;
    }

    // --- POSTS & TOP REACTIONS (Hình 1) ---
    static std::string getReactionIcon(const std::string& type) {
        if (type == "LOVE") return "❤️";
        if (type == "LIKE") return "👍";
        if (type == "HAHA") return "😆";
        if (type == "WOW") return "😮";
        if (type == "SAD") return "😢";
        if (type == "ANGRY") return "😡";
        return "👍";
    }

    void populatePostExtras(Post& p, const std::string& currentUserId) {
        // 1. Tính toán Top Reactions và danh sách tên người thả (Hình 1)
        const char* rSql = R"(
            SELECT r.type, u.fullname
            FROM reactions r
            JOIN users u ON r.user_id = u.id
            WHERE r.post_id = ?;
        )";
        sqlite3_stmt* rStmt = nullptr;
        std::map<std::string, std::vector<std::string>> reactMap;
        if (sqlite3_prepare_v2(db, rSql, -1, &rStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(rStmt, 1, p.id.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(rStmt) == SQLITE_ROW) {
                std::string t = (const char*)sqlite3_column_text(rStmt, 0);
                std::string name = (const char*)sqlite3_column_text(rStmt, 1);
                reactMap[t].push_back(name);
                p.total_reactions++;
            }
            sqlite3_finalize(rStmt);
        }

        // Chuyển sang vector để sắp xếp giảm dần theo số lượng
        std::vector<TopReaction> trList;
        for (const auto& pair : reactMap) {
            TopReaction tr;
            tr.type = pair.first;
            tr.icon = getReactionIcon(pair.first);
            tr.count = (int)pair.second.size();
            tr.user_names = pair.second;
            trList.push_back(tr);
        }
        std::sort(trList.begin(), trList.end(), [](const TopReaction& a, const TopReaction& b) {
            return a.count > b.count;
        });

        // Chỉ lấy tối đa 3 icon nhiều nhất (Hình 1)
        if (trList.size() > 3) trList.resize(3);
        p.top_reactions = trList;

        // 2. Nạp Comments và Nested Replies (Hình 2 & Hình 5)
        const char* cSql = R"(
            SELECT c.id, c.post_id, c.author_id,
                   COALESCE((SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = c.author_id AND nickname != ''), u.fullname) as author_name,
                   u.avatar, c.content, c.reply_to_id, c.reply_to_author, c.created_at
            FROM comments c
            JOIN users u ON c.author_id = u.id
            WHERE c.post_id = ?
            ORDER BY c.created_at ASC;
        )";
        sqlite3_stmt* cStmt = nullptr;
        std::vector<Comment> allComments;
        if (sqlite3_prepare_v2(db, cSql, -1, &cStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(cStmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(cStmt, 2, p.id.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(cStmt) == SQLITE_ROW) {
                Comment c;
                c.id = (const char*)sqlite3_column_text(cStmt, 0);
                c.post_id = (const char*)sqlite3_column_text(cStmt, 1);
                c.author_id = (const char*)sqlite3_column_text(cStmt, 2);
                c.author_name = (const char*)sqlite3_column_text(cStmt, 3);
                c.author_avatar = sqlite3_column_text(cStmt, 4) ? (const char*)sqlite3_column_text(cStmt, 4) : "";
                c.content = (const char*)sqlite3_column_text(cStmt, 5);
                c.reply_to_id = sqlite3_column_text(cStmt, 6) ? (const char*)sqlite3_column_text(cStmt, 6) : "";
                c.reply_to_author = sqlite3_column_text(cStmt, 7) ? (const char*)sqlite3_column_text(cStmt, 7) : "";
                c.created_at = (const char*)sqlite3_column_text(cStmt, 8);
                allComments.push_back(c);
            }
            sqlite3_finalize(cStmt);
        }

        // Cấu trúc phân nhánh cha - con cho replies
        std::map<std::string, Comment*> commentMap;
        std::vector<Comment> rootComments;

        for (auto& c : allComments) {
            if (c.reply_to_id.empty()) {
                rootComments.push_back(c);
            }
        }
        for (auto& rc : rootComments) {
            commentMap[rc.id] = &rc;
        }
        for (const auto& c : allComments) {
            if (!c.reply_to_id.empty()) {
                if (commentMap.find(c.reply_to_id) != commentMap.end()) {
                    commentMap[c.reply_to_id]->replies.push_back(c);
                } else {
                    rootComments.push_back(c);
                }
            }
        }
        p.comments = rootComments;

        // 3. Nếu là bài đăng lại (shared_post_id != ""), nạp thông tin bài viết gốc
        if (!p.shared_post_id.empty()) {
            const char* oSql = R"(
                SELECT p.id, p.author_id,
                       COALESCE((SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = p.author_id AND nickname != ''), u.fullname) as author_name,
                       u.avatar, p.content, p.media_type, p.media_url, p.created_at, p.title, p.visibility, u.user_token,
                       (SELECT COUNT(*) FROM reactions r WHERE r.post_id = p.id) as react_count,
                       (SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) as comment_count
                FROM posts p
                JOIN users u ON p.author_id = u.id
                WHERE p.id = ?;
            )";
            sqlite3_stmt* oStmt = nullptr;
            if (sqlite3_prepare_v2(db, oSql, -1, &oStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(oStmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(oStmt, 2, p.shared_post_id.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(oStmt) == SQLITE_ROW) {
                    p.has_original_post = true;
                    p.original_post.id = (const char*)sqlite3_column_text(oStmt, 0);
                    p.original_post.author_id = (const char*)sqlite3_column_text(oStmt, 1);
                    p.original_post.author_name = (const char*)sqlite3_column_text(oStmt, 2);
                    p.original_post.author_avatar = sqlite3_column_text(oStmt, 3) ? (const char*)sqlite3_column_text(oStmt, 3) : "";
                    p.original_post.content = (const char*)sqlite3_column_text(oStmt, 4);
                    p.original_post.media_type = sqlite3_column_text(oStmt, 5) ? (const char*)sqlite3_column_text(oStmt, 5) : "NONE";
                    p.original_post.media_url = sqlite3_column_text(oStmt, 6) ? (const char*)sqlite3_column_text(oStmt, 6) : "";
                    p.original_post.created_at = (const char*)sqlite3_column_text(oStmt, 7);
                    p.original_post.title = sqlite3_column_text(oStmt, 8) ? (const char*)sqlite3_column_text(oStmt, 8) : "";
                    p.original_post.visibility = sqlite3_column_text(oStmt, 9) ? (const char*)sqlite3_column_text(oStmt, 9) : "PUBLIC";
                    p.original_post.author_token = sqlite3_column_text(oStmt, 10) ? (const char*)sqlite3_column_text(oStmt, 10) : "";
                    p.original_post.total_reactions = sqlite3_column_int(oStmt, 11);
                    p.original_post.comments_count = sqlite3_column_int(oStmt, 12);
                } else {
                    p.has_original_post = false;
                }
                sqlite3_finalize(oStmt);
            }
        }
    }

    json getPostReactionsDetail(const std::string& postId, const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json result;
        json counts = {{"ALL", 0}, {"LIKE", 0}, {"LOVE", 0}, {"HAHA", 0}, {"WOW", 0}, {"SAD", 0}, {"ANGRY", 0}};
        json users = json::array();

        const char* sql = R"(
            SELECT r.type, u.id, u.fullname, u.avatar, u.user_token
            FROM reactions r
            JOIN users u ON r.user_id = u.id
            WHERE r.post_id = ?
            ORDER BY r.created_at DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string type = (const char*)sqlite3_column_text(stmt, 0);
                std::string uid = (const char*)sqlite3_column_text(stmt, 1);
                std::string name = (const char*)sqlite3_column_text(stmt, 2);
                std::string avatar = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "/img/default-avatar.svg";
                std::string token = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : uid;

                counts["ALL"] = counts["ALL"].get<int>() + 1;
                if (counts.contains(type)) counts[type] = counts[type].get<int>() + 1;

                std::string rel = "NONE";
                if (!myId.empty()) {
                    if (myId == uid) rel = "ME";
                    else {
                        const char* fSql = "SELECT status FROM friendships WHERE user_id_1 = ? AND user_id_2 = ?;";
                        sqlite3_stmt* fStmt = nullptr;
                        if (sqlite3_prepare_v2(db, fSql, -1, &fStmt, nullptr) == SQLITE_OK) {
                            sqlite3_bind_text(fStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
                            sqlite3_bind_text(fStmt, 2, uid.c_str(), -1, SQLITE_STATIC);
                            if (sqlite3_step(fStmt) == SQLITE_ROW) rel = "FRIEND";
                            sqlite3_finalize(fStmt);
                        }
                    }
                }

                users.push_back({
                    {"type", type},
                    {"icon", getReactionIcon(type)},
                    {"user_id", uid},
                    {"name", name},
                    {"avatar", avatar},
                    {"user_token", token},
                    {"relation", rel}
                });
            }
            sqlite3_finalize(stmt);
        }
        result["status"] = "ok";
        result["counts"] = counts;
        result["users"] = users;
        result["total"] = counts["ALL"];
        return result;
    }

    /**
     * News Feed Ranking:
     * score = wV*viral + wI*interest + wL*location + wS*school + wR*recency
     * - Dampen viral (log) để bài hot không chiếm hết bảng tin
     * - Diversity: không cho quá nhiều bài liên tiếp cùng 1 tác giả
     * - Ẩn bài moderation_action = BLOCK
     */
    std::vector<Post> getFeedPosts(const std::string& currentUserId, int limit = 20, int offset = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<Post> candidates;

        // Lấy profile viewer để so khớp trường / nơi ở / sở thích
        std::string mySchool, myResidence, myHometown, myInterests;
        {
            const char* uSql = "SELECT school, residence, hometown, interests FROM users WHERE id = ?;";
            sqlite3_stmt* uStmt = nullptr;
            if (sqlite3_prepare_v2(db, uSql, -1, &uStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(uStmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(uStmt) == SQLITE_ROW) {
                    mySchool = sqlite3_column_text(uStmt, 0) ? (const char*)sqlite3_column_text(uStmt, 0) : "";
                    myResidence = sqlite3_column_text(uStmt, 1) ? (const char*)sqlite3_column_text(uStmt, 1) : "";
                    myHometown = sqlite3_column_text(uStmt, 2) ? (const char*)sqlite3_column_text(uStmt, 2) : "";
                    myInterests = sqlite3_column_text(uStmt, 3) ? (const char*)sqlite3_column_text(uStmt, 3) : "";
                }
                sqlite3_finalize(uStmt);
            }
        }

        // Sở thích chủ đề dựa trên tương tác gần đây của user
        std::map<std::string, int> categoryAffinity;
        {
            const char* aSql = R"(
                SELECT COALESCE(p.category, 'doi_song') as cat, COUNT(*) as cnt
                FROM reactions r JOIN posts p ON r.post_id = p.id
                WHERE r.user_id = ?
                GROUP BY cat
                UNION ALL
                SELECT COALESCE(p.category, 'doi_song'), COUNT(*)
                FROM comments c JOIN posts p ON c.post_id = p.id
                WHERE c.author_id = ?
                GROUP BY COALESCE(p.category, 'doi_song');
            )";
            sqlite3_stmt* aStmt = nullptr;
            if (sqlite3_prepare_v2(db, aSql, -1, &aStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(aStmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(aStmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
                while (sqlite3_step(aStmt) == SQLITE_ROW) {
                    std::string cat = sqlite3_column_text(aStmt, 0) ? (const char*)sqlite3_column_text(aStmt, 0) : "doi_song";
                    categoryAffinity[cat] += sqlite3_column_int(aStmt, 1);
                }
                sqlite3_finalize(aStmt);
            }
        }
        int maxAffinity = 1;
        for (auto& kv : categoryAffinity) if (kv.second > maxAffinity) maxAffinity = kv.second;

        // Ứng viên: lấy nhiều hơn limit rồi rank (ví dụ 120 bài gần nhất)
        const char* sql = R"(
            SELECT p.id, p.author_id,
                   COALESCE((SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = p.author_id AND nickname != ''), u.fullname) as author_name,
                   u.avatar, p.content, p.media_type, p.media_url,
                   p.shared_post_id, p.shared_author_name, p.shared_content, p.created_at,
                   (SELECT type FROM reactions r WHERE r.post_id = p.id AND r.user_id = ?) as my_reaction,
                   p.title, p.visibility, u.user_token,
                   COALESCE(p.category, 'doi_song'), COALESCE(p.category_label, 'Đời sống'),
                   COALESCE(p.moderation_status, 'SAFE'), COALESCE(p.moderation_action, 'ALLOW'),
                   COALESCE(p.moderation_reason, ''),
                   u.school, u.residence, u.hometown,
                   (SELECT COUNT(*) FROM reactions rx WHERE rx.post_id = p.id) as react_cnt,
                   (SELECT COUNT(*) FROM comments cm WHERE cm.post_id = p.id) as cmt_cnt,
                   (SELECT COUNT(*) FROM posts sp WHERE sp.shared_post_id = p.id) as share_cnt
            FROM posts p
            JOIN users u ON p.author_id = u.id
            WHERE u.status = 'ACTIVE'
              AND COALESCE(p.moderation_action, 'ALLOW') != 'BLOCK'
              AND (
                    p.visibility = 'PUBLIC'
                 OR p.author_id = ?
                 OR (p.visibility = 'FRIENDS' AND EXISTS (
                        SELECT 1 FROM friendships f
                        WHERE f.status = 'FRIEND'
                          AND ((f.user_id_1 = ? AND f.user_id_2 = p.author_id) OR (f.user_id_2 = ? AND f.user_id_1 = p.author_id))
                    ))
              )
            ORDER BY p.created_at DESC
            LIMIT 120;
        )";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return candidates;

        sqlite3_bind_text(stmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, currentUserId.c_str(), -1, SQLITE_STATIC);

        auto now = std::chrono::system_clock::now();
        auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        auto parseTimeSec = [](const std::string& tstr) -> long long {
            if (tstr.size() < 19) return 0;
            std::tm t = {};
            t.tm_isdst = -1;
            std::istringstream ss(tstr);
            ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
            if (ss.fail()) return 0;
            return (long long)std::mktime(&t);
        };

        auto normEq = [](std::string a, std::string b) {
            auto low = [](std::string& s) {
                for (auto& c : s) c = (char)std::tolower((unsigned char)c);
            };
            low(a); low(b);
            if (a.empty() || b.empty()) return false;
            return a == b || a.find(b) != std::string::npos || b.find(a) != std::string::npos;
        };

        // Trọng số
        const double W_VIRAL = 0.28;
        const double W_INTEREST = 0.22;
        const double W_LOCATION = 0.15;
        const double W_SCHOOL = 0.15;
        const double W_RECENCY = 0.20;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Post p;
            p.id = (const char*)sqlite3_column_text(stmt, 0);
            p.author_id = (const char*)sqlite3_column_text(stmt, 1);
            p.author_name = (const char*)sqlite3_column_text(stmt, 2);
            p.author_avatar = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
            p.content = (const char*)sqlite3_column_text(stmt, 4);
            p.media_type = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "NONE";
            p.media_url = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            p.shared_post_id = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            p.shared_author_name = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            p.shared_content = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            p.created_at = (const char*)sqlite3_column_text(stmt, 10);
            p.my_reaction = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            p.title = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            p.visibility = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "PUBLIC";
            p.author_token = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            p.category = sqlite3_column_text(stmt, 15) ? (const char*)sqlite3_column_text(stmt, 15) : "doi_song";
            p.category_label = sqlite3_column_text(stmt, 16) ? (const char*)sqlite3_column_text(stmt, 16) : "Đời sống";
            p.moderation_status = sqlite3_column_text(stmt, 17) ? (const char*)sqlite3_column_text(stmt, 17) : "SAFE";
            p.moderation_action = sqlite3_column_text(stmt, 18) ? (const char*)sqlite3_column_text(stmt, 18) : "ALLOW";
            p.moderation_reason = sqlite3_column_text(stmt, 19) ? (const char*)sqlite3_column_text(stmt, 19) : "";
            p.author_school = sqlite3_column_text(stmt, 20) ? (const char*)sqlite3_column_text(stmt, 20) : "";
            p.author_residence = sqlite3_column_text(stmt, 21) ? (const char*)sqlite3_column_text(stmt, 21) : "";
            p.author_hometown = sqlite3_column_text(stmt, 22) ? (const char*)sqlite3_column_text(stmt, 22) : "";
            int reactCnt = sqlite3_column_int(stmt, 23);
            int cmtCnt = sqlite3_column_int(stmt, 24);
            int shareCnt = sqlite3_column_int(stmt, 25);
            p.total_reactions = reactCnt;
            p.comments_count_cached = cmtCnt;
            p.share_count = shareCnt;

            // --- Viral (log dampening) ---
            double viralRaw = reactCnt * 1.0 + cmtCnt * 2.0 + shareCnt * 3.0;
            double viralScore = std::log1p(viralRaw) / std::log1p(50.0); // chuẩn hóa ~[0,1+]
            if (viralScore > 1.5) viralScore = 1.5;

            // --- Interest (affinity chủ đề + interests text) ---
            double interestScore = 0.0;
            auto itAff = categoryAffinity.find(p.category);
            if (itAff != categoryAffinity.end()) {
                interestScore = (double)itAff->second / (double)maxAffinity;
            }
            if (!myInterests.empty() && !p.content.empty()) {
                std::string lowI = myInterests, lowC = p.content + " " + p.title;
                for (auto& c : lowI) c = (char)std::tolower((unsigned char)c);
                for (auto& c : lowC) c = (char)std::tolower((unsigned char)c);
                // tách thô theo dấu phẩy
                std::stringstream iss(lowI);
                std::string token;
                while (std::getline(iss, token, ',')) {
                    while (!token.empty() && token.front() == ' ') token.erase(token.begin());
                    if (token.size() >= 2 && lowC.find(token) != std::string::npos) {
                        interestScore = std::min(1.0, interestScore + 0.25);
                    }
                }
            }

            // --- Location ---
            double locScore = 0.0;
            if (normEq(myResidence, p.author_residence) || normEq(myResidence, p.author_hometown)
                || normEq(myHometown, p.author_residence) || normEq(myHometown, p.author_hometown)) {
                locScore = 1.0;
            } else if (!myResidence.empty() && (p.content.find(myResidence) != std::string::npos || p.title.find(myResidence) != std::string::npos)) {
                locScore = 0.5;
            }

            // --- School ---
            double schoolScore = normEq(mySchool, p.author_school) ? 1.0 : 0.0;

            // --- Recency (half-life ~36h) ---
            long long createdSec = parseTimeSec(p.created_at);
            double ageHours = (createdSec > 0) ? (double)(nowSec - createdSec) / 3600.0 : 72.0;
            if (ageHours < 0) ageHours = 0;
            double recencyScore = std::exp(-ageHours / 36.0);

            // FLAGGED giảm nhẹ điểm
            double penalty = (p.moderation_action == "FLAG") ? 0.85 : 1.0;

            p.feed_score = penalty * (
                W_VIRAL * viralScore +
                W_INTEREST * interestScore +
                W_LOCATION * locScore +
                W_SCHOOL * schoolScore +
                W_RECENCY * recencyScore
            );

            candidates.push_back(p);
        }
        sqlite3_finalize(stmt);

        std::sort(candidates.begin(), candidates.end(), [](const Post& a, const Post& b) {
            if (std::abs(a.feed_score - b.feed_score) > 1e-9) return a.feed_score > b.feed_score;
            return a.created_at > b.created_at;
        });

        // Diversity: không quá 2 bài liên tiếp cùng author; tối đa ~3 bài/author trong 1 page
        std::vector<Post> ranked;
        std::map<std::string, int> authorCount;
        std::string lastAuthor;
        int lastStreak = 0;
        for (auto& p : candidates) {
            if ((int)ranked.size() >= offset + limit) break;
            if (authorCount[p.author_id] >= 3) continue;
            if (p.author_id == lastAuthor && lastStreak >= 2) continue;
            ranked.push_back(p);
            authorCount[p.author_id]++;
            if (p.author_id == lastAuthor) lastStreak++;
            else { lastAuthor = p.author_id; lastStreak = 1; }
        }

        // Phân trang sau rank
        std::vector<Post> page;
        for (int i = offset; i < (int)ranked.size() && (int)page.size() < limit; ++i) {
            page.push_back(ranked[i]);
        }

        for (auto& p : page) {
            populatePostExtras(p, currentUserId);
        }
        return page;
    }

    std::vector<Post> getUserPosts(const std::string& authorId, const std::string& currentUserId, int limit = 5, int offset = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string realAuthorId = resolveUserId(authorId);
        std::vector<Post> posts;

        const char* sql = R"(
            SELECT p.id, p.author_id,
                   COALESCE((SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = p.author_id AND nickname != ''), u.fullname) as author_name,
                   u.avatar, p.content, p.media_type, p.media_url,
                   p.shared_post_id, p.shared_author_name, p.shared_content, p.created_at,
                   (SELECT type FROM reactions r WHERE r.post_id = p.id AND r.user_id = ?) as my_reaction,
                   p.title, p.visibility, u.user_token
            FROM posts p
            JOIN users u ON p.author_id = u.id
            WHERE p.author_id = ?
            ORDER BY p.created_at DESC
            LIMIT ? OFFSET ?;
        )";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return posts;

        sqlite3_bind_text(stmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, realAuthorId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, limit);
        sqlite3_bind_int(stmt, 5, offset);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Post p;
            p.id = (const char*)sqlite3_column_text(stmt, 0);
            p.author_id = (const char*)sqlite3_column_text(stmt, 1);
            p.author_name = (const char*)sqlite3_column_text(stmt, 2);
            p.author_avatar = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
            p.content = (const char*)sqlite3_column_text(stmt, 4);
            p.media_type = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "NONE";
            p.media_url = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            p.shared_post_id = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            p.shared_author_name = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            p.shared_content = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            p.created_at = (const char*)sqlite3_column_text(stmt, 10);
            p.my_reaction = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            p.title = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            p.visibility = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "PUBLIC";
            p.author_token = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            posts.push_back(p);
        }
        sqlite3_finalize(stmt);

        for (auto& p : posts) {
            populatePostExtras(p, currentUserId);
        }
        return posts;
    }

    bool getPostById(const std::string& postId, const std::string& currentUserId, Post& p) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = R"(
            SELECT p.id, p.author_id,
                   COALESCE((SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = p.author_id AND nickname != ''), u.fullname) as author_name,
                   u.avatar, p.content, p.media_type, p.media_url,
                   p.shared_post_id, p.shared_author_name, p.shared_content, p.created_at,
                   (SELECT type FROM reactions r WHERE r.post_id = p.id AND r.user_id = ?) as my_reaction,
                   p.title, p.visibility, u.user_token
            FROM posts p
            JOIN users u ON p.author_id = u.id
            WHERE p.id = ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, postId.c_str(), -1, SQLITE_STATIC);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            p.id = (const char*)sqlite3_column_text(stmt, 0);
            p.author_id = (const char*)sqlite3_column_text(stmt, 1);
            p.author_name = (const char*)sqlite3_column_text(stmt, 2);
            p.author_avatar = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
            p.content = (const char*)sqlite3_column_text(stmt, 4);
            p.media_type = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "NONE";
            p.media_url = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            p.shared_post_id = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            p.shared_author_name = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            p.shared_content = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            p.created_at = (const char*)sqlite3_column_text(stmt, 10);
            p.my_reaction = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            p.title = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            p.visibility = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "PUBLIC";
            p.author_token = sqlite3_column_text(stmt, 14) ? (const char*)sqlite3_column_text(stmt, 14) : "";
            found = true;
        }
        sqlite3_finalize(stmt);

        if (found) {
            populatePostExtras(p, currentUserId);
        }
        return found;
    }

    bool createPostWithMedia(const std::string& authorId, const std::string& content,
                             const std::string& mediaType = "NONE", const std::string& mediaUrl = "",
                             const std::string& sharedPostId = "", const std::string& sharedAuthorName = "",
                             const std::string& sharedContent = "",
                             const std::string& title = "", const std::string& visibility = "PUBLIC",
                             const std::string& category = "doi_song",
                             const std::string& categoryLabel = "Đời sống",
                             const std::string& modStatus = "SAFE",
                             const std::string& modAction = "ALLOW",
                             const std::string& modReason = "",
                             std::string* outPostId = nullptr) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string postId = "POST" + std::to_string(nowMs);

        const char* sql = "INSERT INTO posts (id, author_id, content, media_type, media_url, shared_post_id, shared_author_name, shared_content, created_at, title, visibility, category, category_label, moderation_status, moderation_action, moderation_reason) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, authorId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, mediaType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, mediaUrl.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, sharedPostId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, sharedAuthorName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, sharedContent.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, timeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, title.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, visibility.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 12, category.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 13, categoryLabel.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 14, modStatus.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 15, modAction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 16, modReason.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE && outPostId) *outPostId = postId;
        return (rc == SQLITE_DONE);
    }

    bool setReaction(const std::string& postId, const std::string& userId, const std::string& reactionType) {
        std::lock_guard<std::mutex> lock(dbMutex);
        if (reactionType.empty() || reactionType == "NONE") {
            const char* delSql = "DELETE FROM reactions WHERE post_id = ? AND user_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, delSql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
            return true;
        }

        std::string timeStr = getCurrentTimeStr();
        const char* inSql = "INSERT OR REPLACE INTO reactions (post_id, user_id, type, created_at) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, inSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, reactionType.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, timeStr.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            // Gửi thông báo cho chủ bài viết
            const char* aSql = "SELECT author_id FROM posts WHERE id = ?;";
            sqlite3_stmt* aStmt = nullptr;
            if (sqlite3_prepare_v2(db, aSql, -1, &aStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(aStmt, 1, postId.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(aStmt) == SQLITE_ROW) {
                    std::string authorId = (const char*)sqlite3_column_text(aStmt, 0);
                    if (authorId != userId) {
                        addNotificationInternal(authorId, userId, "LIKE", "đã bày tỏ cảm xúc về bài viết của bạn.", "/#post-" + postId);
                    }
                }
                sqlite3_finalize(aStmt);
            }
            return true;
        }
        return false;
    }

    bool addComment(const std::string& postId, const std::string& authorId, const std::string& content,
                    const std::string& replyToId, const std::string& replyToAuthor, Comment& outComment) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string commentId = "CMT" + std::to_string(nowMs);

        const char* sql = "INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, commentId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, authorId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, replyToId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, replyToAuthor.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, timeStr.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_DONE) {
            outComment.id = commentId;
            outComment.post_id = postId;
            outComment.author_id = authorId;
            outComment.content = content;
            outComment.reply_to_id = replyToId;
            outComment.reply_to_author = replyToAuthor;
            outComment.created_at = timeStr;

            // Bắn thông báo
            const char* aSql = "SELECT author_id FROM posts WHERE id = ?;";
            sqlite3_stmt* aStmt = nullptr;
            if (sqlite3_prepare_v2(db, aSql, -1, &aStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(aStmt, 1, postId.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(aStmt) == SQLITE_ROW) {
                    std::string authorIdPost = (const char*)sqlite3_column_text(aStmt, 0);
                    if (authorIdPost != authorId) {
                        addNotificationInternal(authorIdPost, authorId, "COMMENT", "đã bình luận về bài viết của bạn.", "/#post-" + postId);
                    }
                }
                sqlite3_finalize(aStmt);
            }
            return true;
        }
        return false;
    }

    bool deleteComment(const std::string& commentId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // Kiểm tra xem userId có phải người viết comment hoặc tác giả bài đăng không
        const char* checkSql = R"(
            SELECT c.author_id, p.author_id
            FROM comments c
            JOIN posts p ON c.post_id = p.id
            WHERE c.id = ?;
        )";
        sqlite3_stmt* cStmt = nullptr;
        bool canDelete = false;
        if (sqlite3_prepare_v2(db, checkSql, -1, &cStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(cStmt, 1, commentId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(cStmt) == SQLITE_ROW) {
                std::string commentAuthor = (const char*)sqlite3_column_text(cStmt, 0);
                std::string postAuthor = (const char*)sqlite3_column_text(cStmt, 1);
                if (userId == commentAuthor || userId == postAuthor) {
                    canDelete = true;
                }
            }
            sqlite3_finalize(cStmt);
        }

        if (!canDelete) return false;

        const char* delSql = "DELETE FROM comments WHERE id = ? OR reply_to_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, delSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, commentId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, commentId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    // --- CHAT WITH 30 MESSAGES & GROUP CHAT ---
    static std::string getConversationId(const std::string& u1, const std::string& u2) {
        if (u1 < u2) return u1 + "_" + u2;
        return u2 + "_" + u1;
    }

    std::vector<Message> getPaginatedMessages(const std::string& user1, const std::string& user2, int beforeId, int limit, bool& hasMore) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(user1, user2);
        std::vector<Message> messages;
        hasMore = false;

        const char* readSql = "UPDATE messages SET is_read = 1 WHERE conversation_id = ? AND recipient_id = ?;";
        sqlite3_stmt* rdStmt = nullptr;
        if (sqlite3_prepare_v2(db, readSql, -1, &rdStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(rdStmt, 1, convId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(rdStmt, 2, user1.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(rdStmt);
            sqlite3_finalize(rdStmt);
        }

        std::string sql;
        if (beforeId > 0) {
            sql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                  "FROM messages m JOIN users u ON m.sender_id = u.id "
                  "WHERE m.conversation_id = ? AND m.id < ? ORDER BY m.id DESC LIMIT ?;";
        } else {
            sql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                  "FROM messages m JOIN users u ON m.sender_id = u.id "
                  "WHERE m.conversation_id = ? ORDER BY m.id DESC LIMIT ?;";
        }

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return messages;

        sqlite3_bind_text(stmt, 1, convId.c_str(), -1, SQLITE_STATIC);
        if (beforeId > 0) {
            sqlite3_bind_int(stmt, 2, beforeId);
            sqlite3_bind_int(stmt, 3, limit);
        } else {
            sqlite3_bind_int(stmt, 2, limit);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Message m;
            m.id = sqlite3_column_int(stmt, 0);
            m.conversation_id = (const char*)sqlite3_column_text(stmt, 1);
            m.sender_id = (const char*)sqlite3_column_text(stmt, 2);
            m.sender_name = (const char*)sqlite3_column_text(stmt, 3);
            m.sender_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            m.recipient_id = (const char*)sqlite3_column_text(stmt, 5);
            m.content = (const char*)sqlite3_column_text(stmt, 6);
            m.shared_post_id = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            m.is_revoked = sqlite3_column_int(stmt, 8);
            m.created_at = (const char*)sqlite3_column_text(stmt, 9);
            m.media_type = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "NONE";
            m.media_url = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            m.reaction = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            m.reply_to_text = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
            m.is_read = sqlite3_column_int(stmt, 14);
            m.reply_to_id = sqlite3_column_int(stmt, 15);
            messages.push_back(m);
        }
        sqlite3_finalize(stmt);

        if (!messages.empty()) {
            int oldestId = messages.back().id;
            const char* countSql = "SELECT COUNT(*) FROM messages WHERE conversation_id = ? AND id < ?;";
            sqlite3_stmt* countStmt = nullptr;
            if (sqlite3_prepare_v2(db, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(countStmt, 1, convId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(countStmt, 2, oldestId);
                if (sqlite3_step(countStmt) == SQLITE_ROW) {
                    hasMore = (sqlite3_column_int(countStmt, 0) > 0);
                }
                sqlite3_finalize(countStmt);
            }
        }

        std::vector<Message> chronological;
        for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
            chronological.push_back(*it);
        }
        return chronological;
    }

    bool sendMessage(const std::string& senderId, const std::string& recipientId, const std::string& content,
                     const std::string& sharedPostId, Message& outMsg,
                     const std::string& mediaType = "NONE", const std::string& mediaUrl = "",
                     const std::string& replyToText = "", int replyToId = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(senderId, recipientId);
        std::string timeStr = getCurrentTimeStr();

        const char* sql = "INSERT INTO messages (conversation_id, sender_id, recipient_id, content, shared_post_id, media_type, media_url, reply_to_text, is_read, created_at, reply_to_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, convId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, recipientId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, sharedPostId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, mediaType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, mediaUrl.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, replyToText.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, timeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, replyToId);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_DONE) {
            outMsg.id = (int)sqlite3_last_insert_rowid(db);
            outMsg.conversation_id = convId;
            outMsg.sender_id = senderId;
            outMsg.recipient_id = recipientId;
            outMsg.content = content;
            outMsg.shared_post_id = sharedPostId;
            outMsg.media_type = mediaType;
            outMsg.media_url = mediaUrl;
            outMsg.reply_to_text = replyToText;
            outMsg.reply_to_id = replyToId;
            outMsg.reaction = "";
            outMsg.is_revoked = 0;
            outMsg.created_at = timeStr;
            outMsg.reaction = "";
            outMsg.is_revoked = 0;
            outMsg.created_at = timeStr;
            return true;
        }
        return false;
    }

    bool sendMessage(const std::string& senderId, const std::string& recipientId, const std::string& content, Message& outMsg) {
        return sendMessage(senderId, recipientId, content, "", outMsg);
    }

    void setTypingStatus(const std::string& userId, const std::string& partnerId, bool isTyping) {
        std::lock_guard<std::mutex> lock(typingMutex);
        std::string key = userId + "_" + partnerId;
        typingMap[key] = { isTyping, std::chrono::steady_clock::now() };
    }

    bool getTypingStatus(const std::string& userId, const std::string& partnerId) {
        std::lock_guard<std::mutex> lock(typingMutex);
        std::string key = userId + "_" + partnerId;
        auto it = typingMap.find(key);
        if (it == typingMap.end()) return false;
        if (!it->second.isTyping) return false;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - it->second.lastUpdated
        ).count();
        if (elapsed > 4) {
            it->second.isTyping = false;
            return false;
        }
        return true;
    }

    bool getLastMessageReadStatus(const std::string& myId, const std::string& partnerId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(myId, partnerId);
        const char* sql = "SELECT is_read FROM messages WHERE conversation_id = ? AND sender_id = ? ORDER BY id DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, convId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, myId.c_str(), -1, SQLITE_STATIC);
        bool isRead = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            isRead = (sqlite3_column_int(stmt, 0) == 1);
        }
        sqlite3_finalize(stmt);
        return isRead;
    }

    bool recallMessage(int msgId, const std::string& senderId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // Kiểm tra thời gian gửi: chỉ cho phép thu hồi trong vòng 5 phút (300 giây)
        const char* checkSql = "SELECT created_at FROM messages WHERE id = ? AND sender_id = ?;";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, checkSql, -1, &cStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(cStmt, 1, msgId);
        sqlite3_bind_text(cStmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(cStmt) != SQLITE_ROW) {
            sqlite3_finalize(cStmt);
            return false;
        }
        std::string createdAt = (const char*)sqlite3_column_text(cStmt, 0);
        sqlite3_finalize(cStmt);

        auto now = std::chrono::system_clock::now();
        auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::tm t = {};
        t.tm_isdst = -1;
        std::istringstream ss(createdAt);
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            t.tm_isdst = -1;
            auto msgTime = std::mktime(&t);
            if (std::abs(nowSec - msgTime) > 300) {
                return false; // Quá 5 phút
            }
        }

        const char* sql = "UPDATE messages SET is_revoked = 1 WHERE id = ? AND sender_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    }

    bool deleteMessageForMe(int msgId, const std::string& userId) {
        // "Delete for me" - hide message from view by marking in a side table
        // For 1-on-1: simply delete if sender (or just soft mark)
        // We use a simple approach: if sender, can delete; if recipient, also can delete for self
        std::lock_guard<std::mutex> lock(dbMutex);
        // Check sender
        const char* checkSql = "SELECT sender_id, recipient_id FROM messages WHERE id = ?;";
        sqlite3_stmt* cStmt = nullptr;
        std::string senderId, recipientId;
        if (sqlite3_prepare_v2(db, checkSql, -1, &cStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(cStmt, 1, msgId);
            if (sqlite3_step(cStmt) == SQLITE_ROW) {
                senderId = (const char*)sqlite3_column_text(cStmt, 0);
                recipientId = (const char*)sqlite3_column_text(cStmt, 1);
            }
            sqlite3_finalize(cStmt);
        }
        if (senderId.empty()) return false;
        // We delete the row outright (simple approach - visible to both parties)
        const char* delSql = "DELETE FROM messages WHERE id = ?;";
        sqlite3_stmt* dStmt = nullptr;
        if (sqlite3_prepare_v2(db, delSql, -1, &dStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(dStmt, 1, msgId);
        int rc = sqlite3_step(dStmt);
        sqlite3_finalize(dStmt);
        return (rc == SQLITE_DONE);
    }

    bool setNickname(const std::string& userId, const std::string& friendId, const std::string& nickname) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "INSERT OR REPLACE INTO user_nicknames (user_id, friend_id, nickname) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, friendId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, nickname.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    std::string getNickname(const std::string& userId, const std::string& friendId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        std::string nick;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, friendId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                nick = (const char*)sqlite3_column_text(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        return nick;
    }

    bool clearConversation(const std::string& u1, const std::string& u2) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(u1, u2);
        const char* sql = "DELETE FROM messages WHERE conversation_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, convId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    json getFriendsWithLastMessage(const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();

        const char* sql = R"(
            SELECT u.id, u.fullname, u.avatar, u.last_active_at, u.user_token,
                   (SELECT CASE WHEN m.is_revoked = 1 THEN 'Tin nhắn đã được thu hồi' ELSE m.content END FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ? THEN u.id||'_'||? ELSE ?||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_msg,
                   (SELECT m.created_at FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ? THEN u.id||'_'||? ELSE ?||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_msg_time,
                   (SELECT COUNT(*) FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ? THEN u.id||'_'||? ELSE ?||'_'||u.id END) AND m.recipient_id = ? AND m.is_read = 0) as unread_count,
                   (SELECT nickname FROM user_nicknames WHERE user_id = ? AND friend_id = u.id) as nickname
            FROM friendships f
            JOIN users u ON f.user_id_2 = u.id
            WHERE f.user_id_1 = ? AND f.status = 'FRIEND' AND u.role != 'ADMIN'
            ORDER BY last_msg_time DESC NULLS LAST;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

        for (int i = 1; i <= 12; i++) {
            sqlite3_bind_text(stmt, i, myId.c_str(), -1, SQLITE_STATIC);
        }

        auto now = std::chrono::system_clock::now();
        auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string fid = (const char*)sqlite3_column_text(stmt, 0);
            std::string name = (const char*)sqlite3_column_text(stmt, 1);
            std::string avatar = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
            std::string lastActive = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
            std::string utoken = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : fid;
            std::string lastMsg = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "Chưa có tin nhắn";
            std::string lastMsgTime = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            bool isUnread = sqlite3_column_int(stmt, 7) > 0;
            std::string nickname = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";

            bool isOnline = false;
            if (!lastActive.empty()) {
                std::tm t = {};
                t.tm_isdst = -1;
                std::istringstream ss(lastActive);
                ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    t.tm_isdst = -1;
                    auto activeTime = std::mktime(&t);
                    if (std::abs(nowSec - activeTime) <= 300) isOnline = true; // 5 phút (300 giây)
                }
            }

            arr.push_back({
                {"id", fid},
                {"name", !nickname.empty() ? nickname : name},
                {"real_name", name},
                {"nickname", nickname},
                {"avatar", avatar.empty() ? "/img/default-avatar.svg" : avatar},
                {"user_token", utoken},
                {"is_online", isOnline},
                {"last_active_at", lastActive},
                {"last_message", lastMsg},
                {"last_message_time", lastMsgTime},
                {"is_unread", isUnread}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    // --- GROUP CHAT & MESSAGES ---
    bool createGroup(const std::string& name, const std::string& description, const std::string& createdBy,
                     const std::vector<std::string>& memberIds, Group& outGroup) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string groupId = "GRP" + std::to_string(nowMs);

        const char* sql = "INSERT INTO groups (id, name, description, avatar, created_by, created_at) VALUES (?, ?, ?, '', ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, createdBy.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, timeStr.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_DONE) {
            executeSimple("INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES ('" + groupId + "', '" + createdBy + "', 'ADMIN', '" + timeStr + "');");
            for (const auto& mid : memberIds) {
                if (mid == createdBy) continue;
                executeSimple("INSERT OR IGNORE INTO group_members (group_id, user_id, role, joined_at) VALUES ('" + groupId + "', '" + mid + "', 'MEMBER', '" + timeStr + "');");
            }

            outGroup.id = groupId;
            outGroup.name = name;
            outGroup.description = description;
            outGroup.avatar = "/img/default-avatar.svg";
            outGroup.created_by = createdBy;
            outGroup.created_at = timeStr;
            outGroup.member_count = (int)memberIds.size() + 1;
            return true;
        }
        return false;
    }

    std::vector<Group> getUserGroups(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<Group> groups;
        const char* sql = R"(
            SELECT g.id, g.name, g.description, g.avatar, g.created_by, g.created_at,
                   (SELECT COUNT(*) FROM group_members gm WHERE gm.group_id = g.id) as m_count,
                   (SELECT CASE WHEN gm2.is_revoked = 1 THEN 'Tin nhắn đã được thu hồi' ELSE gm2.content END FROM group_messages gm2 WHERE gm2.group_id = g.id ORDER BY gm2.id DESC LIMIT 1) as last_msg,
                   (SELECT gm2.created_at FROM group_messages gm2 WHERE gm2.group_id = g.id ORDER BY gm2.id DESC LIMIT 1) as last_msg_time
            FROM groups g
            JOIN group_members gm ON g.id = gm.group_id
            WHERE gm.user_id = ?
            ORDER BY last_msg_time DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return groups;

        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Group g;
            g.id = (const char*)sqlite3_column_text(stmt, 0);
            g.name = (const char*)sqlite3_column_text(stmt, 1);
            g.description = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
            g.avatar = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
            g.created_by = (const char*)sqlite3_column_text(stmt, 4);
            g.created_at = (const char*)sqlite3_column_text(stmt, 5);
            g.member_count = sqlite3_column_int(stmt, 6);
            g.last_message = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "Nhóm chat";
            g.last_message_time = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : g.created_at;
            groups.push_back(g);
        }
        sqlite3_finalize(stmt);
        return groups;
    }

    // Get members of a group with roles
    json getGroupMembers(const std::string& groupId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT u.id, u.fullname, u.avatar, gm.role, gm.joined_at, u.user_token
            FROM group_members gm
            JOIN users u ON gm.user_id = u.id
            WHERE gm.group_id = ?
            ORDER BY CASE gm.role WHEN 'ADMIN' THEN 0 ELSE 1 END, u.fullname;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string uid = (const char*)sqlite3_column_text(stmt, 0);
            std::string fname = (const char*)sqlite3_column_text(stmt, 1);
            std::string avatar = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
            std::string role = (const char*)sqlite3_column_text(stmt, 3);
            std::string joined = (const char*)sqlite3_column_text(stmt, 4);
            std::string utoken = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : uid;
            arr.push_back({
                {"id", uid}, {"name", fname},
                {"avatar", avatar.empty() ? "/img/default-avatar.svg" : avatar},
                {"role", role}, {"joined_at", joined}, {"user_token", utoken}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    bool addGroupMember(const std::string& groupId, const std::string& userId, const std::string& requesterId) {
        // Only add if requester is ADMIN or the user is not already a member
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        const char* sql = "INSERT OR IGNORE INTO group_members (group_id, user_id, role, joined_at) VALUES (?, ?, 'MEMBER', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool removeGroupMember(const std::string& groupId, const std::string& userId, const std::string& requesterId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // Verify requester is ADMIN of the group
        const char* checkReq = "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";
        sqlite3_stmt* rStmt = nullptr;
        std::string reqRole;
        if (sqlite3_prepare_v2(db, checkReq, -1, &rStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(rStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(rStmt, 2, requesterId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(rStmt) == SQLITE_ROW) {
                reqRole = (const char*)sqlite3_column_text(rStmt, 0);
            }
            sqlite3_finalize(rStmt);
        }
        if (reqRole != "ADMIN") return false; // Chỉ trưởng nhóm mới có quyền xóa thành viên

        // Cannot remove self via remove member (use leaveGroup instead)
        if (userId == requesterId) return false;

        const char* sql = "DELETE FROM group_members WHERE group_id = ? AND user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool leaveGroup(const std::string& groupId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM group_members WHERE group_id = ? AND user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool updateGroupAvatar(const std::string& groupId, const std::string& avatar) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE groups SET avatar = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, avatar.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, groupId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool updateGroupName(const std::string& groupId, const std::string& newName) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE groups SET name = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, newName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, groupId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool recallGroupMessage(int msgId, const std::string& senderId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // Kiểm tra thời gian gửi: chỉ cho phép thu hồi trong vòng 5 phút (300 giây)
        const char* checkSql = "SELECT created_at FROM group_messages WHERE id = ? AND sender_id = ?;";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, checkSql, -1, &cStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(cStmt, 1, msgId);
            sqlite3_bind_text(cStmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(cStmt) == SQLITE_ROW) {
                std::string createdAt = (const char*)sqlite3_column_text(cStmt, 0);
                auto now = std::chrono::system_clock::now();
                auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                std::tm t = {};
                t.tm_isdst = -1;
                std::istringstream ss(createdAt);
                ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail()) {
                    t.tm_isdst = -1;
                    auto msgTime = std::mktime(&t);
                    if (std::abs(nowSec - msgTime) > 300) {
                        sqlite3_finalize(cStmt);
                        return false; // Quá 5 phút
                    }
                }
            }
            sqlite3_finalize(cStmt);
        }

        const char* sql = "UPDATE group_messages SET is_revoked = 1 WHERE id = ? AND sender_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE && sqlite3_changes(db) > 0);
    }

    bool deleteGroupMessageForMe(int msgId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "INSERT OR IGNORE INTO group_messages_deleted (user_id, message_id) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, msgId);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool sendGroupMessage(const std::string& groupId, const std::string& senderId, const std::string& content,
                          const std::string& sharedPostId, GroupMessage& outMsg,
                          const std::string& mediaType = "NONE", const std::string& mediaUrl = "",
                          const std::string& replyToText = "", int replyToId = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        const char* sql = "INSERT INTO group_messages (group_id, sender_id, content, shared_post_id, media_type, media_url, reply_to_text, created_at, reply_to_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, sharedPostId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, mediaType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, mediaUrl.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, replyToText.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, timeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 9, replyToId);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_DONE) {
            outMsg.id = (int)sqlite3_last_insert_rowid(db);
            outMsg.group_id = groupId;
            outMsg.sender_id = senderId;
            outMsg.content = content;
            outMsg.shared_post_id = sharedPostId;
            outMsg.media_type = mediaType;
            outMsg.media_url = mediaUrl;
            outMsg.reply_to_text = replyToText;
            outMsg.reply_to_id = replyToId;
            outMsg.reaction = "";
            outMsg.created_at = timeStr;
            return true;
        }
        return false;
    }

    std::vector<GroupMessage> getGroupMessages(const std::string& groupId, int limit = 30, const std::string& viewerId = "") {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<GroupMessage> list;
        const char* sql = R"(
            SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, gm.content, COALESCE(gm.shared_post_id,''), gm.created_at,
                   COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), COALESCE(gm.is_revoked,0),
                   COALESCE(gm.reaction,''), COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0)
            FROM group_messages gm
            JOIN users u ON gm.sender_id = u.id
            WHERE gm.group_id = ?
            ORDER BY gm.id DESC LIMIT ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int msgId = sqlite3_column_int(stmt, 0);
            // Check if deleted for this viewer
            if (!viewerId.empty()) {
                const char* checkDel = "SELECT 1 FROM group_messages_deleted WHERE user_id = ? AND message_id = ?;";
                sqlite3_stmt* dStmt = nullptr;
                bool isDeleted = false;
                if (sqlite3_prepare_v2(db, checkDel, -1, &dStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(dStmt, 1, viewerId.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_int(dStmt, 2, msgId);
                    isDeleted = (sqlite3_step(dStmt) == SQLITE_ROW);
                    sqlite3_finalize(dStmt);
                }
                if (isDeleted) continue;
            }
            GroupMessage m;
            m.id = msgId;
            m.group_id = (const char*)sqlite3_column_text(stmt, 1);
            m.sender_id = (const char*)sqlite3_column_text(stmt, 2);
            m.sender_name = (const char*)sqlite3_column_text(stmt, 3);
            m.sender_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            m.content = (const char*)sqlite3_column_text(stmt, 5);
            m.shared_post_id = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            m.created_at = (const char*)sqlite3_column_text(stmt, 7);
            m.media_type = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "NONE";
            m.media_url = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            m.is_revoked = sqlite3_column_int(stmt, 10);
            m.reaction = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            m.reply_to_text = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            m.reply_to_id = sqlite3_column_int(stmt, 13);
            list.push_back(m);
        }
        sqlite3_finalize(stmt);

        std::vector<GroupMessage> chronological;
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            chronological.push_back(*it);
        }
        return chronological;
    }

    // Get user's role in group
    std::string getGroupRole(const std::string& groupId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        std::string role;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) role = (const char*)sqlite3_column_text(stmt, 0);
            sqlite3_finalize(stmt);
        }
        return role;
    }

    // Get friends list that are in a group (for adding members check)
    json getFriendsNotInGroup(const std::string& myId, const std::string& groupId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT u.id, u.fullname, u.avatar, u.user_token
            FROM friendships f
            JOIN users u ON f.user_id_2 = u.id
            WHERE f.user_id_1 = ? AND f.status = 'FRIEND'
              AND u.id NOT IN (SELECT user_id FROM group_members WHERE group_id = ?);
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, groupId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string uid = (const char*)sqlite3_column_text(stmt, 0);
            std::string fname = (const char*)sqlite3_column_text(stmt, 1);
            std::string avatar = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
            std::string utoken = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : uid;
            arr.push_back({{"id", uid}, {"name", fname},
                {"avatar", avatar.empty() ? "/img/default-avatar.svg" : avatar},
                {"user_token", utoken}});
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    // --- NOTIFICATIONS (Hình 4) ---
    void addNotificationInternal(const std::string& userId, const std::string& actorId, const std::string& type,
                                 const std::string& content, const std::string& link) {
        std::string timeStr = getCurrentTimeStr();
        const char* sql = "INSERT INTO notifications (user_id, actor_id, type, content, link, is_read, created_at) VALUES (?, ?, ?, ?, ?, 0, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, actorId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, content.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, link.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 6, timeStr.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    std::vector<Notification> getUserNotifications(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<Notification> list;
        const char* sql = R"(
            SELECT n.id, n.user_id, n.actor_id, u.fullname, u.avatar, n.type, n.content, n.link, n.is_read, n.created_at
            FROM notifications n
            JOIN users u ON n.actor_id = u.id
            WHERE n.user_id = ?
            ORDER BY n.id DESC LIMIT 20;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Notification n;
            n.id = sqlite3_column_int(stmt, 0);
            n.user_id = (const char*)sqlite3_column_text(stmt, 1);
            n.actor_id = (const char*)sqlite3_column_text(stmt, 2);
            n.actor_name = (const char*)sqlite3_column_text(stmt, 3);
            n.actor_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            n.type = (const char*)sqlite3_column_text(stmt, 5);
            n.content = (const char*)sqlite3_column_text(stmt, 6);
            n.link = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            n.is_read = sqlite3_column_int(stmt, 8);
            n.created_at = (const char*)sqlite3_column_text(stmt, 9);
            list.push_back(n);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    void markNotificationsRead(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE notifications SET is_read = 1 WHERE user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    void markSingleNotificationRead(int notifId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE notifications SET is_read = 1 WHERE id = ? AND user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, notifId);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    bool reactMessage(int msgId, const std::string& reaction) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE messages SET reaction = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, reaction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, msgId);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool reactGroupMessage(int msgId, const std::string& reaction) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE group_messages SET reaction = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, reaction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, msgId);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    // --- SEARCH REALTIME & SEARCH PAGE (Hình 3 & Hình 4) ---
    json searchEverything(const std::string& keyword, const std::string& filter, const std::string& currentUserId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json result;
        result["users"] = json::array();
        result["groups"] = json::array();
        result["posts"] = json::array();

        std::string pattern = "%" + keyword + "%";

        // Tìm người dùng
        if (filter == "ALL" || filter == "PEOPLE") {
            const char* uSql = R"(
                SELECT id, username, fullname, avatar, school, hometown, user_token
                FROM users
                WHERE (fullname LIKE ? OR username LIKE ? OR school LIKE ? OR hometown LIKE ?) AND role != 'ADMIN'
                LIMIT 15;
            )";
            sqlite3_stmt* uStmt = nullptr;
            if (sqlite3_prepare_v2(db, uSql, -1, &uStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(uStmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 3, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 4, pattern.c_str(), -1, SQLITE_STATIC);
                while (sqlite3_step(uStmt) == SQLITE_ROW) {
                    std::string uid = (const char*)sqlite3_column_text(uStmt, 0);
                    std::string uname = (const char*)sqlite3_column_text(uStmt, 1);
                    std::string fname = (const char*)sqlite3_column_text(uStmt, 2);
                    std::string av = sqlite3_column_text(uStmt, 3) ? (const char*)sqlite3_column_text(uStmt, 3) : "/img/default-avatar.svg";
                    std::string school = sqlite3_column_text(uStmt, 4) ? (const char*)sqlite3_column_text(uStmt, 4) : "";
                    std::string hometown = sqlite3_column_text(uStmt, 5) ? (const char*)sqlite3_column_text(uStmt, 5) : "";
                    std::string utoken = sqlite3_column_text(uStmt, 6) ? (const char*)sqlite3_column_text(uStmt, 6) : uid;

                    result["users"].push_back({
                        {"id", uid},
                        {"username", uname},
                        {"name", fname},
                        {"avatar", av},
                        {"user_token", utoken},
                        {"subtitle", school.empty() ? hometown : school}
                    });
                }
                sqlite3_finalize(uStmt);
            }
        }

        // Tìm nhóm
        if (filter == "ALL" || filter == "GROUPS") {
            const char* gSql = R"(
                SELECT g.id, g.name, g.description, g.avatar,
                       (SELECT COUNT(*) FROM group_members gm WHERE gm.group_id = g.id) as m_count
                FROM groups g
                WHERE g.name LIKE ? OR g.description LIKE ?
                LIMIT 10;
            )";
            sqlite3_stmt* gStmt = nullptr;
            if (sqlite3_prepare_v2(db, gSql, -1, &gStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(gStmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(gStmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
                while (sqlite3_step(gStmt) == SQLITE_ROW) {
                    std::string gid = (const char*)sqlite3_column_text(gStmt, 0);
                    std::string gname = (const char*)sqlite3_column_text(gStmt, 1);
                    std::string desc = sqlite3_column_text(gStmt, 2) ? (const char*)sqlite3_column_text(gStmt, 2) : "";
                    std::string av = sqlite3_column_text(gStmt, 3) ? (const char*)sqlite3_column_text(gStmt, 3) : "/img/default-avatar.svg";
                    int mcount = sqlite3_column_int(gStmt, 4);

                    result["groups"].push_back({
                        {"id", gid},
                        {"name", gname},
                        {"description", desc},
                        {"avatar", av},
                        {"member_count", mcount},
                        {"subtitle", "Công khai · " + std::to_string(mcount) + " thành viên"}
                    });
                }
                sqlite3_finalize(gStmt);
            }
        }

        // Tìm bài viết
        if (filter == "ALL" || filter == "POSTS") {
            const char* pSql = R"(
                SELECT p.id, p.author_id, u.fullname, u.avatar, p.content, p.created_at
                FROM posts p
                JOIN users u ON p.author_id = u.id
                WHERE p.content LIKE ?
                ORDER BY p.created_at DESC LIMIT 10;
            )";
            sqlite3_stmt* pStmt = nullptr;
            if (sqlite3_prepare_v2(db, pSql, -1, &pStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(pStmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
                while (sqlite3_step(pStmt) == SQLITE_ROW) {
                    result["posts"].push_back({
                        {"id", (const char*)sqlite3_column_text(pStmt, 0)},
                        {"author_id", (const char*)sqlite3_column_text(pStmt, 1)},
                        {"author_name", (const char*)sqlite3_column_text(pStmt, 2)},
                        {"author_avatar", sqlite3_column_text(pStmt, 3) ? (const char*)sqlite3_column_text(pStmt, 3) : "/img/default-avatar.svg"},
                        {"content", (const char*)sqlite3_column_text(pStmt, 4)},
                        {"created_at", (const char*)sqlite3_column_text(pStmt, 5)}
                    });
                }
                sqlite3_finalize(pStmt);
            }
        }

        return result;
    }

    // --- REPORTS ---
    bool createReport(const std::string& reporterId, const std::string& reportedUserId,
                      const std::string& reportedPostId, const std::string& reportedCommentId, const std::string& reason) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string timeStr = getCurrentTimeStr();
        const char* sql = "INSERT INTO reports (reporter_id, reported_user_id, reported_post_id, reported_comment_id, reason, status, created_at) "
                          "VALUES (?, ?, ?, ?, ?, 'PENDING', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, reporterId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, reportedUserId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, reportedPostId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, reportedCommentId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, reason.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, timeStr.c_str(), -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool createReport(const std::string& reporterId, const std::string& reportedUserId,
                      const std::string& reportedPostId, const std::string& reason) {
        return createReport(reporterId, reportedUserId, reportedPostId, "", reason);
    }

    std::vector<Report> getAllReports() {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<Report> list;
        const char* sql = R"(
            SELECT r.id, r.reporter_id, u1.fullname, r.reported_user_id,
                   COALESCE(u2.fullname, ''), r.reported_post_id, r.reported_comment_id, r.reason, r.status, r.created_at
            FROM reports r
            JOIN users u1 ON r.reporter_id = u1.id
            LEFT JOIN users u2 ON r.reported_user_id = u2.id
            ORDER BY r.id DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Report r;
            r.id = sqlite3_column_int(stmt, 0);
            r.reporter_id = (const char*)sqlite3_column_text(stmt, 1);
            r.reporter_name = (const char*)sqlite3_column_text(stmt, 2);
            r.reported_user_id = sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "";
            r.reported_user_name = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            r.reported_post_id = sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "";
            r.reported_comment_id = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            r.reason = (const char*)sqlite3_column_text(stmt, 7);
            r.status = (const char*)sqlite3_column_text(stmt, 8);
            r.created_at = (const char*)sqlite3_column_text(stmt, 9);
            list.push_back(r);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    bool resolveReport(int reportId, const std::string& action) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE reports SET status = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, action.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, reportId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            return true;
        }
        return false;
    }

    // --- ADMIN DASHBOARD ---
    json getAdminStats() {
        std::lock_guard<std::mutex> lock(dbMutex);
        auto queryCount = [this](const char* sql) -> int {
            sqlite3_stmt* stmt = nullptr;
            int count = 0;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
            }
            return count;
        };

        return {
            {"total_users", queryCount("SELECT COUNT(*) FROM users;")},
            {"locked_users", queryCount("SELECT COUNT(*) FROM users WHERE status = 'LOCKED';")},
            {"total_posts", queryCount("SELECT COUNT(*) FROM posts;")},
            {"total_messages", queryCount("SELECT COUNT(*) FROM messages;")},
            {"total_reports", queryCount("SELECT COUNT(*) FROM reports;")},
            {"pending_reports", queryCount("SELECT COUNT(*) FROM reports WHERE status = 'PENDING';")},
            {"total_groups", queryCount("SELECT COUNT(*) FROM groups;")}
        };
    }

    bool toggleUserLock(const std::string& userId, bool& nowLocked) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* getSql = "SELECT status FROM users WHERE id = ?;";
        sqlite3_stmt* gStmt = nullptr;
        std::string curr = "ACTIVE";
        if (sqlite3_prepare_v2(db, getSql, -1, &gStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(gStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(gStmt) == SQLITE_ROW) {
                curr = (const char*)sqlite3_column_text(gStmt, 0);
            }
            sqlite3_finalize(gStmt);
        }

        std::string next = (curr == "LOCKED") ? "ACTIVE" : "LOCKED";
        nowLocked = (next == "LOCKED");

        const char* uSql = "UPDATE users SET status = ? WHERE id = ?;";
        sqlite3_stmt* uStmt = nullptr;
        if (sqlite3_prepare_v2(db, uSql, -1, &uStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(uStmt, 1, next.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(uStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(uStmt);
            sqlite3_finalize(uStmt);
            return true;
        }
        return false;
    }

    bool deletePost(const std::string& postId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM posts WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool updatePost(const std::string& postId, const std::string& authorId, const std::string& content, const std::string& title = "", const std::string& visibility = "PUBLIC") {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE posts SET content = ?, title = ?, visibility = ? WHERE id = ? AND author_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, visibility.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, authorId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    static inline std::string removeVietnameseTones(const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (size_t i = 0; i < input.size(); ) {
            unsigned char c = (unsigned char)input[i];
            if (c < 0x80) {
                out += (char)std::tolower(c);
                i += 1;
            } else if ((c == 0xC3 || c == 0xC4 || c == 0xC5 || c == 0xC6) && i + 1 < input.size()) {
                unsigned char c2 = (unsigned char)input[i + 1];
                if (c == 0xC3) {
                    if (c2 == 0xA0 || c2 == 0xA1 || c2 == 0xA2 || c2 == 0xA3 || c2 == 0x80 || c2 == 0x81 || c2 == 0x82 || c2 == 0x83) {
                        out += 'a';
                    } else if (c2 == 0xA8 || c2 == 0xA9 || c2 == 0xAA || c2 == 0x88 || c2 == 0x89 || c2 == 0x8A) {
                        out += 'e';
                    } else if (c2 == 0xAC || c2 == 0xAD || c2 == 0x8C || c2 == 0x8D) {
                        out += 'i';
                    } else if (c2 == 0xB2 || c2 == 0xB3 || c2 == 0xB4 || c2 == 0xB5 || c2 == 0x92 || c2 == 0x93 || c2 == 0x94 || c2 == 0x95) {
                        out += 'o';
                    } else if (c2 == 0xB9 || c2 == 0xBA || c2 == 0x99 || c2 == 0x9A) {
                        out += 'u';
                    } else if (c2 == 0xBD || c2 == 0x9D) {
                        out += 'y';
                    } else {
                        out += (char)std::tolower(c);
                        out += (char)std::tolower(c2);
                    }
                } else if (c == 0xC4) {
                    if (c2 == 0x82 || c2 == 0x83) {
                        out += 'a';
                    } else if (c2 == 0x90 || c2 == 0x91) {
                        out += 'd';
                    } else if (c2 == 0xA8 || c2 == 0xA9) {
                        out += 'i';
                    } else {
                        out += (char)std::tolower(c);
                        out += (char)std::tolower(c2);
                    }
                } else if (c == 0xC5) {
                    if (c2 == 0xA8 || c2 == 0xA9) {
                        out += 'u';
                    } else {
                        out += (char)std::tolower(c);
                        out += (char)std::tolower(c2);
                    }
                } else if (c == 0xC6) {
                    if (c2 == 0xA0 || c2 == 0xA1) {
                        out += 'o';
                    } else if (c2 == 0xAF || c2 == 0xB0) {
                        out += 'u';
                    } else {
                        out += (char)std::tolower(c);
                        out += (char)std::tolower(c2);
                    }
                }
                i += 2;
            } else if (c == 0xE1 && i + 2 < input.size()) {
                unsigned char c2 = (unsigned char)input[i + 1];
                unsigned char c3 = (unsigned char)input[i + 2];
                if (c2 == 0xBA) {
                    if (c3 >= 0x80 && c3 <= 0xB7) {
                        out += 'a';
                    } else if (c3 >= 0xB8 && c3 <= 0xC7) {
                        out += 'e';
                    } else {
                        out += (char)c; out += (char)c2; out += (char)c3;
                    }
                } else if (c2 == 0xBB) {
                    if (c3 >= 0x88 && c3 <= 0x8D) {
                        out += 'i';
                    } else if (c3 >= 0x8E && c3 <= 0xA3) {
                        out += 'o';
                    } else if (c3 >= 0xA4 && c3 <= 0xB3) {
                        out += 'u';
                    } else if (c3 >= 0xB4 && c3 <= 0xB9) {
                        out += 'y';
                    } else {
                        out += (char)c; out += (char)c2; out += (char)c3;
                    }
                } else {
                    out += (char)c; out += (char)c2; out += (char)c3;
                }
                i += 3;
            } else {
                out += (char)c;
                i += 1;
            }
        }
        return out;
    }

    std::vector<Message> getMessagesAround(const std::string& user1, const std::string& user2, int targetId, int range = 15) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(user1, user2);
        std::vector<Message> messages;

        // 1. Lấy tối đa 'range' tin nhắn cũ hơn targetId
        std::vector<Message> beforeMsgs;
        const char* beforeSql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                                "FROM messages m JOIN users u ON m.sender_id = u.id "
                                "WHERE m.conversation_id = ? AND m.id < ? ORDER BY m.id DESC LIMIT ?;";
        sqlite3_stmt* bStmt = nullptr;
        if (sqlite3_prepare_v2(db, beforeSql, -1, &bStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(bStmt, 1, convId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(bStmt, 2, targetId);
            sqlite3_bind_int(bStmt, 3, range);
            while (sqlite3_step(bStmt) == SQLITE_ROW) {
                Message m;
                m.id = sqlite3_column_int(bStmt, 0);
                m.conversation_id = (const char*)sqlite3_column_text(bStmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(bStmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(bStmt, 3);
                m.sender_avatar = sqlite3_column_text(bStmt, 4) ? (const char*)sqlite3_column_text(bStmt, 4) : "";
                m.recipient_id = (const char*)sqlite3_column_text(bStmt, 5);
                m.content = (const char*)sqlite3_column_text(bStmt, 6);
                m.shared_post_id = sqlite3_column_text(bStmt, 7) ? (const char*)sqlite3_column_text(bStmt, 7) : "";
                m.is_revoked = sqlite3_column_int(bStmt, 8);
                m.created_at = (const char*)sqlite3_column_text(bStmt, 9);
                m.media_type = sqlite3_column_text(bStmt, 10) ? (const char*)sqlite3_column_text(bStmt, 10) : "NONE";
                m.media_url = sqlite3_column_text(bStmt, 11) ? (const char*)sqlite3_column_text(bStmt, 11) : "";
                m.reaction = sqlite3_column_text(bStmt, 12) ? (const char*)sqlite3_column_text(bStmt, 12) : "";
                m.reply_to_text = sqlite3_column_text(bStmt, 13) ? (const char*)sqlite3_column_text(bStmt, 13) : "";
                m.is_read = sqlite3_column_int(bStmt, 14);
                m.reply_to_id = sqlite3_column_int(bStmt, 15);
                beforeMsgs.push_back(m);
            }
            sqlite3_finalize(bStmt);
        }
        for (auto it = beforeMsgs.rbegin(); it != beforeMsgs.rend(); ++it) {
            messages.push_back(*it);
        }

        // 2. Lấy chính tin nhắn đích
        const char* targetSql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                                "FROM messages m JOIN users u ON m.sender_id = u.id "
                                "WHERE m.conversation_id = ? AND m.id = ?;";
        sqlite3_stmt* tStmt = nullptr;
        if (sqlite3_prepare_v2(db, targetSql, -1, &tStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(tStmt, 1, convId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(tStmt, 2, targetId);
            if (sqlite3_step(tStmt) == SQLITE_ROW) {
                Message m;
                m.id = sqlite3_column_int(tStmt, 0);
                m.conversation_id = (const char*)sqlite3_column_text(tStmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(tStmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(tStmt, 3);
                m.sender_avatar = sqlite3_column_text(tStmt, 4) ? (const char*)sqlite3_column_text(tStmt, 4) : "";
                m.recipient_id = (const char*)sqlite3_column_text(tStmt, 5);
                m.content = (const char*)sqlite3_column_text(tStmt, 6);
                m.shared_post_id = sqlite3_column_text(tStmt, 7) ? (const char*)sqlite3_column_text(tStmt, 7) : "";
                m.is_revoked = sqlite3_column_int(tStmt, 8);
                m.created_at = (const char*)sqlite3_column_text(tStmt, 9);
                m.media_type = sqlite3_column_text(tStmt, 10) ? (const char*)sqlite3_column_text(tStmt, 10) : "NONE";
                m.media_url = sqlite3_column_text(tStmt, 11) ? (const char*)sqlite3_column_text(tStmt, 11) : "";
                m.reaction = sqlite3_column_text(tStmt, 12) ? (const char*)sqlite3_column_text(tStmt, 12) : "";
                m.reply_to_text = sqlite3_column_text(tStmt, 13) ? (const char*)sqlite3_column_text(tStmt, 13) : "";
                m.is_read = sqlite3_column_int(tStmt, 14);
                m.reply_to_id = sqlite3_column_int(tStmt, 15);
                messages.push_back(m);
            }
            sqlite3_finalize(tStmt);
        }

        // 3. Lấy tối đa 'range' tin nhắn mới hơn targetId
        const char* afterSql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                               "FROM messages m JOIN users u ON m.sender_id = u.id "
                               "WHERE m.conversation_id = ? AND m.id > ? ORDER BY m.id ASC LIMIT ?;";
        sqlite3_stmt* aStmt = nullptr;
        if (sqlite3_prepare_v2(db, afterSql, -1, &aStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(aStmt, 1, convId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(aStmt, 2, targetId);
            sqlite3_bind_int(aStmt, 3, range);
            while (sqlite3_step(aStmt) == SQLITE_ROW) {
                Message m;
                m.id = sqlite3_column_int(aStmt, 0);
                m.conversation_id = (const char*)sqlite3_column_text(aStmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(aStmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(aStmt, 3);
                m.sender_avatar = sqlite3_column_text(aStmt, 4) ? (const char*)sqlite3_column_text(aStmt, 4) : "";
                m.recipient_id = (const char*)sqlite3_column_text(aStmt, 5);
                m.content = (const char*)sqlite3_column_text(aStmt, 6);
                m.shared_post_id = sqlite3_column_text(aStmt, 7) ? (const char*)sqlite3_column_text(aStmt, 7) : "";
                m.is_revoked = sqlite3_column_int(aStmt, 8);
                m.created_at = (const char*)sqlite3_column_text(aStmt, 9);
                m.media_type = sqlite3_column_text(aStmt, 10) ? (const char*)sqlite3_column_text(aStmt, 10) : "NONE";
                m.media_url = sqlite3_column_text(aStmt, 11) ? (const char*)sqlite3_column_text(aStmt, 11) : "";
                m.reaction = sqlite3_column_text(aStmt, 12) ? (const char*)sqlite3_column_text(aStmt, 12) : "";
                m.reply_to_text = sqlite3_column_text(aStmt, 13) ? (const char*)sqlite3_column_text(aStmt, 13) : "";
                m.is_read = sqlite3_column_int(aStmt, 14);
                m.reply_to_id = sqlite3_column_int(aStmt, 15);
                messages.push_back(m);
            }
            sqlite3_finalize(aStmt);
        }

        return messages;
    }

    std::vector<GroupMessage> getGroupMessagesAround(const std::string& groupId, int targetId, int range = 15) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<GroupMessage> messages;

        // 1. Tin nhắn cũ hơn
        std::vector<GroupMessage> beforeMsgs;
        const char* bSql = "SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, gm.content, COALESCE(gm.shared_post_id,''), gm.created_at, "
                           "COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), COALESCE(gm.is_revoked,0), COALESCE(gm.reaction,''), COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0) "
                           "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
                           "WHERE gm.group_id = ? AND gm.id < ? ORDER BY gm.id DESC LIMIT ?;";
        sqlite3_stmt* bStmt = nullptr;
        if (sqlite3_prepare_v2(db, bSql, -1, &bStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(bStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(bStmt, 2, targetId);
            sqlite3_bind_int(bStmt, 3, range);
            while (sqlite3_step(bStmt) == SQLITE_ROW) {
                GroupMessage m;
                m.id = sqlite3_column_int(bStmt, 0);
                m.group_id = (const char*)sqlite3_column_text(bStmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(bStmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(bStmt, 3);
                m.sender_avatar = sqlite3_column_text(bStmt, 4) ? (const char*)sqlite3_column_text(bStmt, 4) : "";
                m.content = (const char*)sqlite3_column_text(bStmt, 5);
                m.shared_post_id = sqlite3_column_text(bStmt, 6) ? (const char*)sqlite3_column_text(bStmt, 6) : "";
                m.created_at = (const char*)sqlite3_column_text(bStmt, 7);
                m.media_type = sqlite3_column_text(bStmt, 8) ? (const char*)sqlite3_column_text(bStmt, 8) : "NONE";
                m.media_url = sqlite3_column_text(bStmt, 9) ? (const char*)sqlite3_column_text(bStmt, 9) : "";
                m.is_revoked = sqlite3_column_int(bStmt, 10);
                m.reaction = sqlite3_column_text(bStmt, 11) ? (const char*)sqlite3_column_text(bStmt, 11) : "";
                m.reply_to_text = sqlite3_column_text(bStmt, 12) ? (const char*)sqlite3_column_text(bStmt, 12) : "";
                m.reply_to_id = sqlite3_column_int(bStmt, 13);
                beforeMsgs.push_back(m);
            }
            sqlite3_finalize(bStmt);
        }
        for (auto it = beforeMsgs.rbegin(); it != beforeMsgs.rend(); ++it) {
            messages.push_back(*it);
        }

        // 2. Tin nhắn đích
        const char* tSql = "SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, gm.content, COALESCE(gm.shared_post_id,''), gm.created_at, "
                           "COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), COALESCE(gm.is_revoked,0), COALESCE(gm.reaction,''), COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0) "
                           "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
                           "WHERE gm.group_id = ? AND gm.id = ?;";
        sqlite3_stmt* tStmt = nullptr;
        if (sqlite3_prepare_v2(db, tSql, -1, &tStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(tStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(tStmt, 2, targetId);
            if (sqlite3_step(tStmt) == SQLITE_ROW) {
                GroupMessage m;
                m.id = sqlite3_column_int(tStmt, 0);
                m.group_id = (const char*)sqlite3_column_text(tStmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(tStmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(tStmt, 3);
                m.sender_avatar = sqlite3_column_text(tStmt, 4) ? (const char*)sqlite3_column_text(tStmt, 4) : "";
                m.content = (const char*)sqlite3_column_text(tStmt, 5);
                m.shared_post_id = sqlite3_column_text(tStmt, 6) ? (const char*)sqlite3_column_text(tStmt, 6) : "";
                m.created_at = (const char*)sqlite3_column_text(tStmt, 7);
                m.media_type = sqlite3_column_text(tStmt, 8) ? (const char*)sqlite3_column_text(tStmt, 8) : "NONE";
                m.media_url = sqlite3_column_text(tStmt, 9) ? (const char*)sqlite3_column_text(tStmt, 9) : "";
                m.is_revoked = sqlite3_column_int(tStmt, 10);
                m.reaction = sqlite3_column_text(tStmt, 11) ? (const char*)sqlite3_column_text(tStmt, 11) : "";
                m.reply_to_text = sqlite3_column_text(tStmt, 12) ? (const char*)sqlite3_column_text(tStmt, 12) : "";
                m.reply_to_id = sqlite3_column_int(tStmt, 13);
                messages.push_back(m);
            }
            sqlite3_finalize(tStmt);
        }

        // 3. Tin nhắn mới hơn
        const char* aSql = "SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, gm.content, COALESCE(gm.shared_post_id,''), gm.created_at, "
                           "COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), COALESCE(gm.is_revoked,0), COALESCE(gm.reaction,''), COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0) "
                           "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
                           "WHERE gm.group_id = ? AND gm.id > ? ORDER BY gm.id ASC LIMIT ?;";
        sqlite3_stmt* aStmt = nullptr;
        if (sqlite3_prepare_v2(db, aSql, -1, &aStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(aStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(aStmt, 2, targetId);
            sqlite3_bind_int(aStmt, 3, range);
            while (sqlite3_step(aStmt) == SQLITE_ROW) {
                GroupMessage m;
                m.id = sqlite3_column_int(aStmt, 0);
                m.group_id = (const char*)sqlite3_column_text(aStmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(aStmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(aStmt, 3);
                m.sender_avatar = sqlite3_column_text(aStmt, 4) ? (const char*)sqlite3_column_text(aStmt, 4) : "";
                m.content = (const char*)sqlite3_column_text(aStmt, 5);
                m.shared_post_id = sqlite3_column_text(aStmt, 6) ? (const char*)sqlite3_column_text(aStmt, 6) : "";
                m.created_at = (const char*)sqlite3_column_text(aStmt, 7);
                m.media_type = sqlite3_column_text(aStmt, 8) ? (const char*)sqlite3_column_text(aStmt, 8) : "NONE";
                m.media_url = sqlite3_column_text(aStmt, 9) ? (const char*)sqlite3_column_text(aStmt, 9) : "";
                m.is_revoked = sqlite3_column_int(aStmt, 10);
                m.reaction = sqlite3_column_text(aStmt, 11) ? (const char*)sqlite3_column_text(aStmt, 11) : "";
                m.reply_to_text = sqlite3_column_text(aStmt, 12) ? (const char*)sqlite3_column_text(aStmt, 12) : "";
                m.reply_to_id = sqlite3_column_int(aStmt, 13);
                messages.push_back(m);
            }
            sqlite3_finalize(aStmt);
        }

        return messages;
    }

    std::vector<Message> searchMessages(const std::string& user1, const std::string& user2, const std::string& query) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(user1, user2);
        std::vector<Message> results;
        if (query.empty()) return results;

        std::string normQ = removeVietnameseTones(query);

        std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                          "FROM messages m JOIN users u ON m.sender_id = u.id "
                          "WHERE m.conversation_id = ? AND m.is_revoked = 0 ORDER BY m.id DESC;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

        sqlite3_bind_text(stmt, 1, convId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string content = (const char*)sqlite3_column_text(stmt, 6);
            std::string normContent = removeVietnameseTones(content);
            if (normContent.find(normQ) != std::string::npos) {
                Message m;
                m.id = sqlite3_column_int(stmt, 0);
                m.conversation_id = (const char*)sqlite3_column_text(stmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(stmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(stmt, 3);
                m.sender_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
                m.recipient_id = (const char*)sqlite3_column_text(stmt, 5);
                m.content = content;
                m.shared_post_id = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
                m.is_revoked = sqlite3_column_int(stmt, 8);
                m.created_at = (const char*)sqlite3_column_text(stmt, 9);
                m.media_type = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "NONE";
                m.media_url = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
                m.reaction = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
                m.reply_to_text = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
                m.is_read = sqlite3_column_int(stmt, 14);
                m.reply_to_id = sqlite3_column_int(stmt, 15);
                results.push_back(m);
                if (results.size() >= 50) break;
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<GroupMessage> searchGroupMessages(const std::string& groupId, const std::string& query, const std::string& viewerId = "") {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<GroupMessage> results;
        if (query.empty()) return results;

        std::string normQ = removeVietnameseTones(query);

        std::string sql = "SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, gm.content, COALESCE(gm.shared_post_id,''), gm.created_at, "
                          "COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), COALESCE(gm.is_revoked,0), COALESCE(gm.reaction,''), COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0) "
                          "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
                          "WHERE gm.group_id = ? AND gm.is_revoked = 0 ORDER BY gm.id DESC;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int msgId = sqlite3_column_int(stmt, 0);
            if (!viewerId.empty()) {
                const char* checkDel = "SELECT 1 FROM group_messages_deleted WHERE user_id = ? AND message_id = ?;";
                sqlite3_stmt* dStmt = nullptr;
                bool isDeleted = false;
                if (sqlite3_prepare_v2(db, checkDel, -1, &dStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(dStmt, 1, viewerId.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_int(dStmt, 2, msgId);
                    isDeleted = (sqlite3_step(dStmt) == SQLITE_ROW);
                    sqlite3_finalize(dStmt);
                }
                if (isDeleted) continue;
            }

            std::string content = (const char*)sqlite3_column_text(stmt, 5);
            std::string normContent = removeVietnameseTones(content);
            if (normContent.find(normQ) != std::string::npos) {
                GroupMessage m;
                m.id = msgId;
                m.group_id = (const char*)sqlite3_column_text(stmt, 1);
                m.sender_id = (const char*)sqlite3_column_text(stmt, 2);
                m.sender_name = (const char*)sqlite3_column_text(stmt, 3);
                m.sender_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
                m.content = content;
                m.shared_post_id = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
                m.created_at = (const char*)sqlite3_column_text(stmt, 7);
                m.media_type = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "NONE";
                m.media_url = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
                m.is_revoked = sqlite3_column_int(stmt, 10);
                m.reaction = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
                m.reply_to_text = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
                m.reply_to_id = sqlite3_column_int(stmt, 13);
                results.push_back(m);
                if (results.size() >= 50) break;
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<SearchHistoryItem> getSearchHistory(const std::string& userId, int limit = 10) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<SearchHistoryItem> list;
        const char* sql = "SELECT id, user_id, query, created_at FROM search_history WHERE user_id = ? ORDER BY created_at DESC LIMIT ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SearchHistoryItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.user_id = (const char*)sqlite3_column_text(stmt, 1);
            item.query = (const char*)sqlite3_column_text(stmt, 2);
            item.created_at = (const char*)sqlite3_column_text(stmt, 3);
            list.push_back(item);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    bool addSearchHistory(const std::string& userId, const std::string& query) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string q = query;
        while (!q.empty() && (q.front() == ' ' || q.front() == '\t')) q.erase(q.begin());
        while (!q.empty() && (q.back() == ' ' || q.back() == '\t')) q.pop_back();
        if (q.empty() || userId.empty()) return false;

        const char* delSql = "DELETE FROM search_history WHERE user_id = ? AND query = ?;";
        sqlite3_stmt* delStmt = nullptr;
        if (sqlite3_prepare_v2(db, delSql, -1, &delStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(delStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(delStmt, 2, q.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(delStmt);
            sqlite3_finalize(delStmt);
        }

        std::string timeStr = getCurrentTimeStr();
        const char* inSql = "INSERT INTO search_history (user_id, query, created_at) VALUES (?, ?, ?);";
        sqlite3_stmt* inStmt = nullptr;
        if (sqlite3_prepare_v2(db, inSql, -1, &inStmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(inStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 2, q.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(inStmt);
        sqlite3_finalize(inStmt);
        return (rc == SQLITE_DONE);
    }

    bool deleteSearchHistoryItem(const std::string& userId, int id) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM search_history WHERE user_id = ? AND id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool clearSearchHistory(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM search_history WHERE user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool repostPost(const std::string& userId, const std::string& originalPostId, const std::string& thoughts, const std::string& visibility, std::string& outNewPostId, std::string& errMessage) {
        User reposter;
        if (!getUserById(userId, reposter) || reposter.status != "ACTIVE") {
            errMessage = "Tài khoản không hợp lệ hoặc đã bị khóa.";
            return false;
        }

        Post origPost;
        if (!getPostById(originalPostId, userId, origPost)) {
            errMessage = "Bài viết gốc không tồn tại hoặc đã bị xóa.";
            return false;
        }

        // Nếu bài viết này đã là repost, chuyển tới bài viết gốc ban đầu
        if (!origPost.shared_post_id.empty() && origPost.has_original_post) {
            Post rootPost;
            if (getPostById(origPost.shared_post_id, userId, rootPost)) {
                origPost = rootPost;
            }
        }

        if (origPost.author_id == userId) {
            errMessage = "Bạn không thể đăng lại bài viết của chính mình.";
            return false;
        }

        if (origPost.visibility == "PRIVATE") {
            errMessage = "Bài viết này ở chế độ riêng tư, không thể đăng lại.";
            return false;
        }

        std::string rel = getFriendshipRelation(userId, origPost.author_id);
        if (rel == "BLOCKED") {
            errMessage = "Không thể tương tác với bài viết của người này.";
            return false;
        }
        if (origPost.visibility == "FRIENDS" && rel != "FRIEND") {
            errMessage = "Chỉ bạn bè của tác giả mới có quyền đăng lại bài viết này.";
            return false;
        }

        std::string timeStr = getCurrentTimeStr();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string newPostId = "POST" + std::to_string(nowMs);
        std::string postVis = visibility.empty() ? "PUBLIC" : visibility;

        {
            std::lock_guard<std::mutex> lock(dbMutex);
            const char* inSql = "INSERT INTO posts (id, author_id, content, media_type, media_url, shared_post_id, shared_author_name, shared_content, created_at, title, visibility) "
                                "VALUES (?, ?, ?, 'NONE', '', ?, ?, ?, ?, '', ?);";
            sqlite3_stmt* inStmt = nullptr;
            if (sqlite3_prepare_v2(db, inSql, -1, &inStmt, nullptr) != SQLITE_OK) {
                errMessage = "Lỗi cơ sở dữ liệu khi tạo bài đăng lại.";
                return false;
            }

            sqlite3_bind_text(inStmt, 1, newPostId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 3, thoughts.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 4, origPost.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 5, origPost.author_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 6, origPost.content.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 7, timeStr.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(inStmt, 8, postVis.c_str(), -1, SQLITE_STATIC);

            int rc = sqlite3_step(inStmt);
            sqlite3_finalize(inStmt);
            if (rc != SQLITE_DONE) {
                errMessage = "Không thể lưu bài đăng lại vào cơ sở dữ liệu.";
                return false;
            }

            addNotificationInternal(origPost.author_id, userId, "REPOST", "đã đăng lại bài viết của bạn.", "/index.html#post-" + newPostId);
        }

        outNewPostId = newPostId;
        return true;
    }
};
