#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <random>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <cmath>
#include <cctype>
#include <sqlite3.h>
#include "Models.h"
#include "Security.h"

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
        std::tm utcTime{};
#ifdef _WIN32
        gmtime_s(&utcTime, &in_time_t);
#else
        gmtime_r(&in_time_t, &utcTime);
#endif
        std::stringstream ss;
        ss << std::put_time(&utcTime, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    json getMessageReactionSummaries(int messageId, bool isGroup) {
        json byEmoji = json::object();
        const char* table = isGroup ? "group_message_reactions" : "message_reactions";
        const char* idColumn = isGroup ? "group_message_id" : "message_id";
        const std::string sql = std::string("SELECT r.emoji, u.id, u.fullname, u.avatar, u.user_token ") +
            "FROM " + table + " r JOIN users u ON u.id = r.user_id WHERE r." + idColumn +
            " = ? ORDER BY r.created_at ASC, u.fullname COLLATE NOCASE ASC;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to load message reactions: " << sqlite3_errmsg(db) << std::endl;
            return json::array();
        }
        sqlite3_bind_int(stmt, 1, messageId);
        int stepResult = SQLITE_OK;
        while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW) {
            const auto* emojiText = sqlite3_column_text(stmt, 0);
            const auto* idText = sqlite3_column_text(stmt, 1);
            const auto* nameText = sqlite3_column_text(stmt, 2);
            const auto* avatarText = sqlite3_column_text(stmt, 3);
            const auto* tokenText = sqlite3_column_text(stmt, 4);
            const std::string emoji = emojiText ? reinterpret_cast<const char*>(emojiText) : "";
            if (!byEmoji.contains(emoji)) byEmoji[emoji] = {{"emoji", emoji}, {"count", 0}, {"users", json::array()}};
            byEmoji[emoji]["count"] = byEmoji[emoji]["count"].get<int>() + 1;
            byEmoji[emoji]["users"].push_back({
                {"user_id", idText ? reinterpret_cast<const char*>(idText) : ""},
                {"name", nameText ? reinterpret_cast<const char*>(nameText) : "Người dùng"},
                {"avatar", avatarText ? reinterpret_cast<const char*>(avatarText) : ""},
                {"user_token", tokenText ? reinterpret_cast<const char*>(tokenText) : ""}
            });
        }
        sqlite3_finalize(stmt);
        if (stepResult != SQLITE_DONE) {
            std::cerr << "Failed to read message reactions: " << sqlite3_errmsg(db) << std::endl;
            return json::array();
        }

        json summaries = json::array();
        for (auto& [emoji, summary] : byEmoji.items()) summaries.push_back(summary);
        std::sort(summaries.begin(), summaries.end(), [](const json& left, const json& right) {
            if (left["count"] != right["count"]) return left["count"].get<int>() > right["count"].get<int>();
            return left["emoji"].get<std::string>() < right["emoji"].get<std::string>();
        });
        return summaries;
    }

    void syncPendingDeliveriesForUserLocked(const std::string& userId) {
        if (userId.empty()) return;
        const std::string now = getCurrentTimeStr();
        const char* dSql = R"(
            INSERT OR IGNORE INTO direct_message_receipts (message_id, user_id, delivered_at, read_at)
            SELECT m.id, ?1, ?2, ''
            FROM messages m
            WHERE m.recipient_id = ?1 AND m.sender_id <> ?1
              AND NOT EXISTS (
                  SELECT 1 FROM direct_message_receipts r 
                  WHERE r.message_id = m.id AND r.user_id = ?1
              );
        )";
        sqlite3_stmt* dStmt = nullptr;
        if (sqlite3_prepare_v2(db, dSql, -1, &dStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(dStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(dStmt, 2, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(dStmt);
            sqlite3_finalize(dStmt);
        }

        const char* gSql = R"(
            INSERT OR IGNORE INTO group_message_receipts (group_message_id, user_id, delivered_at, read_at)
            SELECT gm.id, ?1, ?2, ''
            FROM group_messages gm
            JOIN group_members gmbr ON gmbr.group_id = gm.group_id AND gmbr.user_id = ?1
            WHERE gm.sender_id <> ?1
              AND NOT EXISTS (
                  SELECT 1 FROM group_messages_deleted gmd 
                  WHERE gmd.message_id = gm.id AND gmd.user_id = ?1
              )
              AND NOT EXISTS (
                  SELECT 1 FROM group_message_receipts r 
                  WHERE r.group_message_id = gm.id AND r.user_id = ?1
              );
        )";
        sqlite3_stmt* gStmt = nullptr;
        if (sqlite3_prepare_v2(db, gSql, -1, &gStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(gStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(gStmt, 2, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(gStmt);
            sqlite3_finalize(gStmt);
        }
    }

public:
    Database() {}
    ~Database() {
        close();
    }

    json getMessageReceiptDetails(int messageId, bool isGroup) {
        const char* table = isGroup ? "group_message_receipts" : "direct_message_receipts";
        const char* messageColumn = isGroup ? "group_message_id" : "message_id";
        const std::string sql = std::string(
            "SELECT r.user_id, u.fullname, u.avatar, r.delivered_at, r.read_at "
            "FROM ") + table + " r JOIN users u ON u.id = r.user_id WHERE r." +
            messageColumn + " = ? ORDER BY CASE WHEN r.read_at = '' THEN 1 ELSE 0 END, r.read_at DESC, r.delivered_at DESC, u.fullname COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to load message receipts: " << sqlite3_errmsg(db) << std::endl;
            return json::array();
        }
        sqlite3_bind_int(stmt, 1, messageId);
        json receipts = json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* userId = sqlite3_column_text(stmt, 0);
            const auto* name = sqlite3_column_text(stmt, 1);
            const auto* avatar = sqlite3_column_text(stmt, 2);
            const auto* deliveredAt = sqlite3_column_text(stmt, 3);
            const auto* readAt = sqlite3_column_text(stmt, 4);
            receipts.push_back({
                {"user_id", userId ? reinterpret_cast<const char*>(userId) : ""},
                {"name", name ? reinterpret_cast<const char*>(name) : "Người dùng"},
                {"avatar", avatar && *avatar ? reinterpret_cast<const char*>(avatar) : "/img/default-avatar.svg"},
                {"delivered_at", deliveredAt ? reinterpret_cast<const char*>(deliveredAt) : ""},
                {"read_at", readAt ? reinterpret_cast<const char*>(readAt) : ""}
            });
        }
        sqlite3_finalize(stmt);
        return receipts;
    }

    void recordDirectMessageDelivered(int messageId, const std::string& userId) {
        const std::string now = getCurrentTimeStr();
        const char* sql = R"(
            INSERT OR IGNORE INTO direct_message_receipts (message_id, user_id, delivered_at, read_at)
            SELECT id, ?, ?, '' FROM messages
            WHERE id = ? AND recipient_id = ? AND sender_id <> ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, messageId);
        sqlite3_bind_text(stmt, 4, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        const char* updateSql = "UPDATE direct_message_receipts SET delivered_at = ? WHERE message_id = ? AND user_id = ? AND delivered_at = '';";
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, messageId);
            sqlite3_bind_text(stmt, 3, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    void recordGroupMessageDelivered(int messageId, const std::string& groupId, const std::string& userId) {
        const std::string now = getCurrentTimeStr();
        const char* sql = R"(
            INSERT OR IGNORE INTO group_message_receipts (group_message_id, user_id, delivered_at, read_at)
            SELECT gm.id, ?, ?, '' FROM group_messages gm
            WHERE gm.id = ? AND gm.group_id = ? AND gm.sender_id <> ?
              AND EXISTS (SELECT 1 FROM group_members WHERE group_id = gm.group_id AND user_id = ?)
              AND NOT EXISTS (SELECT 1 FROM group_messages_deleted WHERE message_id = gm.id AND user_id = ?);
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, messageId);
        sqlite3_bind_text(stmt, 4, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        const char* updateSql = "UPDATE group_message_receipts SET delivered_at = ? WHERE group_message_id = ? AND user_id = ? AND delivered_at = '';";
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, messageId);
            sqlite3_bind_text(stmt, 3, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    void syncPendingDeliveriesForUser(const std::string& userId) {
        if (userId.empty()) return;
        std::lock_guard<std::mutex> lock(dbMutex);
        syncPendingDeliveriesForUserLocked(userId);
    }

    bool getDirectMessageById(int messageId, Message& outMsg) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, "
                          "m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), "
                          "COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), "
                          "m.is_read, COALESCE(m.reply_to_id, 0) "
                          "FROM messages m JOIN users u ON m.sender_id = u.id "
                          "WHERE m.id = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, messageId);
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outMsg.id = sqlite3_column_int(stmt, 0);
            outMsg.conversation_id = (const char*)sqlite3_column_text(stmt, 1);
            outMsg.sender_id = (const char*)sqlite3_column_text(stmt, 2);
            outMsg.sender_name = (const char*)sqlite3_column_text(stmt, 3);
            outMsg.sender_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            outMsg.recipient_id = (const char*)sqlite3_column_text(stmt, 5);
            outMsg.content = (const char*)sqlite3_column_text(stmt, 6);
            outMsg.shared_post_id = sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "";
            outMsg.is_revoked = sqlite3_column_int(stmt, 8);
            outMsg.created_at = (const char*)sqlite3_column_text(stmt, 9);
            outMsg.media_type = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "NONE";
            outMsg.media_url = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            outMsg.reaction = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            outMsg.reply_to_text = sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "";
            outMsg.is_read = sqlite3_column_int(stmt, 14);
            outMsg.reply_to_id = sqlite3_column_int(stmt, 15);
            found = true;
        }
        sqlite3_finalize(stmt);
        if (found) {
            outMsg.receipts = getMessageReceiptDetails(outMsg.id, false);
            outMsg.reactions = getMessageReactionSummaries(outMsg.id, false);
        }
        return found;
    }

    bool getGroupMessageById(int messageId, GroupMessage& outMsg) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, "
                          "gm.content, COALESCE(gm.shared_post_id,''), gm.created_at, "
                          "COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), "
                          "COALESCE(gm.is_revoked, 0), COALESCE(gm.reaction,''), "
                          "COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0) "
                          "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
                          "WHERE gm.id = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, messageId);
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outMsg.id = sqlite3_column_int(stmt, 0);
            outMsg.group_id = (const char*)sqlite3_column_text(stmt, 1);
            outMsg.sender_id = (const char*)sqlite3_column_text(stmt, 2);
            outMsg.sender_name = (const char*)sqlite3_column_text(stmt, 3);
            outMsg.sender_avatar = sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "";
            outMsg.content = (const char*)sqlite3_column_text(stmt, 5);
            outMsg.shared_post_id = sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "";
            outMsg.created_at = (const char*)sqlite3_column_text(stmt, 7);
            outMsg.media_type = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "NONE";
            outMsg.media_url = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            outMsg.is_revoked = sqlite3_column_int(stmt, 10);
            outMsg.reaction = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            outMsg.reply_to_text = sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "";
            outMsg.reply_to_id = sqlite3_column_int(stmt, 13);
            found = true;
        }
        sqlite3_finalize(stmt);
        if (found) {
            outMsg.receipts = getMessageReceiptDetails(outMsg.id, true);
            outMsg.reactions = getMessageReactionSummaries(outMsg.id, true);
        }
        return found;
    }

    bool canUserViewMessageReceipts(const std::string& userId, int messageId, bool isGroup) {
        if (userId.empty() || messageId <= 0) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        if (isGroup) {
            const char* sql = "SELECT 1 FROM group_messages gm JOIN group_members gmbr ON gmbr.group_id = gm.group_id WHERE gm.id = ? AND gmbr.user_id = ? LIMIT 1;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_int(stmt, 1, messageId);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            bool canView = (sqlite3_step(stmt) == SQLITE_ROW);
            sqlite3_finalize(stmt);
            return canView;
        } else {
            const char* sql = "SELECT 1 FROM messages WHERE id = ? AND (sender_id = ? OR recipient_id = ?) LIMIT 1;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_int(stmt, 1, messageId);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, userId.c_str(), -1, SQLITE_STATIC);
            bool canView = (sqlite3_step(stmt) == SQLITE_ROW);
            sqlite3_finalize(stmt);
            return canView;
        }
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
        seedCommunityGroupsIfEmpty();
        securityBootstrapLocked();
        return true;
    }

    void seedCommunityGroupsIfEmpty() {
        // Caller (init) đã giữ dbMutex — không lock lại để tránh deadlock
        sqlite3_stmt* stmt = nullptr;
        int count = 0;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM community_groups;", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        if (count > 0 || !sec::envFlag("SEED_DEMO_DATA")) return;

        std::string now = getCurrentTimeStr();
        // Nhóm mẫu
        executeSimple("INSERT OR IGNORE INTO community_groups (id, name, description, avatar, privacy, created_by, created_at) VALUES "
            "('CG000001', 'Lập trình C++ Việt Nam', 'Cộng đồng chia sẻ kiến thức C++, web server, SQLite và dự án mã nguồn mở.', '', 'PUBLIC', 'ACC000001', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_groups (id, name, description, avatar, privacy, created_by, created_at) VALUES "
            "('CG000002', 'Đời sống sinh viên', 'Chia sẻ chuyện học tập, ký túc xá, việc làm thêm và các mẹo hay cho sinh viên.', '', 'PUBLIC', 'ACC000002', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_groups (id, name, description, avatar, privacy, created_by, created_at) VALUES "
            "('CG000003', 'Thể thao & Sức khỏe', 'Chạy bộ, bóng đá, gym và lối sống lành mạnh.', '', 'PUBLIC', 'ACC000003', '" + now + "');");

        // Thành viên
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000001', 'ACC000001', 'ADMIN', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000001', 'ACC000002', 'MEMBER', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000001', 'ACC000003', 'MEMBER', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000002', 'ACC000002', 'ADMIN', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000002', 'ACC000001', 'MEMBER', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000002', 'ACC000004', 'MEMBER', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000003', 'ACC000003', 'ADMIN', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES ('CG000003', 'ACC000001', 'MEMBER', '" + now + "');");

        // Bài viết đã duyệt
        executeSimple("INSERT OR IGNORE INTO community_group_posts (id, group_id, author_id, content, media_type, media_url, approval_status, created_at) VALUES "
            "('CGP000001', 'CG000001', 'ACC000001', 'Chào mừng mọi người đến nhóm Lập trình C++! Hãy chia sẻ project và câu hỏi của bạn nhé.', 'NONE', '', 'APPROVED', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_posts (id, group_id, author_id, content, media_type, media_url, approval_status, created_at) VALUES "
            "('CGP000002', 'CG000001', 'ACC000002', 'Mình vừa viết xong một HTTP server bằng C++17 + SQLite, performance khá ổn trên localhost.', 'NONE', '', 'APPROVED', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_posts (id, group_id, author_id, content, media_type, media_url, approval_status, created_at) VALUES "
            "('CGP000003', 'CG000002', 'ACC000002', 'Cuối tuần này mọi người có kế hoạch gì không? Đi cafe code chung không nào?', 'NONE', '', 'APPROVED', '" + now + "');");
        executeSimple("INSERT OR IGNORE INTO community_group_posts (id, group_id, author_id, content, media_type, media_url, approval_status, created_at) VALUES "
            "('CGP000004', 'CG000003', 'ACC000003', 'Sáng nay chạy 5km, pace ổn. Ai cần partner chạy bộ khu Cầu Giấy inbox nhé!', 'NONE', '', 'APPROVED', '" + now + "');");
        // Bài chờ duyệt (thành viên đăng)
        executeSimple("INSERT OR IGNORE INTO community_group_posts (id, group_id, author_id, content, media_type, media_url, approval_status, created_at) VALUES "
            "('CGP000005', 'CG000001', 'ACC000003', 'Có ai dùng httplib + nlohmann/json chưa? Mình đang kẹt chỗ multipart upload.', 'NONE', '', 'PENDING', '" + now + "');");
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

    bool weakAdminPassword_ = false;

    // Chạy một lần khi khởi động (đã giữ dbMutex): băm mật khẩu cũ, làm sạch dữ liệu, tạo admin an toàn
    void securityBootstrapLocked() {
        // 1) Băm mọi mật khẩu còn lưu dạng văn bản thường (DB cũ lưu plaintext)
        std::vector<std::pair<std::string, std::string>> toFix;
        {
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT id, password_hash FROM users;", -1, &st, nullptr) == SQLITE_OK) {
                while (sqlite3_step(st) == SQLITE_ROW) {
                    const unsigned char* a = sqlite3_column_text(st, 0);
                    const unsigned char* b = sqlite3_column_text(st, 1);
                    const std::string id = a ? reinterpret_cast<const char*>(a) : "";
                    const std::string pw = b ? reinterpret_cast<const char*>(b) : "";
                    if (!sec::isHashedPassword(pw) && pw != "!") toFix.emplace_back(id, pw);
                }
                sqlite3_finalize(st);
            }
        }
        if (!toFix.empty()) {
            std::cout << "[Bảo mật] Đang băm " << toFix.size() << " mật khẩu lưu dạng văn bản thường (PBKDF2-SHA256)..." << std::endl;
            sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
            for (const auto& f : toFix) {
                const std::string h = (f.second.empty() || f.second == "GOOGLE_AUTH") ? "!" : sec::hashPassword(f.second);
                sqlite3_stmt* up = nullptr;
                if (sqlite3_prepare_v2(db, "UPDATE users SET password_hash = ? WHERE id = ?;", -1, &up, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(up, 1, h.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(up, 2, f.first.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(up);
                    sqlite3_finalize(up);
                }
            }
            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        }

        // 2) Làm sạch dữ liệu hồ sơ cũ có thể chứa ký tự gây XSS
        auto strip = [](const char* col) {
            return std::string("replace(replace(replace(replace(replace(") + col + ",'<',''),'>',''),'\"',''),'`',''),'\\','')";
        };
        executeSimple("UPDATE users SET fullname = " + strip("fullname") + ", hometown = " + strip("hometown") + ", school = " + strip("school") +
                      ", workplace = " + strip("workplace") + ", residence = " + strip("residence") + ";");
        executeSimple("UPDATE users SET avatar = '' WHERE avatar LIKE '%\"%' OR avatar LIKE '%''%' OR avatar LIKE '%<%' OR avatar LIKE '%>%' OR avatar LIKE '% %' OR lower(avatar) LIKE 'javascript:%';");
        executeSimple("UPDATE groups SET name = " + strip("name") + ";");
        executeSimple("UPDATE community_groups SET name = " + strip("name") + ";");

        // 2b) Token admin cũ (lưu plaintext, không hết hạn) bị thu hồi hết
        executeSimple("DELETE FROM admin_sessions WHERE expires_at IS NULL OR expires_at = 0;");
        executeSimple("DELETE FROM admin_sessions WHERE expires_at < strftime('%s','now');");
        executeSimple("DELETE FROM user_sessions WHERE expires_at < strftime('%s','now');");

        // 3) Tài khoản admin: tạo mới nếu chưa có, đồng bộ mật khẩu từ ADMIN_PASSWORD, cảnh báo mật khẩu mặc định
        const std::string envPw = sec::envOr("ADMIN_PASSWORD");
        int admins = 0;
        {
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE role = 'ADMIN';", -1, &st, nullptr) == SQLITE_OK) {
                if (sqlite3_step(st) == SQLITE_ROW) admins = sqlite3_column_int(st, 0);
                sqlite3_finalize(st);
            }
        }
        if (admins == 0) {
            std::string pw = envPw;
            bool generated = false;
            if (pw.empty()) { pw = sec::randomToken(9); generated = true; }  // 18 ký tự ngẫu nhiên
            const std::string h = sec::hashPassword(pw);
            const std::string now = getCurrentTimeStr();
            const std::string tok = sec::randomToken(8);
            sqlite3_stmt* ins = nullptr;
            if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO users (id, username, email, password_hash, fullname, bio, role, status, last_active_at, created_at, user_token) "
                                       "VALUES ('ACC000000', 'admin', 'admin@mangxahoi.vn', ?, 'Quản Trị Viên Hệ Thống', 'Ban quản trị hệ thống', 'ADMIN', 'ACTIVE', ?, ?, ?);", -1, &ins, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(ins, 1, h.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 2, now.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 3, now.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 4, tok.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(ins);
                sqlite3_finalize(ins);
            }
            std::cout << "[Bảo mật] Đã tạo tài khoản admin (username: admin)." << std::endl;
            if (generated) std::cout << "[Bảo mật] MẬT KHẨU ADMIN NGẪU NHIÊN (chỉ hiện 1 lần, hãy lưu lại): " << pw << std::endl;
        } else {
            std::vector<std::pair<std::string, std::string>> adminRows;
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT id, password_hash FROM users WHERE role = 'ADMIN';", -1, &st, nullptr) == SQLITE_OK) {
                while (sqlite3_step(st) == SQLITE_ROW) {
                    const unsigned char* a = sqlite3_column_text(st, 0);
                    const unsigned char* b = sqlite3_column_text(st, 1);
                    adminRows.emplace_back(a ? reinterpret_cast<const char*>(a) : "", b ? reinterpret_cast<const char*>(b) : "");
                }
                sqlite3_finalize(st);
            }
            for (auto& r : adminRows) {
                if (!envPw.empty() && !sec::verifyPassword(envPw, r.second)) {
                    const std::string h = sec::hashPassword(envPw);
                    sqlite3_stmt* up = nullptr;
                    if (sqlite3_prepare_v2(db, "UPDATE users SET password_hash = ? WHERE id = ?;", -1, &up, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(up, 1, h.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(up, 2, r.first.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_step(up);
                        sqlite3_finalize(up);
                    }
                    r.second = h;
                    executeSimple("DELETE FROM admin_sessions;");
                    std::cout << "[Bảo mật] Đã đồng bộ mật khẩu admin từ ADMIN_PASSWORD (các phiên admin cũ bị thu hồi)." << std::endl;
                }
                if (sec::verifyPassword("Admin@123", r.second)) weakAdminPassword_ = true;
            }
            if (weakAdminPassword_) std::cerr << "[CẢNH BÁO BẢO MẬT] Tài khoản admin đang dùng mật khẩu mặc định 'Admin@123' (công khai trong tài liệu)! Đặt ADMIN_PASSWORD trong .env." << std::endl;
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

            CREATE TABLE IF NOT EXISTS message_reactions (
                message_id INTEGER NOT NULL,
                user_id TEXT NOT NULL,
                emoji TEXT NOT NULL,
                created_at TEXT NOT NULL,
                PRIMARY KEY (message_id, user_id),
                FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE CASCADE,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS group_message_reactions (
                group_message_id INTEGER NOT NULL,
                user_id TEXT NOT NULL,
                emoji TEXT NOT NULL,
                created_at TEXT NOT NULL,
                PRIMARY KEY (group_message_id, user_id),
                FOREIGN KEY (group_message_id) REFERENCES group_messages(id) ON DELETE CASCADE,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
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

            CREATE TABLE IF NOT EXISTS comment_reactions (
                comment_id TEXT NOT NULL,
                user_id TEXT NOT NULL,
                type TEXT NOT NULL,
                created_at TEXT NOT NULL,
                PRIMARY KEY (comment_id, user_id),
                FOREIGN KEY (comment_id) REFERENCES comments(id) ON DELETE CASCADE,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
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

            CREATE TABLE IF NOT EXISTS direct_message_receipts (
                message_id INTEGER NOT NULL,
                user_id TEXT NOT NULL,
                delivered_at TEXT NOT NULL DEFAULT '',
                read_at TEXT NOT NULL DEFAULT '',
                PRIMARY KEY (message_id, user_id),
                FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE CASCADE,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS group_message_receipts (
                group_message_id INTEGER NOT NULL,
                user_id TEXT NOT NULL,
                delivered_at TEXT NOT NULL DEFAULT '',
                read_at TEXT NOT NULL DEFAULT '',
                PRIMARY KEY (group_message_id, user_id),
                FOREIGN KEY (group_message_id) REFERENCES group_messages(id) ON DELETE CASCADE,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
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
            CREATE INDEX IF NOT EXISTS idx_direct_receipts_message ON direct_message_receipts(message_id);
            CREATE INDEX IF NOT EXISTS idx_direct_receipts_user ON direct_message_receipts(user_id);
            CREATE INDEX IF NOT EXISTS idx_direct_receipts_read_at ON direct_message_receipts(read_at);
            CREATE INDEX IF NOT EXISTS idx_group_receipts_message ON group_message_receipts(group_message_id);
            CREATE INDEX IF NOT EXISTS idx_group_receipts_user ON group_message_receipts(user_id);
            CREATE INDEX IF NOT EXISTS idx_group_receipts_read_at ON group_message_receipts(read_at);
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
        tryAlter("ALTER TABLE reports ADD COLUMN resolution_note TEXT DEFAULT '';");
        tryAlter("ALTER TABLE reports ADD COLUMN resolved_by TEXT DEFAULT '';");
        tryAlter("ALTER TABLE reports ADD COLUMN resolved_at TEXT DEFAULT '';");
        tryAlter("ALTER TABLE reports ADD COLUMN category TEXT DEFAULT '';");
        executeSimple("CREATE TABLE IF NOT EXISTS admin_sessions (token TEXT PRIMARY KEY, admin_id TEXT NOT NULL, created_at TEXT NOT NULL);");
        tryAlter("ALTER TABLE admin_sessions ADD COLUMN expires_at INTEGER DEFAULT 0;");
        // Phiên đăng nhập người dùng: chỉ lưu SHA-256 của token (DB bị lộ vẫn không dùng được token)
        executeSimple("CREATE TABLE IF NOT EXISTS user_sessions (token_hash TEXT PRIMARY KEY, user_id TEXT NOT NULL, created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, last_seen INTEGER NOT NULL, ip TEXT DEFAULT '', user_agent TEXT DEFAULT '');");
        executeSimple("CREATE INDEX IF NOT EXISTS idx_user_sessions_user ON user_sessions(user_id);");
        tryAlter("ALTER TABLE otp_codes ADD COLUMN attempts INTEGER DEFAULT 0;");
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
        executeSimple(R"(
            INSERT OR IGNORE INTO direct_message_receipts (message_id, user_id, delivered_at, read_at)
            SELECT id, recipient_id, created_at, CASE WHEN is_read = 1 THEN created_at ELSE '' END
            FROM messages WHERE sender_id <> recipient_id;
        )");
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
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS chat_preferences (
                user_id TEXT NOT NULL,
                conversation_key TEXT NOT NULL,
                is_archived INTEGER NOT NULL DEFAULT 0,
                force_unread INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY(user_id, conversation_key)
            );
        )");
        // Chặn người dùng (giữ quan hệ bạn bè, chỉ ẩn tương tác)
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS user_blocks (
                blocker_id TEXT NOT NULL,
                blocked_id TEXT NOT NULL,
                created_at TEXT NOT NULL,
                PRIMARY KEY (blocker_id, blocked_id)
            );
        )");
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS admin_audit_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                admin_id TEXT NOT NULL,
                action TEXT NOT NULL,
                target_type TEXT DEFAULT '',
                target_id TEXT DEFAULT '',
                detail TEXT DEFAULT '',
                created_at TEXT NOT NULL
            );
        )");
        // Nhóm cộng đồng (khác nhóm chat)
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS community_groups (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                avatar TEXT DEFAULT '',
                privacy TEXT DEFAULT 'PUBLIC',
                created_by TEXT NOT NULL,
                created_at TEXT NOT NULL
            );
        )");
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS community_group_members (
                group_id TEXT NOT NULL,
                user_id TEXT NOT NULL,
                role TEXT DEFAULT 'MEMBER',
                joined_at TEXT NOT NULL,
                PRIMARY KEY (group_id, user_id)
            );
        )");
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS community_group_posts (
                id TEXT PRIMARY KEY,
                group_id TEXT NOT NULL,
                author_id TEXT NOT NULL,
                content TEXT NOT NULL,
                media_type TEXT DEFAULT 'NONE',
                media_url TEXT DEFAULT '',
                approval_status TEXT DEFAULT 'PENDING',
                created_at TEXT NOT NULL,
                FOREIGN KEY (group_id) REFERENCES community_groups(id) ON DELETE CASCADE,
                FOREIGN KEY (author_id) REFERENCES users(id)
            );
        )");
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS community_group_post_reactions (
                post_id TEXT NOT NULL,
                user_id TEXT NOT NULL,
                type TEXT NOT NULL,
                created_at TEXT NOT NULL,
                PRIMARY KEY (post_id, user_id)
            );
        )");
        executeSimple(R"(
            CREATE TABLE IF NOT EXISTS community_group_post_comments (
                id TEXT PRIMARY KEY,
                post_id TEXT NOT NULL,
                author_id TEXT NOT NULL,
                content TEXT NOT NULL,
                created_at TEXT NOT NULL
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

        // AN TOÀN: bản cũ xoá TOÀN BỘ dữ liệu và nạp lại tài khoản demo (mật khẩu công khai) mỗi khi có < 10 user.
        // Nay chỉ nạp demo khi DB trống hoàn toàn VÀ bật SEED_DEMO_DATA=1 (chỉ dùng khi phát triển).
        if (count > 0 || !sec::envFlag("SEED_DEMO_DATA")) return;

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
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000003', 'POST000001', 'ACC000005', 'Chúc mừng team, giao diện nhìn rất hiện đại và tinh tế.', '', '', '2026-11-04 09:20:00');");

        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000004', 'POST000002', 'ACC000004', 'Quán cà phê góc phố nào thế nàng ơi, nhìn ấm cúng quá! ☕', '', '', '2026-11-04 09:30:00');");
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000005', 'POST000002', 'ACC000002', 'Góc Đinh Tiên Hoàng nhìn ra hồ Hoàn Kiếm đấy cậu ơi.', 'CMT000004', 'Phạm Ngọc Anh', '2026-11-04 09:45:00');");

        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000006', 'POST000003', 'ACC000001', 'Tuyệt vời bác ơi, độ phức tạp O(V + E) chạy mượt mà ngay cả với hàng triệu quan hệ!', '', '', '2026-11-04 10:35:00');");
        executeSimple("INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES ('CMT000007', 'POST000003', 'ACC000009', 'Cho xin link repo tham khảo với bro ơi, mình cũng đang nghiên cứu.', '', '', '2026-11-04 10:50:00');");

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
        syncPendingDeliveriesForUserLocked(userId);
    }

    std::string generateNewUserId() {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT COALESCE(MAX(CAST(SUBSTR(id, 4) AS INTEGER)), 0) FROM users;";
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
    // OTP chỉ lưu dạng băm (gắn với email + mục đích), tối đa 5 lần thử, hiệu lực ngắn
    static std::string otpDigest(const std::string& email, const std::string& purpose, const std::string& code) {
        return sec::sha256Hex(email + ":" + purpose + ":" + code);
    }

    bool saveOtp(const std::string& email, const std::string& code, const std::string& purpose, int expirySeconds = 300) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const long long expiresAt = sec::epochSeconds() + expirySeconds;

        const char* invalidateSql = "UPDATE otp_codes SET is_used = 1 WHERE email = ? AND purpose = ?;";
        sqlite3_stmt* invStmt = nullptr;
        if (sqlite3_prepare_v2(db, invalidateSql, -1, &invStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(invStmt, 1, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(invStmt, 2, purpose.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(invStmt);
            sqlite3_finalize(invStmt);
        }
        // Dọn OTP cũ để bảng không phình to
        executeSimple("DELETE FROM otp_codes WHERE is_used = 1 OR expires_at < strftime('%s','now') - 3600;");

        const std::string digest = otpDigest(email, purpose, code);
        const char* insertSql = "INSERT INTO otp_codes (email, code, purpose, expires_at, is_used, attempts) VALUES (?, ?, ?, ?, 0, 0);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, digest.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, purpose.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, expiresAt);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    bool verifyOtp(const std::string& email, const std::string& code, const std::string& purpose) {
        if (code.size() != 6) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const long long now = sec::epochSeconds();
        const char* sql = "SELECT id, code, attempts FROM otp_codes WHERE email = ? AND purpose = ? AND is_used = 0 AND expires_at > ? ORDER BY id DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, purpose.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, now);
        long long otpId = 0; int attempts = 0; std::string stored;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            otpId = sqlite3_column_int64(stmt, 0);
            const unsigned char* c = sqlite3_column_text(stmt, 1);
            stored = c ? reinterpret_cast<const char*>(c) : "";
            attempts = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
        if (otpId == 0) return false;

        const bool match = attempts < 5 && sec::ctEquals(stored, otpDigest(email, purpose, code));
        const char* upd = match ? "UPDATE otp_codes SET is_used = 1 WHERE id = ?;"
                                : "UPDATE otp_codes SET attempts = attempts + 1, is_used = CASE WHEN attempts + 1 >= 5 THEN 1 ELSE 0 END WHERE id = ?;";
        sqlite3_stmt* u = nullptr;
        if (sqlite3_prepare_v2(db, upd, -1, &u, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(u, 1, otpId);
            sqlite3_step(u);
            sqlite3_finalize(u);
        }
        return match;
    }

    // --- FRIENDSHIP & REQUESTS ---
    std::string getFriendshipRelation(const std::string& myId, const std::string& targetId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // Ưu tiên trạng thái chặn từ bảng user_blocks
        if (isUserBlockedUnlocked(myId, targetId)) return "BLOCKED_BY_ME";
        if (isUserBlockedUnlocked(targetId, myId)) return "BLOCKED";
        const char* fSql = "SELECT status FROM friendships WHERE user_id_1 = ? AND user_id_2 = ?;";
        sqlite3_stmt* fStmt = nullptr;
        if (sqlite3_prepare_v2(db, fSql, -1, &fStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(fStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(fStmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(fStmt) == SQLITE_ROW) {
                std::string st = (const char*)sqlite3_column_text(fStmt, 0);
                sqlite3_finalize(fStmt);
                if (st == "BLOCKED") return "BLOCKED_BY_ME";
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
        // Không cho gửi lời mời kết bạn nếu đã chặn (hai chiều)
        if (isUserBlockedUnlocked(requesterId, receiverId) || isUserBlockedUnlocked(receiverId, requesterId)) {
            return false;
        }
        // Admin không tham gia kết bạn (cả chiều gửi và nhận)
        auto roleOf = [this](const std::string& uid) -> std::string {
            const char* q = "SELECT role FROM users WHERE id = ? LIMIT 1;";
            sqlite3_stmt* s = nullptr;
            std::string role;
            if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s, 1, uid.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
                    role = (const char*)sqlite3_column_text(s, 0);
                sqlite3_finalize(s);
            }
            return role;
        };
        if (roleOf(requesterId) == "ADMIN" || roleOf(receiverId) == "ADMIN") return false;
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
            SELECT u.id, u.username, u.email, u.fullname, u.avatar, u.bio, u.hometown, u.school, u.workplace, u.interests, r.created_at, COALESCE(u.user_token, '')
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
            u.user_token = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
            list.push_back(u);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    // Lời mời kết bạn mình đã gửi (outgoing)
    std::vector<User> getSentFriendRequests(const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<User> list;
        const char* sql = R"(
            SELECT u.id, u.username, u.email, u.fullname, u.avatar, u.bio, u.hometown, u.school, u.workplace, u.interests, r.created_at, COALESCE(u.user_token, '')
            FROM friend_requests r
            JOIN users u ON r.receiver_id = u.id
            WHERE r.requester_id = ? AND r.status = 'PENDING';
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
            u.user_token = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "";
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
        const char* sql = R"(
            SELECT user_id_2 FROM friendships
            WHERE user_id_1 = ? AND status = 'FRIEND'
              AND user_id_2 NOT IN (SELECT blocked_id FROM user_blocks WHERE blocker_id = ?)
              AND user_id_2 NOT IN (SELECT user_id_2 FROM friendships WHERE user_id_1 = ? AND status = 'BLOCKED')
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return friends;

        sqlite3_bind_text(stmt, 1, realId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, realId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, realId.c_str(), -1, SQLITE_STATIC);
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
            WHERE f.user_id_1 = ? AND f.status = 'FRIEND'
              AND u.id NOT IN (SELECT blocked_id FROM user_blocks WHERE blocker_id = ?)
              AND u.id NOT IN (SELECT user_id_2 FROM friendships WHERE user_id_1 = ? AND status = 'BLOCKED')
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, realId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, realId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, realId.c_str(), -1, SQLITE_STATIC);
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
        if (myId.empty() || targetId.empty() || myId == targetId) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        // Không hủy kết bạn — chỉ ghi nhận chặn; hủy lời mời kết bạn đang chờ
        std::string timeStr = getCurrentTimeStr();
        const char* inSql = "INSERT OR REPLACE INTO user_blocks (blocker_id, blocked_id, created_at) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, inSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        // Hủy lời mời kết bạn hai chiều (nếu còn PENDING)
        const char* cancelReq = "DELETE FROM friend_requests WHERE (requester_id = ? AND receiver_id = ?) OR (requester_id = ? AND receiver_id = ?);";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, cancelReq, -1, &cStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(cStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(cStmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(cStmt, 3, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(cStmt, 4, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(cStmt);
            sqlite3_finalize(cStmt);
        }
        // Đồng bộ legacy: đánh dấu BLOCKED một chiều để các chỗ cũ vẫn hiểu
        const char* legacySql = "INSERT OR REPLACE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES (?, ?, 'BLOCKED', ?);";
        sqlite3_stmt* leg = nullptr;
        if (sqlite3_prepare_v2(db, legacySql, -1, &leg, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(leg, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(leg, 2, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(leg, 3, timeStr.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(leg);
            sqlite3_finalize(leg);
        }
        return ok;
    }

    bool unblockUser(const std::string& myId, const std::string& targetId) {
        if (myId.empty() || targetId.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* delSql = "DELETE FROM user_blocks WHERE blocker_id = ? AND blocked_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, delSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // Gỡ bản ghi BLOCKED legacy; nếu trước đó là bạn bè thì khôi phục FRIEND cả 2 chiều
        const char* delFriendBlock = "DELETE FROM friendships WHERE user_id_1 = ? AND user_id_2 = ? AND status = 'BLOCKED';";
        sqlite3_stmt* d1 = nullptr;
        if (sqlite3_prepare_v2(db, delFriendBlock, -1, &d1, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(d1, 1, myId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(d1, 2, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(d1);
            sqlite3_finalize(d1);
        }
        // Khôi phục FRIEND nếu chiều ngược còn FRIEND hoặc từng là bạn
        const char* checkFriend = "SELECT 1 FROM friendships WHERE user_id_1 = ? AND user_id_2 = ? AND status = 'FRIEND' LIMIT 1;";
        sqlite3_stmt* cStmt = nullptr;
        bool otherSideFriend = false;
        if (sqlite3_prepare_v2(db, checkFriend, -1, &cStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(cStmt, 1, targetId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(cStmt, 2, myId.c_str(), -1, SQLITE_STATIC);
            otherSideFriend = sqlite3_step(cStmt) == SQLITE_ROW;
            sqlite3_finalize(cStmt);
        }
        if (otherSideFriend) {
            std::string timeStr = getCurrentTimeStr();
            const char* restore = "INSERT OR REPLACE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES (?, ?, 'FRIEND', ?);";
            sqlite3_stmt* rStmt = nullptr;
            if (sqlite3_prepare_v2(db, restore, -1, &rStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(rStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(rStmt, 2, targetId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(rStmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(rStmt);
                sqlite3_finalize(rStmt);
            }
        }
        return true;
    }

    // Phiên bản nội bộ (caller đã giữ dbMutex)
    bool isUserBlockedUnlocked(const std::string& blockerId, const std::string& blockedId) {
        if (blockerId.empty() || blockedId.empty()) return false;
        const char* sql = "SELECT 1 FROM user_blocks WHERE blocker_id = ? AND blocked_id = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            const char* leg = "SELECT 1 FROM friendships WHERE user_id_1 = ? AND user_id_2 = ? AND status = 'BLOCKED' LIMIT 1;";
            if (sqlite3_prepare_v2(db, leg, -1, &stmt, nullptr) != SQLITE_OK) return false;
        }
        sqlite3_bind_text(stmt, 1, blockerId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, blockedId.c_str(), -1, SQLITE_STATIC);
        const bool blocked = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return blocked;
    }

    bool isUserBlocked(const std::string& blockerId, const std::string& blockedId) {
        if (blockerId.empty() || blockedId.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        return isUserBlockedUnlocked(blockerId, blockedId);
    }

    // Kiểm tra chặn hai chiều: A chặn B hoặc B chặn A đều trả về true
    bool areUsersBlockedEitherWay(const std::string& userA, const std::string& userB) {
        if (userA.empty() || userB.empty() || userA == userB) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        return isUserBlockedUnlocked(userA, userB) || isUserBlockedUnlocked(userB, userA);
    }

    json getBlockedUsers(const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT u.id, u.fullname, u.avatar, u.user_token, b.created_at
            FROM user_blocks b
            JOIN users u ON u.id = b.blocked_id
            WHERE b.blocker_id = ?
            ORDER BY b.created_at DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            // fallback legacy friendships BLOCKED
            const char* leg = R"(
                SELECT u.id, u.fullname, u.avatar, u.user_token, f.created_at
                FROM friendships f JOIN users u ON u.id = f.user_id_2
                WHERE f.user_id_1 = ? AND f.status = 'BLOCKED'
                ORDER BY f.created_at DESC;
            )";
            if (sqlite3_prepare_v2(db, leg, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        }
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            arr.push_back({
                {"id", (const char*)sqlite3_column_text(stmt, 0)},
                {"name", (const char*)sqlite3_column_text(stmt, 1)},
                {"avatar", sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : ""},
                {"user_token", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : ""},
                {"blocked_at", sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : ""}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
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

    void populateCommentReactions(Comment& comment, const std::string& currentUserId) {
        const char* sql = R"(
            SELECT r.type, u.fullname
            FROM comment_reactions r
            JOIN users u ON u.id = r.user_id
            WHERE r.comment_id = ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        std::map<std::string, std::vector<std::string>> reactionUsers;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, comment.id.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const auto* typeText = sqlite3_column_text(stmt, 0);
                const auto* nameText = sqlite3_column_text(stmt, 1);
                if (!typeText) continue;
                reactionUsers[(const char*)typeText].push_back(nameText ? (const char*)nameText : "");
                ++comment.total_reactions;
            }
            sqlite3_finalize(stmt);
        }

        for (const auto& entry : reactionUsers) {
            TopReaction reaction;
            reaction.type = entry.first;
            reaction.icon = getReactionIcon(entry.first);
            reaction.count = static_cast<int>(entry.second.size());
            reaction.user_names = entry.second;
            comment.top_reactions.push_back(reaction);
        }
        std::sort(comment.top_reactions.begin(), comment.top_reactions.end(),
                  [](const TopReaction& left, const TopReaction& right) {
                      return left.count > right.count;
                  });

        const char* myReactionSql = "SELECT type FROM comment_reactions WHERE comment_id = ? AND user_id = ?;";
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, myReactionSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, comment.id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_text(stmt, 0)) {
                comment.my_reaction = (const char*)sqlite3_column_text(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        for (auto& reply : comment.replies) {
            populateCommentReactions(reply, currentUserId);
        }
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
                   u.avatar, c.content, c.reply_to_id, c.reply_to_author, c.created_at, COALESCE(u.user_token, '')
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
                c.author_token = sqlite3_column_text(cStmt, 9) ? (const char*)sqlite3_column_text(cStmt, 9) : "";
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
        for (auto& comment : p.comments) {
            populateCommentReactions(comment, currentUserId);
        }

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

    bool setCommentReaction(const std::string& commentId, const std::string& userId, const std::string& type) {
        static const std::set<std::string> allowedTypes = {"LIKE", "LOVE", "HAHA", "WOW", "SAD", "ANGRY"};
        if (commentId.empty() || userId.empty() || (type != "NONE" && !allowedTypes.count(type))) return false;

        std::lock_guard<std::mutex> lock(dbMutex);
        if (type == "NONE") {
            const char* sql = "DELETE FROM comment_reactions WHERE comment_id = ? AND user_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_text(stmt, 1, commentId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            const int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            return rc == SQLITE_DONE;
        }

        const char* sql = R"(
            INSERT OR REPLACE INTO comment_reactions (comment_id, user_id, type, created_at)
            SELECT ?, ?, ?, ?
            WHERE EXISTS (SELECT 1 FROM comments WHERE id = ?);
        )";
        const std::string createdAt = getCurrentTimeStr();
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, commentId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, createdAt.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, commentId.c_str(), -1, SQLITE_STATIC);
        const int rc = sqlite3_step(stmt);
        const bool changed = rc == SQLITE_DONE && sqlite3_changes(db) > 0;
        sqlite3_finalize(stmt);
        return changed;
    }

    json getCommentReactionsDetail(const std::string& commentId, const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json counts = {{"ALL", 0}, {"LIKE", 0}, {"LOVE", 0}, {"HAHA", 0}, {"WOW", 0}, {"SAD", 0}, {"ANGRY", 0}};
        json users = json::array();
        const char* sql = R"(
            SELECT r.type, u.id, u.fullname, u.avatar, u.user_token
            FROM comment_reactions r
            JOIN users u ON u.id = r.user_id
            WHERE r.comment_id = ?
            ORDER BY r.created_at DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, commentId.c_str(), -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const auto* typeText = sqlite3_column_text(stmt, 0);
                const auto* idText = sqlite3_column_text(stmt, 1);
                const auto* nameText = sqlite3_column_text(stmt, 2);
                if (!typeText || !idText) continue;
                const std::string type = (const char*)typeText;
                const std::string userId = (const char*)idText;
                const std::string name = nameText ? (const char*)nameText : "Người dùng";
                const std::string avatar = sqlite3_column_text(stmt, 3)
                    ? (const char*)sqlite3_column_text(stmt, 3) : "/img/default-avatar.svg";
                const std::string token = sqlite3_column_text(stmt, 4)
                    ? (const char*)sqlite3_column_text(stmt, 4) : userId;
                counts["ALL"] = counts["ALL"].get<int>() + 1;
                if (counts.contains(type)) counts[type] = counts[type].get<int>() + 1;

                std::string relation = userId == myId ? "ME" : "NONE";
                if (!myId.empty() && relation == "NONE") {
                    const char* friendshipSql = R"(
                        SELECT 1 FROM friendships
                        WHERE status = 'FRIEND'
                          AND ((user_id_1 = ? AND user_id_2 = ?) OR (user_id_1 = ? AND user_id_2 = ?))
                        LIMIT 1;
                    )";
                    sqlite3_stmt* friendshipStmt = nullptr;
                    if (sqlite3_prepare_v2(db, friendshipSql, -1, &friendshipStmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(friendshipStmt, 1, myId.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(friendshipStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(friendshipStmt, 3, userId.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(friendshipStmt, 4, myId.c_str(), -1, SQLITE_STATIC);
                        if (sqlite3_step(friendshipStmt) == SQLITE_ROW) relation = "FRIEND";
                        sqlite3_finalize(friendshipStmt);
                    }
                }

                users.push_back({
                    {"type", type}, {"icon", getReactionIcon(type)}, {"user_id", userId},
                    {"name", name}, {"avatar", avatar}, {"user_token", token}, {"relation", relation}
                });
            }
            sqlite3_finalize(stmt);
        }
        return {{"status", "ok"}, {"counts", counts}, {"users", users}, {"total", counts["ALL"]}};
    }

    static std::vector<std::string> parseCategoryTags(const std::string& rawTags) {
        static const std::vector<std::string> allowed = {
            "giai_tri", "hoc_tap", "cong_nghe", "doi_song", "the_thao", "khac"
        };
        std::vector<std::string> tags;
        std::stringstream stream(rawTags);
        std::string tag;
        while (std::getline(stream, tag, '|')) {
            if (std::find(allowed.begin(), allowed.end(), tag) != allowed.end() &&
                std::find(tags.begin(), tags.end(), tag) == tags.end()) {
                tags.push_back(tag);
            }
        }
        if (tags.empty()) tags.push_back("doi_song");
        return tags;
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
                SELECT COALESCE(p.category, 'doi_song') as cat
                FROM reactions r JOIN posts p ON r.post_id = p.id
                WHERE r.user_id = ?
                UNION ALL
                SELECT COALESCE(p.category, 'doi_song')
                FROM comments c JOIN posts p ON c.post_id = p.id
                WHERE c.author_id = ?
            )";
            sqlite3_stmt* aStmt = nullptr;
            if (sqlite3_prepare_v2(db, aSql, -1, &aStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(aStmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(aStmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
                while (sqlite3_step(aStmt) == SQLITE_ROW) {
                    std::string rawTags = sqlite3_column_text(aStmt, 0)
                        ? (const char*)sqlite3_column_text(aStmt, 0) : "doi_song";
                    for (const auto& tag : parseCategoryTags(rawTags)) {
                        categoryAffinity[tag] += 1;
                    }
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
              AND p.author_id NOT IN (
                    SELECT blocked_id FROM user_blocks WHERE blocker_id = ?
                    UNION
                    SELECT user_id_2 FROM friendships WHERE user_id_1 = ? AND status = 'BLOCKED'
              )
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

        sqlite3_bind_text(stmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC); // my_reaction
        sqlite3_bind_text(stmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC); // block filter 1
        sqlite3_bind_text(stmt, 3, currentUserId.c_str(), -1, SQLITE_STATIC); // block filter 2
        sqlite3_bind_text(stmt, 4, currentUserId.c_str(), -1, SQLITE_STATIC); // author self
        sqlite3_bind_text(stmt, 5, currentUserId.c_str(), -1, SQLITE_STATIC); // friends 1
        sqlite3_bind_text(stmt, 6, currentUserId.c_str(), -1, SQLITE_STATIC); // friends 2

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
            std::string rawCategories = sqlite3_column_text(stmt, 15)
                ? (const char*)sqlite3_column_text(stmt, 15) : "doi_song";
            p.categories = parseCategoryTags(rawCategories);
            p.category = p.categories.front();
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
            for (const auto& tag : p.categories) {
                auto itAff = categoryAffinity.find(tag);
                if (itAff != categoryAffinity.end()) {
                    interestScore = std::max(interestScore,
                        (double)itAff->second / (double)maxAffinity);
                }
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
        // Admin không được đăng bài như người dùng thường
        {
            const char* q = "SELECT role FROM users WHERE id = ? LIMIT 1;";
            sqlite3_stmt* s = nullptr;
            if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s, 1, authorId.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0) &&
                    std::string((const char*)sqlite3_column_text(s, 0)) == "ADMIN") {
                    sqlite3_finalize(s);
                    return false;
                }
                sqlite3_finalize(s);
            }
        }
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

    bool canAccessDirectConversation(const std::string& userId, const std::string& partnerId) {
        if (userId.empty() || partnerId.empty() || userId == partnerId) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const std::string conversationId = getConversationId(userId, partnerId);
        const char* sql = R"(
            SELECT 1
            WHERE EXISTS (
                SELECT 1 FROM messages
                WHERE conversation_id = ?
                  AND ((sender_id = ? AND recipient_id = ?) OR (sender_id = ? AND recipient_id = ?))
            ) OR EXISTS (
                SELECT 1 FROM friendships
                WHERE status = 'FRIEND'
                  AND ((user_id_1 = ? AND user_id_2 = ?) OR (user_id_1 = ? AND user_id_2 = ?))
            )
            LIMIT 1;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, conversationId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, partnerId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, partnerId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, partnerId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, partnerId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, userId.c_str(), -1, SQLITE_STATIC);
        const bool allowed = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return allowed;
    }

    bool isGroupMember(const std::string& groupId, const std::string& userId) {
        if (groupId.empty() || userId.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        const bool isMember = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return isMember;
    }

    bool markDirectMessagesRead(const std::string& userId, const std::string& partnerId,
                                const std::vector<int>& messageIds, int& markedCount) {
        markedCount = 0;
        if (userId.empty() || partnerId.empty() || userId == partnerId) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const std::string conversationId = getConversationId(userId, partnerId);
        const std::string now = getCurrentTimeStr();
        const char* receiptSql = R"(
            INSERT OR IGNORE INTO direct_message_receipts (message_id, user_id, delivered_at, read_at)
            SELECT id, ?, ?, ? FROM messages
            WHERE id = ? AND conversation_id = ? AND sender_id = ? AND recipient_id = ?;
        )";
        const char* updateReceiptSql = R"(
            UPDATE direct_message_receipts
            SET delivered_at = CASE WHEN delivered_at = '' THEN ? ELSE delivered_at END,
                read_at = CASE WHEN read_at = '' THEN ? ELSE read_at END
            WHERE message_id = ? AND user_id = ?
              AND EXISTS (
                SELECT 1 FROM messages
                WHERE id = ? AND conversation_id = ? AND sender_id = ? AND recipient_id = ?
              );
        )";
        const char* legacySql = "UPDATE messages SET is_read = 1 WHERE id = ? AND conversation_id = ? AND sender_id = ? AND recipient_id = ?;";
        sqlite3_stmt* receiptStmt = nullptr;
        sqlite3_stmt* updateReceiptStmt = nullptr;
        sqlite3_stmt* legacyStmt = nullptr;
        if (sqlite3_prepare_v2(db, receiptSql, -1, &receiptStmt, nullptr) != SQLITE_OK ||
            sqlite3_prepare_v2(db, updateReceiptSql, -1, &updateReceiptStmt, nullptr) != SQLITE_OK ||
            sqlite3_prepare_v2(db, legacySql, -1, &legacyStmt, nullptr) != SQLITE_OK) {
            if (receiptStmt) sqlite3_finalize(receiptStmt);
            if (updateReceiptStmt) sqlite3_finalize(updateReceiptStmt);
            if (legacyStmt) sqlite3_finalize(legacyStmt);
            return false;
        }

        for (const int messageId : messageIds) {
            if (messageId <= 0) continue;
            sqlite3_reset(receiptStmt);
            sqlite3_clear_bindings(receiptStmt);
            sqlite3_bind_text(receiptStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(receiptStmt, 2, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(receiptStmt, 3, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(receiptStmt, 4, messageId);
            sqlite3_bind_text(receiptStmt, 5, conversationId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(receiptStmt, 6, partnerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(receiptStmt, 7, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(receiptStmt) != SQLITE_DONE) continue;

            sqlite3_reset(updateReceiptStmt);
            sqlite3_clear_bindings(updateReceiptStmt);
            sqlite3_bind_text(updateReceiptStmt, 1, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateReceiptStmt, 2, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(updateReceiptStmt, 3, messageId);
            sqlite3_bind_text(updateReceiptStmt, 4, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(updateReceiptStmt, 5, messageId);
            sqlite3_bind_text(updateReceiptStmt, 6, conversationId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateReceiptStmt, 7, partnerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateReceiptStmt, 8, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(updateReceiptStmt) != SQLITE_DONE || sqlite3_changes(db) == 0) continue;
            ++markedCount;

            sqlite3_reset(legacyStmt);
            sqlite3_clear_bindings(legacyStmt);
            sqlite3_bind_int(legacyStmt, 1, messageId);
            sqlite3_bind_text(legacyStmt, 2, conversationId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(legacyStmt, 3, partnerId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(legacyStmt, 4, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(legacyStmt);
        }
        sqlite3_finalize(receiptStmt);
        sqlite3_finalize(updateReceiptStmt);
        sqlite3_finalize(legacyStmt);
        return true;
    }

    bool markGroupMessagesRead(const std::string& groupId, const std::string& userId,
                               const std::vector<int>& messageIds, int& markedCount) {
        markedCount = 0;
        std::lock_guard<std::mutex> lock(dbMutex);
        sqlite3_stmt* memberStmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ? LIMIT 1;", -1, &memberStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(memberStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(memberStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        const bool isMember = sqlite3_step(memberStmt) == SQLITE_ROW;
        sqlite3_finalize(memberStmt);
        if (!isMember) return false;

        const std::string now = getCurrentTimeStr();
        const char* insertSql = R"(
            INSERT OR IGNORE INTO group_message_receipts (group_message_id, user_id, delivered_at, read_at)
            SELECT gm.id, ?, ?, ? FROM group_messages gm
            WHERE gm.id = ? AND gm.group_id = ? AND gm.sender_id <> ?
              AND EXISTS (SELECT 1 FROM group_members WHERE group_id = gm.group_id AND user_id = ?);
        )";
        const char* updateSql = R"(
            UPDATE group_message_receipts
            SET delivered_at = CASE WHEN delivered_at = '' THEN ? ELSE delivered_at END,
                read_at = CASE WHEN read_at = '' THEN ? ELSE read_at END
            WHERE group_message_id = ? AND user_id = ?
              AND EXISTS (
                SELECT 1 FROM group_messages gm
                JOIN group_members gmbr ON gmbr.group_id = gm.group_id AND gmbr.user_id = ?
                WHERE gm.id = ? AND gm.group_id = ? AND gm.sender_id <> ?
              );
        )";
        sqlite3_stmt* insertStmt = nullptr;
        sqlite3_stmt* updateStmt = nullptr;
        if (sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK ||
            sqlite3_prepare_v2(db, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
            if (insertStmt) sqlite3_finalize(insertStmt);
            if (updateStmt) sqlite3_finalize(updateStmt);
            return false;
        }

        for (const int messageId : messageIds) {
            if (messageId <= 0) continue;
            sqlite3_reset(insertStmt);
            sqlite3_clear_bindings(insertStmt);
            sqlite3_bind_text(insertStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 2, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 3, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(insertStmt, 4, messageId);
            sqlite3_bind_text(insertStmt, 5, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 6, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 7, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(insertStmt) != SQLITE_DONE) continue;

            sqlite3_reset(updateStmt);
            sqlite3_clear_bindings(updateStmt);
            sqlite3_bind_text(updateStmt, 1, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateStmt, 2, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(updateStmt, 3, messageId);
            sqlite3_bind_text(updateStmt, 4, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateStmt, 5, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(updateStmt, 6, messageId);
            sqlite3_bind_text(updateStmt, 7, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(updateStmt, 8, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(updateStmt) == SQLITE_DONE && sqlite3_changes(db) > 0) ++markedCount;
        }
        sqlite3_finalize(insertStmt);
        sqlite3_finalize(updateStmt);
        return true;
    }

    std::vector<Message> getPaginatedMessages(const std::string& user1, const std::string& user2, int beforeId, int limit, bool& hasMore) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(user1, user2);
        std::vector<Message> messages;
        hasMore = false;

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

        for (auto& message : messages) {
            if (message.recipient_id == user1 && message.sender_id != user1) {
                recordDirectMessageDelivered(message.id, user1);
            }
            message.receipts = getMessageReceiptDetails(message.id, false);
            message.reactions = getMessageReactionSummaries(message.id, false);
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
        // Không cho gửi tin nếu một trong hai bên đã chặn bên kia
        if (isUserBlockedUnlocked(senderId, recipientId) || isUserBlockedUnlocked(recipientId, senderId)) {
            return false;
        }
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
            if (PresenceRegistry::instance().isOnline(recipientId)) {
                recordDirectMessageDelivered(outMsg.id, recipientId);
            }
            outMsg.receipts = getMessageReceiptDetails(outMsg.id, false);
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

    bool setConversationArchived(const std::string& userId, const std::string& conversationId, bool isGroup, bool archived) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const std::string key = (isGroup ? "G:" : "D:") + (isGroup ? conversationId : getConversationId(userId, conversationId));
        const char* sql = R"(
            INSERT INTO chat_preferences (user_id, conversation_key, is_archived)
            VALUES (?, ?, ?)
            ON CONFLICT(user_id, conversation_key) DO UPDATE SET is_archived = excluded.is_archived;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, archived ? 1 : 0);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool setConversationForcedUnread(const std::string& userId, const std::string& conversationId, bool isGroup, bool unread) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const std::string key = (isGroup ? "G:" : "D:") + (isGroup ? conversationId : getConversationId(userId, conversationId));
        const char* sql = R"(
            INSERT INTO chat_preferences (user_id, conversation_key, force_unread)
            VALUES (?, ?, ?)
            ON CONFLICT(user_id, conversation_key) DO UPDATE SET force_unread = excluded.force_unread;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, unread ? 1 : 0);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool isConversationArchived(const std::string& userId, const std::string& conversationId, bool isGroup) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const std::string key = (isGroup ? "G:" : "D:") + (isGroup ? conversationId : getConversationId(userId, conversationId));
        const char* sql = "SELECT is_archived FROM chat_preferences WHERE user_id = ? AND conversation_key = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);
        const bool archived = sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) != 0;
        sqlite3_finalize(stmt);
        return archived;
    }

    int getGroupUnreadCount(const std::string& groupId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = R"(
            SELECT COUNT(*) FROM group_messages gm
            WHERE gm.group_id = ?
              AND gm.sender_id != ?
              AND COALESCE(gm.is_revoked, 0) = 0
              AND NOT EXISTS (
                  SELECT 1 FROM group_messages_deleted d
                  WHERE d.message_id = gm.id AND d.user_id = ?
              )
              AND NOT EXISTS (
                  SELECT 1 FROM group_message_receipts r
                  WHERE r.group_message_id = gm.id AND r.user_id = ? AND r.read_at != ''
              );
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, userId.c_str(), -1, SQLITE_STATIC);
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return count;
    }

    bool isConversationForcedUnread(const std::string& userId, const std::string& conversationId, bool isGroup) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const std::string key = (isGroup ? "G:" : "D:") + (isGroup ? conversationId : getConversationId(userId, conversationId));
        const char* sql = "SELECT force_unread FROM chat_preferences WHERE user_id = ? AND conversation_key = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);
        const bool unread = sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) != 0;
        sqlite3_finalize(stmt);
        return unread;
    }

    json getFriendsWithLastMessage(const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        syncPendingDeliveriesForUserLocked(myId);
        json arr = json::array();

        const char* sql = R"(
            SELECT u.id, u.fullname, u.avatar, u.last_active_at, u.user_token,
                   (SELECT CASE WHEN m.is_revoked = 1 THEN 'Tin nhắn đã được thu hồi' ELSE m.content END FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_msg,
                   (SELECT m.created_at FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_msg_time,
                   (SELECT COUNT(*) FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END) AND m.recipient_id = ?1 AND m.is_read = 0) as unread_count,
                   (SELECT nickname FROM user_nicknames WHERE user_id = ?1 AND friend_id = u.id) as nickname,
                   (SELECT m.sender_id FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_sender_id,
                   (SELECT m.is_read FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_message_read,
                   COALESCE(cp.is_archived, 0) as is_archived,
                   COALESCE(cp.force_unread, 0) as force_unread,
                   (SELECT COALESCE(m.media_type, 'NONE') FROM messages m WHERE m.conversation_id = (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END) ORDER BY m.id DESC LIMIT 1) as last_message_media_type
            FROM friendships f
            JOIN users u ON f.user_id_2 = u.id
            LEFT JOIN chat_preferences cp ON cp.user_id = ?1 AND cp.conversation_key = 'D:' || (CASE WHEN u.id < ?1 THEN u.id||'_'||?1 ELSE ?1||'_'||u.id END)
            WHERE f.user_id_1 = ?1 AND f.status = 'FRIEND' AND u.role != 'ADMIN'
            ORDER BY last_msg_time DESC NULLS LAST;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;

        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);

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
            bool isUnread = sqlite3_column_int(stmt, 7) > 0 || sqlite3_column_int(stmt, 12) != 0;
            std::string nickname = sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "";
            std::string lastSenderId = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            bool lastMessageRead = sqlite3_column_int(stmt, 10) != 0;
            bool isArchived = sqlite3_column_int(stmt, 11) != 0;

            bool isOnline = PresenceRegistry::instance().isOnline(fid);

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
                {"is_unread", isUnread},
                {"last_message_is_mine", lastSenderId == myId},
                {"last_message_sender_id", lastSenderId},
                {"is_seen_by_partner", lastSenderId == myId && lastMessageRead},
                {"last_message_media_type", sqlite3_column_text(stmt, 13) ? (const char*)sqlite3_column_text(stmt, 13) : "NONE"},
                {"is_archived", isArchived}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    json getPendingMessageContacts(const std::string& myId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT u.id, u.fullname, u.avatar, u.user_token,
                   (SELECT CASE WHEN m.is_revoked = 1 THEN 'Tin nhắn đã được thu hồi' ELSE m.content END
                    FROM messages m
                    WHERE m.sender_id = u.id AND m.recipient_id = ?1
                    ORDER BY m.id DESC LIMIT 1) AS last_msg,
                   (SELECT m.created_at FROM messages m
                    WHERE m.sender_id = u.id AND m.recipient_id = ?1
                    ORDER BY m.id DESC LIMIT 1) AS last_msg_time,
                   (SELECT COALESCE(m.media_type, 'NONE') FROM messages m
                    WHERE m.sender_id = u.id AND m.recipient_id = ?1
                    ORDER BY m.id DESC LIMIT 1) AS last_media_type,
                   (SELECT COUNT(*) FROM messages m
                    WHERE m.sender_id = u.id AND m.recipient_id = ?1 AND m.is_read = 0) AS unread_count
            FROM users u
            WHERE u.id != ?1 AND u.role != 'ADMIN'
              AND EXISTS (SELECT 1 FROM messages m WHERE m.sender_id = u.id AND m.recipient_id = ?1)
              AND NOT EXISTS (
                  SELECT 1 FROM friendships f
                  WHERE ((f.user_id_1 = ?1 AND f.user_id_2 = u.id)
                      OR (f.user_id_1 = u.id AND f.user_id_2 = ?1))
                    AND f.status IN ('FRIEND', 'BLOCKED')
              )
            ORDER BY (
                SELECT MAX(m.id) FROM messages m
                WHERE m.sender_id = u.id AND m.recipient_id = ?1
            ) DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto textColumn = [stmt](int column, const std::string& fallback = "") -> std::string {
                const unsigned char* text = sqlite3_column_text(stmt, column);
                return text ? reinterpret_cast<const char*>(text) : fallback;
            };
            const std::string userId = textColumn(0);
            arr.push_back({
                {"id", userId},
                {"name", textColumn(1, "Người dùng")},
                {"avatar", textColumn(2, "/img/default-avatar.svg")},
                {"user_token", textColumn(3, userId)},
                {"last_message", textColumn(4)},
                {"last_message_time", textColumn(5)},
                {"last_message_media_type", textColumn(6, "NONE")},
                {"is_unread", sqlite3_column_int(stmt, 7) > 0}
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
            // BẢO MẬT: bản cũ ghép chuỗi mid (do client gửi) vào SQL chạy bằng sqlite3_exec -> SQL injection
            // (ví dụ chèn thêm UPDATE users SET role='ADMIN'...). Nay dùng prepared statement + kiểm tra user tồn tại.
            auto addMember = [&](const std::string& uid, const char* role, bool requireExists) {
                const char* msql = requireExists
                    ? "INSERT OR IGNORE INTO group_members (group_id, user_id, role, joined_at) SELECT ?, id, ?, ? FROM users WHERE id = ? AND role != 'ADMIN';"
                    : "INSERT OR IGNORE INTO group_members (group_id, user_id, role, joined_at) VALUES (?, ?, ?, ?);";
                sqlite3_stmt* ms = nullptr;
                if (sqlite3_prepare_v2(db, msql, -1, &ms, nullptr) != SQLITE_OK) return;
                sqlite3_bind_text(ms, 1, groupId.c_str(), -1, SQLITE_STATIC);
                if (requireExists) {
                    sqlite3_bind_text(ms, 2, role, -1, SQLITE_STATIC);
                    sqlite3_bind_text(ms, 3, timeStr.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(ms, 4, uid.c_str(), -1, SQLITE_STATIC);
                } else {
                    sqlite3_bind_text(ms, 2, uid.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(ms, 3, role, -1, SQLITE_STATIC);
                    sqlite3_bind_text(ms, 4, timeStr.c_str(), -1, SQLITE_STATIC);
                }
                sqlite3_step(ms);
                sqlite3_finalize(ms);
            };
            addMember(createdBy, "ADMIN", false);
            size_t added = 0;
            for (const auto& mid : memberIds) {
                if (mid == createdBy) continue;
                if (++added > 200) break;  // giới hạn số thành viên khi tạo nhóm
                addMember(mid, "MEMBER", true);
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
        syncPendingDeliveriesForUserLocked(userId);
        std::vector<Group> groups;
        const char* sql = R"(
            SELECT g.id, g.name, g.description, g.avatar, g.created_by, g.created_at,
                   (SELECT COUNT(*) FROM group_members gm WHERE gm.group_id = g.id) as m_count,
                   (SELECT CASE WHEN gm2.is_revoked = 1 THEN 'Tin nhắn đã được thu hồi' ELSE gm2.content END
                    FROM group_messages gm2
                    WHERE gm2.group_id = g.id
                      AND NOT EXISTS (SELECT 1 FROM group_messages_deleted deleted WHERE deleted.user_id = ?1 AND deleted.message_id = gm2.id)
                    ORDER BY gm2.id DESC LIMIT 1) as last_msg,
                   (SELECT gm2.created_at FROM group_messages gm2
                    WHERE gm2.group_id = g.id
                      AND NOT EXISTS (SELECT 1 FROM group_messages_deleted deleted WHERE deleted.user_id = ?1 AND deleted.message_id = gm2.id)
                    ORDER BY gm2.id DESC LIMIT 1) as last_msg_time,
                   (SELECT u2.fullname FROM group_messages gm2 JOIN users u2 ON u2.id = gm2.sender_id
                    WHERE gm2.group_id = g.id
                      AND NOT EXISTS (SELECT 1 FROM group_messages_deleted deleted WHERE deleted.user_id = ?1 AND deleted.message_id = gm2.id)
                    ORDER BY gm2.id DESC LIMIT 1) as last_sender_name,
                   (SELECT gm2.sender_id FROM group_messages gm2
                    WHERE gm2.group_id = g.id
                      AND NOT EXISTS (SELECT 1 FROM group_messages_deleted deleted WHERE deleted.user_id = ?1 AND deleted.message_id = gm2.id)
                    ORDER BY gm2.id DESC LIMIT 1) as last_sender_id,
                   (SELECT COALESCE(gm2.media_type, 'NONE') FROM group_messages gm2
                    WHERE gm2.group_id = g.id
                      AND NOT EXISTS (SELECT 1 FROM group_messages_deleted deleted WHERE deleted.user_id = ?1 AND deleted.message_id = gm2.id)
                    ORDER BY gm2.id DESC LIMIT 1) as last_media_type
            FROM groups g
            JOIN group_members gm ON g.id = gm.group_id
            WHERE gm.user_id = ?1
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
            g.last_message_sender = sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "";
            g.last_message_sender_id = sqlite3_column_text(stmt, 10) ? (const char*)sqlite3_column_text(stmt, 10) : "";
            g.last_message_media_type = sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "NONE";
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

    bool addGroupMembers(const std::string& groupId, const std::vector<std::string>& userIds,
                        const std::string& requesterId, std::vector<std::string>& addedNames) {
        std::lock_guard<std::mutex> lock(dbMutex);
        addedNames.clear();
        if (groupId.empty() || requesterId.empty() || userIds.empty()) return false;

        sqlite3_stmt* memberStmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?;", -1, &memberStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(memberStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(memberStmt, 2, requesterId.c_str(), -1, SQLITE_STATIC);
        const bool requesterIsMember = sqlite3_step(memberStmt) == SQLITE_ROW;
        sqlite3_finalize(memberStmt);
        if (!requesterIsMember) return false;

        char* errorMessage = nullptr;
        if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errorMessage) != SQLITE_OK) {
            sqlite3_free(errorMessage);
            return false;
        }

        bool ok = true;
        for (const auto& userId : userIds) {
            if (userId.empty() || userId == requesterId) continue;

            sqlite3_stmt* friendshipStmt = nullptr;
            const char* friendshipSql = R"(
                SELECT 1 FROM friendships
                WHERE status = 'FRIEND'
                  AND ((user_id_1 = ? AND user_id_2 = ?) OR (user_id_1 = ? AND user_id_2 = ?))
                LIMIT 1;
            )";
            if (sqlite3_prepare_v2(db, friendshipSql, -1, &friendshipStmt, nullptr) != SQLITE_OK) {
                ok = false;
                break;
            }
            sqlite3_bind_text(friendshipStmt, 1, requesterId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(friendshipStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(friendshipStmt, 3, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(friendshipStmt, 4, requesterId.c_str(), -1, SQLITE_STATIC);
            const bool areFriends = sqlite3_step(friendshipStmt) == SQLITE_ROW;
            sqlite3_finalize(friendshipStmt);
            if (!areFriends) {
                ok = false;
                break;
            }

            sqlite3_stmt* userStmt = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT fullname FROM users WHERE id = ?;", -1, &userStmt, nullptr) != SQLITE_OK) {
                ok = false;
                break;
            }
            sqlite3_bind_text(userStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(userStmt) != SQLITE_ROW) {
                sqlite3_finalize(userStmt);
                ok = false;
                break;
            }
            const unsigned char* nameText = sqlite3_column_text(userStmt, 0);
            const std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
            sqlite3_finalize(userStmt);

            sqlite3_stmt* insertStmt = nullptr;
            const char* insertSql = "INSERT OR IGNORE INTO group_members (group_id, user_id, role, joined_at) VALUES (?, ?, 'MEMBER', ?);";
            if (sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK) {
                ok = false;
                break;
            }
            const std::string joinedAt = getCurrentTimeStr();
            sqlite3_bind_text(insertStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 3, joinedAt.c_str(), -1, SQLITE_STATIC);
            ok = sqlite3_step(insertStmt) == SQLITE_DONE;
            sqlite3_finalize(insertStmt);
            if (!ok) break;
            if (sqlite3_changes(db) > 0) addedNames.push_back(name);
        }

        if (ok && sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errorMessage) == SQLITE_OK) {
            return !addedNames.empty();
        }
        sqlite3_free(errorMessage);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        addedNames.clear();
        return false;
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

    bool clearGroupConversationForUser(const std::string& groupId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* memberSql = "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?;";
        sqlite3_stmt* memberStmt = nullptr;
        if (sqlite3_prepare_v2(db, memberSql, -1, &memberStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(memberStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(memberStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        const bool isMember = sqlite3_step(memberStmt) == SQLITE_ROW;
        sqlite3_finalize(memberStmt);
        if (!isMember) return false;

        const char* sql = R"(
            INSERT OR IGNORE INTO group_messages_deleted (user_id, message_id)
            SELECT ?, id FROM group_messages WHERE group_id = ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, groupId.c_str(), -1, SQLITE_STATIC);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
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

            // Đánh dấu đã nhận ngay cho các thành viên trong nhóm đang online
            sqlite3_stmt* mStmt = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT user_id FROM group_members WHERE group_id = ? AND user_id <> ?;", -1, &mStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(mStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(mStmt, 2, senderId.c_str(), -1, SQLITE_STATIC);
                std::vector<std::string> onlineMembers;
                while (sqlite3_step(mStmt) == SQLITE_ROW) {
                    const char* mid = (const char*)sqlite3_column_text(mStmt, 0);
                    if (mid && PresenceRegistry::instance().isOnline(mid)) {
                        onlineMembers.push_back(mid);
                    }
                }
                sqlite3_finalize(mStmt);
                for (const auto& mid : onlineMembers) {
                    recordGroupMessageDelivered(outMsg.id, groupId, mid);
                }
            }
            outMsg.receipts = getMessageReceiptDetails(outMsg.id, true);
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

        for (auto& message : list) {
            if (!viewerId.empty() && message.sender_id != viewerId) {
                recordGroupMessageDelivered(message.id, groupId, viewerId);
            }
            message.receipts = getMessageReceiptDetails(message.id, true);
            message.reactions = getMessageReactionSummaries(message.id, true);
        }

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
            SELECT DISTINCT u.id, u.fullname, u.avatar, u.user_token
            FROM friendships f
            JOIN users u ON u.id = CASE WHEN f.user_id_1 = ? THEN f.user_id_2 ELSE f.user_id_1 END
            WHERE f.status = 'FRIEND'
              AND (f.user_id_1 = ? OR f.user_id_2 = ?)
              AND u.id NOT IN (SELECT user_id FROM group_members WHERE group_id = ?);
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, myId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, groupId.c_str(), -1, SQLITE_STATIC);
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

    std::vector<Notification> getUserNotifications(const std::string& userId, int limit = 15, int offset = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<Notification> list;
        const char* sql = R"(
            SELECT n.id, n.user_id, n.actor_id, u.fullname, u.avatar, n.type, n.content, n.link, n.is_read, n.created_at
            FROM notifications n
            JOIN users u ON n.actor_id = u.id
            WHERE n.user_id = ?
            ORDER BY n.id DESC LIMIT ? OFFSET ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;

        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, std::max(1, limit));
        sqlite3_bind_int(stmt, 3, std::max(0, offset));
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

    int getUnreadNotificationCount(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT COUNT(*) FROM notifications WHERE user_id = ? AND is_read = 0;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        int count = sqlite3_step(stmt) == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : 0;
        sqlite3_finalize(stmt);
        return count;
    }

    bool deleteNotification(int notifId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM notifications WHERE id = ? AND user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, notifId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        bool deleted = rc == SQLITE_DONE && sqlite3_changes(db) > 0;
        sqlite3_finalize(stmt);
        return deleted;
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

    bool reactMessage(int msgId, const std::string& userId, const std::string& reaction) {
        std::lock_guard<std::mutex> lock(dbMutex);
        if (msgId <= 0 || userId.empty() || reaction.empty()) return false;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM messages WHERE id = ? AND (sender_id = ? OR recipient_id = ?);", -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, userId.c_str(), -1, SQLITE_STATIC);
        const bool canReact = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        if (!canReact) return false;

        const char* deleteSql = "DELETE FROM message_reactions WHERE message_id = ? AND user_id = ? AND emoji = ?;";
        if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, reaction.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        const bool removed = sqlite3_changes(db) > 0;
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) return false;
        if (removed) {
            executeSimple("UPDATE messages SET reaction = '' WHERE id = " + std::to_string(msgId) + ";");
            return true;
        }

        const char* upsertSql = "INSERT INTO message_reactions (message_id, user_id, emoji, created_at) VALUES (?, ?, ?, ?) "
                                "ON CONFLICT(message_id, user_id) DO UPDATE SET emoji = excluded.emoji, created_at = excluded.created_at;";
        if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        const std::string now = getCurrentTimeStr();
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, reaction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) executeSimple("UPDATE messages SET reaction = '' WHERE id = " + std::to_string(msgId) + ";");
        return rc == SQLITE_DONE;
    }

    bool reactGroupMessage(int msgId, const std::string& userId, const std::string& reaction) {
        std::lock_guard<std::mutex> lock(dbMutex);
        if (msgId <= 0 || userId.empty() || reaction.empty()) return false;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM group_messages gm JOIN group_members g ON g.group_id = gm.group_id WHERE gm.id = ? AND g.user_id = ?;", -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        const bool canReact = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        if (!canReact) return false;

        const char* deleteSql = "DELETE FROM group_message_reactions WHERE group_message_id = ? AND user_id = ? AND emoji = ?;";
        if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, reaction.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        const bool removed = sqlite3_changes(db) > 0;
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) return false;
        if (removed) {
            executeSimple("UPDATE group_messages SET reaction = '' WHERE id = " + std::to_string(msgId) + ";");
            return true;
        }

        const char* upsertSql = "INSERT INTO group_message_reactions (group_message_id, user_id, emoji, created_at) VALUES (?, ?, ?, ?) "
                                "ON CONFLICT(group_message_id, user_id) DO UPDATE SET emoji = excluded.emoji, created_at = excluded.created_at;";
        if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        const std::string now = getCurrentTimeStr();
        sqlite3_bind_int(stmt, 1, msgId);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, reaction.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) executeSimple("UPDATE group_messages SET reaction = '' WHERE id = " + std::to_string(msgId) + ";");
        return rc == SQLITE_DONE;
    }

    // --- SEARCH REALTIME & SEARCH PAGE (Hình 3 & Hình 4) ---
    json searchEverything(const std::string& keyword, const std::string& filter, const std::string& currentUserId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json result;
        result["users"] = json::array();
        result["groups"] = json::array();
        result["posts"] = json::array();

        std::string pattern = "%" + keyword + "%";

        // Tìm người dùng (ẩn người đã chặn)
        if (filter == "ALL" || filter == "PEOPLE") {
            const char* uSql = R"(
                SELECT u.id, u.username, u.fullname, u.avatar, u.school, u.hometown, u.user_token, u.bio,
                       CASE WHEN EXISTS (
                         SELECT 1 FROM friendships f
                         WHERE f.status = 'FRIEND'
                           AND ((f.user_id_1 = ? AND f.user_id_2 = u.id) OR (f.user_id_1 = u.id AND f.user_id_2 = ?))
                       ) THEN 1 ELSE 0 END AS is_friend,
                       CASE
                         WHEN EXISTS (SELECT 1 FROM friend_requests fr WHERE fr.requester_id = ? AND fr.receiver_id = u.id AND fr.status = 'PENDING') THEN 'SENT'
                         WHEN EXISTS (SELECT 1 FROM friend_requests fr WHERE fr.requester_id = u.id AND fr.receiver_id = ? AND fr.status = 'PENDING') THEN 'INCOMING'
                         ELSE 'NONE'
                       END AS request_status
                FROM users u
                WHERE (u.fullname LIKE ? OR u.username LIKE ? OR u.school LIKE ? OR u.hometown LIKE ?) AND u.role != 'ADMIN'
                  AND u.id != ?
                  AND u.id NOT IN (SELECT blocked_id FROM user_blocks WHERE blocker_id = ?)
                  AND u.id NOT IN (SELECT user_id_2 FROM friendships WHERE user_id_1 = ? AND status = 'BLOCKED')
                LIMIT 24;
            )";
            sqlite3_stmt* uStmt = nullptr;
            if (sqlite3_prepare_v2(db, uSql, -1, &uStmt, nullptr) == SQLITE_OK) {
                // friendship checks
                sqlite3_bind_text(uStmt, 1, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 2, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 3, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 4, currentUserId.c_str(), -1, SQLITE_STATIC);
                // LIKE patterns
                sqlite3_bind_text(uStmt, 5, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 6, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 7, pattern.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 8, pattern.c_str(), -1, SQLITE_STATIC);
                // filters
                sqlite3_bind_text(uStmt, 9, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 10, currentUserId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(uStmt, 11, currentUserId.c_str(), -1, SQLITE_STATIC);
                while (sqlite3_step(uStmt) == SQLITE_ROW) {
                    std::string uid = (const char*)sqlite3_column_text(uStmt, 0);
                    std::string uname = (const char*)sqlite3_column_text(uStmt, 1);
                    std::string fname = (const char*)sqlite3_column_text(uStmt, 2);
                    std::string av = sqlite3_column_text(uStmt, 3) ? (const char*)sqlite3_column_text(uStmt, 3) : "/img/default-avatar.svg";
                    std::string school = sqlite3_column_text(uStmt, 4) ? (const char*)sqlite3_column_text(uStmt, 4) : "";
                    std::string hometown = sqlite3_column_text(uStmt, 5) ? (const char*)sqlite3_column_text(uStmt, 5) : "";
                    std::string utoken = sqlite3_column_text(uStmt, 6) ? (const char*)sqlite3_column_text(uStmt, 6) : uid;
                    std::string bio = sqlite3_column_text(uStmt, 7) ? (const char*)sqlite3_column_text(uStmt, 7) : "";
                    bool isFriend = sqlite3_column_int(uStmt, 8) == 1;
                    std::string reqStatus = sqlite3_column_text(uStmt, 9) ? (const char*)sqlite3_column_text(uStmt, 9) : "NONE";

                    std::string subtitle = school.empty() ? hometown : school;
                    if (!school.empty() && !hometown.empty()) subtitle = school;

                    result["users"].push_back({
                        {"id", uid},
                        {"username", uname},
                        {"name", fname},
                        {"avatar", av},
                        {"user_token", utoken},
                        {"bio", bio},
                        {"school", school},
                        {"hometown", hometown},
                        {"subtitle", subtitle},
                        {"is_friend", isFriend},
                        {"request_status", reqStatus}
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
        return getReportsPaged(-1, 0, "");
    }

    // Lấy báo cáo theo trang (phục vụ cuộn vô hạn ở trang Admin).
    // limit < 0: lấy toàn bộ. keyword: lọc theo lý do, người báo cáo, người/bài bị báo cáo, trạng thái.
    std::vector<Report> getReportsPaged(int limit, int offset, const std::string& keyword) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<Report> list;
        const char* sql = R"(
            SELECT r.id, r.reporter_id, COALESCE(u1.fullname, ''), COALESCE(r.reported_user_id, ''),
                   COALESCE(u2.fullname, ''), COALESCE(r.reported_post_id, ''), COALESCE(r.reported_comment_id, ''),
                   COALESCE(r.reason, ''), COALESCE(r.status, ''), COALESCE(r.created_at, ''),
                   COALESCE(r.category, ''), COALESCE(r.resolution_note, ''), COALESCE(r.resolved_by, ''), COALESCE(r.resolved_at, ''),
                   COALESCE(substr(p.content, 1, 300), '')
            FROM reports r
            JOIN users u1 ON r.reporter_id = u1.id
            LEFT JOIN users u2 ON r.reported_user_id = u2.id
            LEFT JOIN posts p ON p.id = r.reported_post_id AND r.reported_post_id != ''
            WHERE (?1 = '' OR r.reason LIKE ?2 OR u1.fullname LIKE ?2 OR COALESCE(u2.fullname, '') LIKE ?2
                   OR COALESCE(r.reported_user_id, '') LIKE ?2 OR COALESCE(r.reported_post_id, '') LIKE ?2
                   OR r.status LIKE ?2 OR CAST(r.id AS TEXT) = ?1)
            ORDER BY r.id DESC
            LIMIT ?3 OFFSET ?4;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;
        const std::string like = "%" + keyword + "%";
        sqlite3_bind_text(stmt, 1, keyword.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, like.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, limit);
        sqlite3_bind_int(stmt, 4, offset < 0 ? 0 : offset);
        auto col = [&stmt](int i) -> std::string {
            const unsigned char* t = sqlite3_column_text(stmt, i);
            return t ? std::string((const char*)t) : std::string();
        };
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Report r;
            r.id = sqlite3_column_int(stmt, 0);
            r.reporter_id = col(1);
            r.reporter_name = col(2);
            r.reported_user_id = col(3);
            r.reported_user_name = col(4);
            r.reported_post_id = col(5);
            r.reported_comment_id = col(6);
            r.reason = col(7);
            r.status = col(8);
            r.created_at = col(9);
            r.category = col(10);
            r.resolution_note = col(11);
            r.resolved_by = col(12);
            r.resolved_at = col(13);
            r.reported_post_content = col(14);
            list.push_back(r);
        }
        sqlite3_finalize(stmt);
        return list;
    }

    bool classifyReport(int reportId, const std::string& category) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "UPDATE reports SET category = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, reportId);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
        sqlite3_finalize(stmt);
        return ok;
    }

    // ---- Phiên đăng nhập quản trị viên (lưu DB: khởi động lại server vẫn không phải đăng nhập lại) ----
    // ---------------- Token quản trị (chỉ lưu băm, hết hạn sau 8 giờ) ----------------
    std::string issueAdminToken(const std::string& adminId) {
        const std::string token = sec::randomToken(24);
        const std::string hash = sec::sha256Hex(token);
        const long long now = sec::epochSeconds();
        std::lock_guard<std::mutex> lock(dbMutex);
        executeSimple("DELETE FROM admin_sessions WHERE expires_at < strftime('%s','now');");
        const char* sql = "INSERT INTO admin_sessions (token, admin_id, created_at, expires_at) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
        const std::string nowStr = getCurrentTimeStr();
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, adminId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, nowStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, now + 8 * 3600);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok ? token : "";
    }

    bool verifyAdminToken(const std::string& adminId, const std::string& token) {
        if (adminId.empty() || token.size() != 48) return false;
        const std::string hash = sec::sha256Hex(token);
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT 1 FROM admin_sessions s JOIN users u ON u.id = s.admin_id "
                          "WHERE s.token = ? AND s.admin_id = ? AND s.expires_at > ? AND u.role = 'ADMIN' AND u.status != 'LOCKED' LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, adminId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, sec::epochSeconds());
        const bool ok = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return ok;
    }

    void revokeAdminToken(const std::string& token) {
        if (token.empty()) return;
        const std::string hash = sec::sha256Hex(token);
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM admin_sessions WHERE token = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // ---------------- Phiên đăng nhập người dùng (cookie HttpOnly) ----------------
    static constexpr long long kSessionTtlSec = 7LL * 24 * 3600;

    std::string createUserSession(const std::string& userId, const std::string& ip, const std::string& userAgent) {
        const std::string token = sec::randomToken(32);
        const std::string hash = sec::sha256Hex(token);
        const long long now = sec::epochSeconds();
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "INSERT INTO user_sessions (token_hash, user_id, created_at, expires_at, last_seen, ip, user_agent) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
        const std::string ua = userAgent.substr(0, 200);
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_bind_int64(stmt, 4, now + kSessionTtlSec);
        sqlite3_bind_int64(stmt, 5, now);
        sqlite3_bind_text(stmt, 6, ip.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, ua.c_str(), -1, SQLITE_STATIC);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!ok) return "";
        // Mỗi tài khoản giữ tối đa 10 phiên (xoá phiên cũ nhất)
        const char* trim = "DELETE FROM user_sessions WHERE user_id = ? AND token_hash NOT IN "
                           "(SELECT token_hash FROM user_sessions WHERE user_id = ? ORDER BY created_at DESC LIMIT 10);";
        sqlite3_stmt* ts = nullptr;
        if (sqlite3_prepare_v2(db, trim, -1, &ts, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(ts, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(ts, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(ts);
            sqlite3_finalize(ts);
        }
        return token;
    }

    // Trả về true và gán userIdOut nếu token hợp lệ, chưa hết hạn, tài khoản không bị khoá và không phải admin
    bool getSessionUser(const std::string& token, std::string& userIdOut) {
        if (token.size() != 64) return false;
        const std::string hash = sec::sha256Hex(token);
        const long long now = sec::epochSeconds();
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT s.user_id, s.expires_at, s.last_seen, u.role, u.status FROM user_sessions s JOIN users u ON u.id = s.user_id WHERE s.token_hash = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
        bool ok = false, expired = false, refresh = false;
        std::string uid;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* c0 = sqlite3_column_text(stmt, 0);
            uid = c0 ? reinterpret_cast<const char*>(c0) : "";
            const long long exp = sqlite3_column_int64(stmt, 1);
            const long long seen = sqlite3_column_int64(stmt, 2);
            const unsigned char* c3 = sqlite3_column_text(stmt, 3);
            const unsigned char* c4 = sqlite3_column_text(stmt, 4);
            const std::string role = c3 ? reinterpret_cast<const char*>(c3) : "";
            const std::string status = c4 ? reinterpret_cast<const char*>(c4) : "";
            if (exp <= now) expired = true;
            else if (role != "ADMIN" && status != "LOCKED") { ok = true; refresh = (now - seen > 3600); }
        }
        sqlite3_finalize(stmt);
        if (expired) {
            sqlite3_stmt* d = nullptr;
            if (sqlite3_prepare_v2(db, "DELETE FROM user_sessions WHERE token_hash = ?;", -1, &d, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(d, 1, hash.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(d);
                sqlite3_finalize(d);
            }
        }
        if (ok && refresh) {  // gia hạn trượt, tối đa 1 lần/giờ để không ghi DB mỗi request
            sqlite3_stmt* u = nullptr;
            if (sqlite3_prepare_v2(db, "UPDATE user_sessions SET last_seen = ?, expires_at = ? WHERE token_hash = ?;", -1, &u, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(u, 1, now);
                sqlite3_bind_int64(u, 2, now + kSessionTtlSec);
                sqlite3_bind_text(u, 3, hash.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(u);
                sqlite3_finalize(u);
            }
        }
        if (ok) userIdOut = uid;
        return ok;
    }

    void deleteUserSession(const std::string& token) {
        if (token.size() != 64) return;
        const std::string hash = sec::sha256Hex(token);
        std::lock_guard<std::mutex> lock(dbMutex);
        sqlite3_stmt* d = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM user_sessions WHERE token_hash = ?;", -1, &d, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(d, 1, hash.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(d);
            sqlite3_finalize(d);
        }
    }

    // Xoá mọi phiên của user (trừ phiên keepToken nếu có) - dùng khi đổi mật khẩu / khoá tài khoản
    void deleteUserSessions(const std::string& userId, const std::string& keepToken = "") {
        const std::string keepHash = keepToken.empty() ? "" : sec::sha256Hex(keepToken);
        std::lock_guard<std::mutex> lock(dbMutex);
        sqlite3_stmt* d = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM user_sessions WHERE user_id = ? AND token_hash != ?;", -1, &d, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(d, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(d, 2, keepHash.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(d);
            sqlite3_finalize(d);
        }
    }

    void purgeExpiredSessions() {
        std::lock_guard<std::mutex> lock(dbMutex);
        executeSimple("DELETE FROM user_sessions WHERE expires_at < strftime('%s','now');");
        executeSimple("DELETE FROM admin_sessions WHERE expires_at < strftime('%s','now');");
        executeSimple("DELETE FROM otp_codes WHERE expires_at < strftime('%s','now') - 3600;");
    }

    // Ghi mật khẩu đã băm (updateUser() KHÔNG cập nhật mật khẩu nên bản cũ đổi mật khẩu không có tác dụng)
    bool updatePassword(const std::string& userId, const std::string& passwordHash) {
        std::lock_guard<std::mutex> lock(dbMutex);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE users SET password_hash = ? WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, passwordHash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) == 1;
        sqlite3_finalize(stmt);
        return ok;
    }

    // Người dùng có được xem bài trong nhóm cộng đồng này không (nhóm công khai hoặc là thành viên)
    bool canViewCommunityPost(const std::string& postId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT g.privacy, EXISTS(SELECT 1 FROM community_group_members m WHERE m.group_id = g.id AND m.user_id = ?) "
                          "FROM community_group_posts p JOIN community_groups g ON g.id = p.group_id WHERE p.id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, postId.c_str(), -1, SQLITE_STATIC);
        bool ok = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* pv = sqlite3_column_text(stmt, 0);
            const std::string privacy = pv ? reinterpret_cast<const char*>(pv) : "PUBLIC";
            ok = (privacy != "PRIVATE") || sqlite3_column_int(stmt, 1) == 1;
        }
        sqlite3_finalize(stmt);
        return ok;
    }

    bool hasWeakAdminPassword() const { return weakAdminPassword_; }

    bool isUserAdmin(const std::string& userId) {
        if (userId.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "SELECT 1 FROM users WHERE id = ? AND role = 'ADMIN' LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        bool isAdmin = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return isAdmin;
    }

    void logAdminAction(const std::string& adminId, const std::string& action,
                        const std::string& targetType = "", const std::string& targetId = "",
                        const std::string& detail = "") {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "INSERT INTO admin_audit_logs (admin_id, action, target_type, target_id, detail, created_at) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        std::string now = getCurrentTimeStr();
        sqlite3_bind_text(stmt, 1, adminId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, action.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, targetType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, targetId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, detail.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    json getAdminAuditLogs(int limit = 50) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT a.id, a.admin_id, COALESCE(u.fullname,''), a.action, a.target_type, a.target_id, a.detail, a.created_at
            FROM admin_audit_logs a
            LEFT JOIN users u ON u.id = a.admin_id
            ORDER BY a.id DESC LIMIT ?;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            arr.push_back({
                {"id", sqlite3_column_int(stmt, 0)},
                {"admin_id", sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : ""},
                {"admin_name", sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : ""},
                {"action", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : ""},
                {"target_type", sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : ""},
                {"target_id", sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : ""},
                {"detail", sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : ""},
                {"created_at", sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : ""}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    bool resolveReportEx(int reportId, const std::string& action, const std::string& adminId,
                         const std::string& note, std::string& reporterIdOut) {
        reporterIdOut.clear();
        std::lock_guard<std::mutex> lock(dbMutex);
        // fetch reporter
        {
            const char* q = "SELECT reporter_id FROM reports WHERE id = ? LIMIT 1;";
            sqlite3_stmt* s = nullptr;
            if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(s, 1, reportId);
                if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0)) {
                    reporterIdOut = (const char*)sqlite3_column_text(s, 0);
                }
                sqlite3_finalize(s);
            }
        }
        std::string now = getCurrentTimeStr();
        const char* sql = "UPDATE reports SET status = ?, resolution_note = ?, resolved_by = ?, resolved_at = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, action.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, note.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, adminId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, reportId);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool deleteUserAccount(const std::string& userId) {
        if (userId.empty()) return false;
        User u;
        if (!getUserById(userId, u)) return false;
        if (u.role == "ADMIN") return false; // never delete admin via this path
        std::lock_guard<std::mutex> lock(dbMutex);
        // Soft-delete approach: lock + anonymize to keep FK integrity
        const char* sql = R"(
            UPDATE users SET status = 'DELETED', fullname = 'Tài khoản đã xóa', bio = '',
              avatar = '/img/default-avatar.svg', email = email || '.deleted.' || id,
              username = username || '_deleted_' || id, password_hash = 'DELETED'
            WHERE id = ? AND role != 'ADMIN';
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
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
            {"total_users", queryCount("SELECT COUNT(*) FROM users WHERE role != 'ADMIN';")},
            {"locked_users", queryCount("SELECT COUNT(*) FROM users WHERE status = 'LOCKED' AND role != 'ADMIN';")},
            {"total_posts", queryCount("SELECT COUNT(*) FROM posts;")},
            {"total_messages", queryCount("SELECT COUNT(*) FROM messages;")},
            {"total_reports", queryCount("SELECT COUNT(*) FROM reports;")},
            {"pending_reports", queryCount("SELECT COUNT(*) FROM reports WHERE status = 'PENDING';")},
            {"in_progress_reports", queryCount("SELECT COUNT(*) FROM reports WHERE status = 'IN_PROGRESS';")},
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

        for (auto& message : messages) {
            if (message.recipient_id == user1 && message.sender_id != user1) {
                recordDirectMessageDelivered(message.id, user1);
            }
            message.receipts = getMessageReceiptDetails(message.id, false);
            message.reactions = getMessageReactionSummaries(message.id, false);
        }
        return messages;
    }

    std::vector<GroupMessage> getGroupMessagesAround(const std::string& groupId, int targetId, int range = 15,
                                                      const std::string& viewerId = "") {
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

        for (auto& message : messages) {
            if (!viewerId.empty() && message.sender_id != viewerId) {
                recordGroupMessageDelivered(message.id, groupId, viewerId);
            }
            message.receipts = getMessageReceiptDetails(message.id, true);
            message.reactions = getMessageReactionSummaries(message.id, true);
        }
        return messages;
    }

    std::vector<Message> searchMessages(const std::string& user1, const std::string& user2, const std::string& query,
                                        int limit = 10, int offset = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string convId = getConversationId(user1, user2);
        std::vector<Message> results;
        if (query.empty()) return results;
        limit = std::max(1, limit);
        offset = std::max(0, offset);

        std::string normQ = removeVietnameseTones(query);

        std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, u.fullname, u.avatar, m.recipient_id, m.content, m.shared_post_id, m.is_revoked, m.created_at, COALESCE(m.media_type,'NONE'), COALESCE(m.media_url,''), COALESCE(m.reaction,''), COALESCE(m.reply_to_text,''), m.is_read, COALESCE(m.reply_to_id, 0) "
                          "FROM messages m JOIN users u ON m.sender_id = u.id "
                          "WHERE m.conversation_id = ? AND m.is_revoked = 0 ORDER BY m.id DESC;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

        sqlite3_bind_text(stmt, 1, convId.c_str(), -1, SQLITE_STATIC);
        int matchedCount = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string content = (const char*)sqlite3_column_text(stmt, 6);
            // Lọc bỏ tin nhắn hệ thống (__SYS_...) khi tìm kiếm
            if (content.rfind("__SYS_", 0) == 0) continue;
            std::string normContent = removeVietnameseTones(content);
            if (normContent.find(normQ) != std::string::npos) {
                if (matchedCount++ < offset) continue;
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
                if (static_cast<int>(results.size()) >= limit) break;
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<GroupMessage> searchGroupMessages(const std::string& groupId, const std::string& query,
                                                   const std::string& viewerId = "",
                                                   int limit = 10, int offset = 0) {
        std::lock_guard<std::mutex> lock(dbMutex);
        std::vector<GroupMessage> results;
        if (query.empty()) return results;
        limit = std::max(1, limit);
        offset = std::max(0, offset);

        std::string normQ = removeVietnameseTones(query);

        std::string sql = "SELECT gm.id, gm.group_id, gm.sender_id, u.fullname, u.avatar, gm.content, COALESCE(gm.shared_post_id,''), gm.created_at, "
                          "COALESCE(gm.media_type,'NONE'), COALESCE(gm.media_url,''), COALESCE(gm.is_revoked,0), COALESCE(gm.reaction,''), COALESCE(gm.reply_to_text,''), COALESCE(gm.reply_to_id, 0) "
                          "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
                          "WHERE gm.group_id = ? AND gm.is_revoked = 0 ORDER BY gm.id DESC;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        int matchedCount = 0;
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
            // Lọc bỏ tin nhắn hệ thống (__SYS_...) khi tìm kiếm nhóm
            if (content.rfind("__SYS_", 0) == 0) continue;
            std::string normContent = removeVietnameseTones(content);
            if (normContent.find(normQ) != std::string::npos) {
                if (matchedCount++ < offset) continue;
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
                if (static_cast<int>(results.size()) >= limit) break;
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

    json searchAllMessages(const std::string& userId, const std::string& query) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json results = json::array();
        if (userId.empty() || query.empty()) return results;

        const std::string normalizedQuery = removeVietnameseTones(query);
        std::vector<json> matches;
        const char* directSql = R"(
            SELECT m.id, m.conversation_id, m.sender_id, s.fullname, s.avatar, m.recipient_id,
                   m.content, m.shared_post_id, m.is_revoked, m.created_at,
                   COALESCE(m.media_type, 'NONE'), COALESCE(m.media_url, ''),
                   COALESCE(m.reaction, ''), COALESCE(m.reply_to_text, ''), m.is_read,
                   COALESCE(m.reply_to_id, 0),
                   CASE WHEN m.sender_id = ? THEN m.recipient_id ELSE m.sender_id END,
                   COALESCE(
                     (SELECT nickname FROM user_nicknames
                      WHERE user_id = ? AND friend_id =
                        CASE WHEN m.sender_id = ? THEN m.recipient_id ELSE m.sender_id END),
                     partner.fullname),
                   partner.avatar, COALESCE(partner.user_token, '')
            FROM messages m
            JOIN users s ON s.id = m.sender_id
            JOIN users partner ON partner.id =
              CASE WHEN m.sender_id = ? THEN m.recipient_id ELSE m.sender_id END
            WHERE m.is_revoked = 0
              AND (m.sender_id = ? OR m.recipient_id = ?)
              AND EXISTS (
                SELECT 1 FROM friendships f
                WHERE f.status = 'FRIEND'
                  AND ((f.user_id_1 = ? AND f.user_id_2 =
                        CASE WHEN m.sender_id = ? THEN m.recipient_id ELSE m.sender_id END)
                    OR (f.user_id_2 = ? AND f.user_id_1 =
                        CASE WHEN m.sender_id = ? THEN m.recipient_id ELSE m.sender_id END))
              )
            ORDER BY m.id DESC;
        )";
        sqlite3_stmt* directStmt = nullptr;
        if (sqlite3_prepare_v2(db, directSql, -1, &directStmt, nullptr) == SQLITE_OK) {
            for (int index : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}) {
                sqlite3_bind_text(directStmt, index, userId.c_str(), -1, SQLITE_STATIC);
            }
            while (sqlite3_step(directStmt) == SQLITE_ROW && matches.size() < 50) {
                const char* rawContent = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 6));
                const std::string content = rawContent ? rawContent : "";
                if (content.rfind("__SYS_", 0) == 0 ||
                    removeVietnameseTones(content).find(normalizedQuery) == std::string::npos) continue;

                Message message;
                message.id = sqlite3_column_int(directStmt, 0);
                message.conversation_id = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 1));
                message.sender_id = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 2));
                message.sender_name = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 3));
                message.sender_avatar = sqlite3_column_text(directStmt, 4)
                    ? reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 4)) : "";
                message.recipient_id = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 5));
                message.content = content;
                message.shared_post_id = sqlite3_column_text(directStmt, 7)
                    ? reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 7)) : "";
                message.is_revoked = sqlite3_column_int(directStmt, 8);
                message.created_at = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 9));
                message.media_type = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 10));
                message.media_url = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 11));
                message.reaction = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 12));
                message.reply_to_text = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 13));
                message.is_read = sqlite3_column_int(directStmt, 14);
                message.reply_to_id = sqlite3_column_int(directStmt, 15);

                json item = message.toJson();
                item["is_group"] = false;
                item["partner_id"] = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 16));
                item["partner_name"] = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 17));
                item["partner_avatar"] = sqlite3_column_text(directStmt, 18)
                    ? reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 18)) : "";
                item["partner_token"] = reinterpret_cast<const char*>(sqlite3_column_text(directStmt, 19));
                matches.push_back(std::move(item));
            }
            sqlite3_finalize(directStmt);
        } else {
            return json{{"error", "Không thể chuẩn bị truy vấn tìm kiếm tin nhắn."}};
        }

        const char* groupSql = R"(
            SELECT gm.id, gm.group_id, gm.sender_id, s.fullname, s.avatar, gm.content,
                   COALESCE(gm.shared_post_id, ''), gm.created_at,
                   COALESCE(gm.media_type, 'NONE'), COALESCE(gm.media_url, ''),
                   COALESCE(gm.is_revoked, 0), COALESCE(gm.reaction, ''),
                   COALESCE(gm.reply_to_text, ''), COALESCE(gm.reply_to_id, 0),
                   g.name, g.avatar
            FROM group_messages gm
            JOIN users s ON s.id = gm.sender_id
            JOIN groups g ON g.id = gm.group_id
            JOIN group_members membership ON membership.group_id = gm.group_id AND membership.user_id = ?
            WHERE COALESCE(gm.is_revoked, 0) = 0
              AND NOT EXISTS (
                SELECT 1 FROM group_messages_deleted deleted
                WHERE deleted.user_id = ? AND deleted.message_id = gm.id
              )
            ORDER BY gm.id DESC;
        )";
        sqlite3_stmt* groupStmt = nullptr;
        if (sqlite3_prepare_v2(db, groupSql, -1, &groupStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(groupStmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(groupStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            size_t groupMatches = 0;
            while (sqlite3_step(groupStmt) == SQLITE_ROW && groupMatches < 50) {
                const char* rawContent = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 5));
                const std::string content = rawContent ? rawContent : "";
                if (content.rfind("__SYS_", 0) == 0 ||
                    removeVietnameseTones(content).find(normalizedQuery) == std::string::npos) continue;

                GroupMessage message;
                message.id = sqlite3_column_int(groupStmt, 0);
                message.group_id = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 1));
                message.sender_id = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 2));
                message.sender_name = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 3));
                message.sender_avatar = sqlite3_column_text(groupStmt, 4)
                    ? reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 4)) : "";
                message.content = content;
                message.shared_post_id = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 6));
                message.created_at = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 7));
                message.media_type = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 8));
                message.media_url = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 9));
                message.is_revoked = sqlite3_column_int(groupStmt, 10);
                message.reaction = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 11));
                message.reply_to_text = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 12));
                message.reply_to_id = sqlite3_column_int(groupStmt, 13);

                json item = message.toJson();
                item["is_group"] = true;
                item["group_name"] = reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 14));
                item["group_avatar"] = sqlite3_column_text(groupStmt, 15)
                    ? reinterpret_cast<const char*>(sqlite3_column_text(groupStmt, 15)) : "";
                matches.push_back(std::move(item));
                ++groupMatches;
            }
            sqlite3_finalize(groupStmt);
        } else {
            return json{{"error", "Không thể chuẩn bị truy vấn tìm kiếm tin nhắn nhóm."}};
        }

        std::sort(matches.begin(), matches.end(), [](const json& left, const json& right) {
            return left.value("created_at", "") > right.value("created_at", "");
        });
        if (matches.size() > 50) matches.resize(50);
        for (auto& match : matches) results.push_back(std::move(match));
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

    // ===================== COMMUNITY GROUPS =====================
    std::string generateCommunityGroupId() {
        const char* sql = "SELECT COUNT(*) FROM community_groups;";
        sqlite3_stmt* stmt = nullptr;
        int count = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        std::stringstream ss;
        ss << "CG" << std::setw(6) << std::setfill('0') << (count + 1);
        return ss.str();
    }

    bool createCommunityGroup(const std::string& name, const std::string& description,
                              const std::string& avatar, const std::string& privacy,
                              const std::string& createdBy, json& outGroup) {
        if (name.empty() || createdBy.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        std::string id = generateCommunityGroupId();
        std::string timeStr = getCurrentTimeStr();
        std::string priv = (privacy == "PRIVATE") ? "PRIVATE" : "PUBLIC";
        const char* sql = "INSERT INTO community_groups (id, name, description, avatar, privacy, created_by, created_at) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, avatar.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, priv.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, createdBy.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, timeStr.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
        sqlite3_finalize(stmt);
        const char* mSql = "INSERT INTO community_group_members (group_id, user_id, role, joined_at) VALUES (?, ?, 'ADMIN', ?);";
        sqlite3_stmt* mStmt = nullptr;
        if (sqlite3_prepare_v2(db, mSql, -1, &mStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(mStmt, 1, id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(mStmt, 2, createdBy.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(mStmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(mStmt);
            sqlite3_finalize(mStmt);
        }
        outGroup = {{"id", id}, {"name", name}, {"description", description}, {"avatar", avatar},
                    {"privacy", priv}, {"created_by", createdBy}, {"created_at", timeStr}, {"member_count", 1}, {"role", "ADMIN"}};
        return true;
    }

    bool joinCommunityGroup(const std::string& groupId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // PRIVATE: vẫn cho join trực tiếp (đơn giản); có thể mở rộng request sau
        const char* sql = "INSERT OR IGNORE INTO community_group_members (group_id, user_id, role, joined_at) VALUES (?, ?, 'MEMBER', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        std::string timeStr = getCurrentTimeStr();
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, timeStr.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool leaveCommunityGroup(const std::string& groupId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* sql = "DELETE FROM community_group_members WHERE group_id = ? AND user_id = ? AND role != 'ADMIN';";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    json getMyCommunityGroups(const std::string& userId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT g.id, g.name, g.description, g.avatar, g.privacy, g.created_by, g.created_at, m.role,
                   (SELECT COUNT(*) FROM community_group_members WHERE group_id = g.id) as member_count,
                   (SELECT MAX(created_at) FROM community_group_posts WHERE group_id = g.id AND approval_status = 'APPROVED') as last_activity
            FROM community_groups g
            JOIN community_group_members m ON m.group_id = g.id AND m.user_id = ?
            ORDER BY COALESCE(last_activity, g.created_at) DESC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            arr.push_back({
                {"id", (const char*)sqlite3_column_text(stmt, 0)},
                {"name", (const char*)sqlite3_column_text(stmt, 1)},
                {"description", sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : ""},
                {"avatar", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : ""},
                {"privacy", sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "PUBLIC"},
                {"created_by", (const char*)sqlite3_column_text(stmt, 5)},
                {"created_at", (const char*)sqlite3_column_text(stmt, 6)},
                {"role", (const char*)sqlite3_column_text(stmt, 7)},
                {"member_count", sqlite3_column_int(stmt, 8)},
                {"last_activity", sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : ""}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    json discoverCommunityGroups(const std::string& userId, const std::string& keyword = "") {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        std::string sql = R"(
            SELECT g.id, g.name, g.description, g.avatar, g.privacy, g.created_by, g.created_at,
                   (SELECT COUNT(*) FROM community_group_members WHERE group_id = g.id) as member_count,
                   EXISTS(SELECT 1 FROM community_group_members WHERE group_id = g.id AND user_id = ?) as is_member
            FROM community_groups g
            WHERE g.privacy = 'PUBLIC'
        )";
        if (!keyword.empty()) sql += " AND (g.name LIKE ? OR g.description LIKE ?)";
        sql += " ORDER BY member_count DESC, g.created_at DESC LIMIT 50;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
        if (!keyword.empty()) {
            std::string pattern = "%" + keyword + "%";
            sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_STATIC);
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            arr.push_back({
                {"id", (const char*)sqlite3_column_text(stmt, 0)},
                {"name", (const char*)sqlite3_column_text(stmt, 1)},
                {"description", sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : ""},
                {"avatar", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : ""},
                {"privacy", sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "PUBLIC"},
                {"created_by", (const char*)sqlite3_column_text(stmt, 5)},
                {"created_at", (const char*)sqlite3_column_text(stmt, 6)},
                {"member_count", sqlite3_column_int(stmt, 7)},
                {"is_member", sqlite3_column_int(stmt, 8) != 0}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    bool createCommunityGroupPost(const std::string& groupId, const std::string& authorId,
                                  const std::string& content, const std::string& mediaType,
                                  const std::string& mediaUrl, std::string& outPostId, std::string& outStatus) {
        if (groupId.empty() || authorId.empty() || content.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        // check membership + role
        const char* roleSql = "SELECT role FROM community_group_members WHERE group_id = ? AND user_id = ? LIMIT 1;";
        sqlite3_stmt* rStmt = nullptr;
        std::string role;
        if (sqlite3_prepare_v2(db, roleSql, -1, &rStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(rStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(rStmt, 2, authorId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(rStmt) != SQLITE_ROW) { sqlite3_finalize(rStmt); return false; }
        role = (const char*)sqlite3_column_text(rStmt, 0);
        sqlite3_finalize(rStmt);

        std::string status = (role == "ADMIN") ? "APPROVED" : "PENDING";
        outStatus = status;
        std::string postId = "CGP" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string timeStr = getCurrentTimeStr();
        const char* sql = "INSERT INTO community_group_posts (id, group_id, author_id, content, media_type, media_url, approval_status, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, authorId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, mediaType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, mediaUrl.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, status.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, timeStr.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (ok) outPostId = postId;
        return ok;
    }

    bool approveCommunityGroupPost(const std::string& postId, const std::string& adminId, bool approve) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* check = R"(
            SELECT 1 FROM community_group_posts p
            JOIN community_group_members m ON m.group_id = p.group_id AND m.user_id = ? AND m.role = 'ADMIN'
            WHERE p.id = ? LIMIT 1;
        )";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, check, -1, &cStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(cStmt, 1, adminId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(cStmt, 2, postId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(cStmt) != SQLITE_ROW) { sqlite3_finalize(cStmt); return false; }
        sqlite3_finalize(cStmt);
        std::string status = approve ? "APPROVED" : "REJECTED";
        const char* sql = "UPDATE community_group_posts SET approval_status = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, postId.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    json getCommunityGroupFeed(const std::string& userId, const std::string& groupId = "", int limit = 20) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        std::string sql;
        if (groupId.empty()) {
            sql = R"(
                SELECT p.id, p.group_id, g.name, p.author_id, u.fullname, u.avatar, p.content,
                       p.media_type, p.media_url, p.approval_status, p.created_at, u.user_token
                FROM community_group_posts p
                JOIN community_groups g ON g.id = p.group_id
                JOIN users u ON u.id = p.author_id
                JOIN community_group_members m ON m.group_id = p.group_id AND m.user_id = ?
                WHERE p.approval_status = 'APPROVED'
                ORDER BY p.created_at DESC LIMIT ?;
            )";
        } else {
            sql = R"(
                SELECT p.id, p.group_id, g.name, p.author_id, u.fullname, u.avatar, p.content,
                       p.media_type, p.media_url, p.approval_status, p.created_at, u.user_token
                FROM community_group_posts p
                JOIN community_groups g ON g.id = p.group_id
                JOIN users u ON u.id = p.author_id
                WHERE p.group_id = ?
                  AND (
                    p.approval_status = 'APPROVED'
                    OR (p.approval_status = 'PENDING' AND EXISTS (
                        SELECT 1 FROM community_group_members m WHERE m.group_id = p.group_id AND m.user_id = ? AND m.role = 'ADMIN'
                    ))
                    OR p.author_id = ?
                  )
                ORDER BY p.created_at DESC LIMIT ?;
            )";
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return arr;
        if (groupId.empty()) {
            sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, limit);
        } else {
            sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, userId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 4, limit);
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string pid = (const char*)sqlite3_column_text(stmt, 0);
            int reactCnt = 0, cmtCnt = 0;
            {
                sqlite3_stmt* rStmt = nullptr;
                if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM community_group_post_reactions WHERE post_id = ?;", -1, &rStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(rStmt, 1, pid.c_str(), -1, SQLITE_STATIC);
                    if (sqlite3_step(rStmt) == SQLITE_ROW) reactCnt = sqlite3_column_int(rStmt, 0);
                    sqlite3_finalize(rStmt);
                }
                if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM community_group_post_comments WHERE post_id = ?;", -1, &rStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(rStmt, 1, pid.c_str(), -1, SQLITE_STATIC);
                    if (sqlite3_step(rStmt) == SQLITE_ROW) cmtCnt = sqlite3_column_int(rStmt, 0);
                    sqlite3_finalize(rStmt);
                }
            }
            std::string myReact;
            {
                sqlite3_stmt* mStmt = nullptr;
                if (sqlite3_prepare_v2(db, "SELECT type FROM community_group_post_reactions WHERE post_id = ? AND user_id = ? LIMIT 1;", -1, &mStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(mStmt, 1, pid.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(mStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
                    if (sqlite3_step(mStmt) == SQLITE_ROW && sqlite3_column_text(mStmt, 0))
                        myReact = (const char*)sqlite3_column_text(mStmt, 0);
                    sqlite3_finalize(mStmt);
                }
            }
            arr.push_back({
                {"id", pid},
                {"group_id", (const char*)sqlite3_column_text(stmt, 1)},
                {"group_name", (const char*)sqlite3_column_text(stmt, 2)},
                {"author_id", (const char*)sqlite3_column_text(stmt, 3)},
                {"author_name", (const char*)sqlite3_column_text(stmt, 4)},
                {"author_avatar", sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "/img/default-avatar.svg"},
                {"content", (const char*)sqlite3_column_text(stmt, 6)},
                {"media_type", sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "NONE"},
                {"media_url", sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : ""},
                {"approval_status", sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "APPROVED"},
                {"created_at", (const char*)sqlite3_column_text(stmt, 10)},
                {"user_token", sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : ""},
                {"reaction_count", reactCnt},
                {"comments_count", cmtCnt},
                {"my_reaction", myReact}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    json getCommunityGroupPendingPosts(const std::string& groupId, const std::string& adminId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* check = "SELECT 1 FROM community_group_members WHERE group_id = ? AND user_id = ? AND role = 'ADMIN' LIMIT 1;";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, check, -1, &cStmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(cStmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(cStmt, 2, adminId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(cStmt) != SQLITE_ROW) { sqlite3_finalize(cStmt); return arr; }
        sqlite3_finalize(cStmt);
        const char* sql = R"(
            SELECT p.id, p.author_id, u.fullname, u.avatar, p.content, p.media_type, p.media_url, p.created_at
            FROM community_group_posts p JOIN users u ON u.id = p.author_id
            WHERE p.group_id = ? AND p.approval_status = 'PENDING'
            ORDER BY p.created_at ASC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, groupId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            arr.push_back({
                {"id", (const char*)sqlite3_column_text(stmt, 0)},
                {"author_id", (const char*)sqlite3_column_text(stmt, 1)},
                {"author_name", (const char*)sqlite3_column_text(stmt, 2)},
                {"author_avatar", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "/img/default-avatar.svg"},
                {"content", (const char*)sqlite3_column_text(stmt, 4)},
                {"media_type", sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "NONE"},
                {"media_url", sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : ""},
                {"created_at", (const char*)sqlite3_column_text(stmt, 7)},
                {"approval_status", "PENDING"}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    bool reactCommunityGroupPost(const std::string& postId, const std::string& userId, const std::string& type) {
        std::lock_guard<std::mutex> lock(dbMutex);
        // Chỉ cho tương tác bài đã duyệt
        const char* check = "SELECT approval_status FROM community_group_posts WHERE id = ? LIMIT 1;";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, check, -1, &cStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(cStmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(cStmt) != SQLITE_ROW) { sqlite3_finalize(cStmt); return false; }
        std::string st = (const char*)sqlite3_column_text(cStmt, 0);
        sqlite3_finalize(cStmt);
        if (st != "APPROVED") return false;

        if (type.empty() || type == "NONE") {
            const char* del = "DELETE FROM community_group_post_reactions WHERE post_id = ? AND user_id = ?;";
            sqlite3_stmt* d = nullptr;
            if (sqlite3_prepare_v2(db, del, -1, &d, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_text(d, 1, postId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(d, 2, userId.c_str(), -1, SQLITE_STATIC);
            bool ok = sqlite3_step(d) == SQLITE_DONE;
            sqlite3_finalize(d);
            return ok;
        }
        std::string now = getCurrentTimeStr();
        const char* sql = "INSERT OR REPLACE INTO community_group_post_reactions (post_id, user_id, type, created_at) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool commentCommunityGroupPost(const std::string& postId, const std::string& authorId, const std::string& content, json& outComment) {
        if (content.empty()) return false;
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* check = "SELECT approval_status FROM community_group_posts WHERE id = ? LIMIT 1;";
        sqlite3_stmt* cStmt = nullptr;
        if (sqlite3_prepare_v2(db, check, -1, &cStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(cStmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(cStmt) != SQLITE_ROW) { sqlite3_finalize(cStmt); return false; }
        std::string st = (const char*)sqlite3_column_text(cStmt, 0);
        sqlite3_finalize(cStmt);
        if (st != "APPROVED") return false;

        std::string id = "CGC" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string now = getCurrentTimeStr();
        const char* sql = "INSERT INTO community_group_post_comments (id, post_id, author_id, content, created_at) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, authorId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!ok) return false;
        outComment = {{"id", id}, {"post_id", postId}, {"author_id", authorId}, {"content", content}, {"created_at", now}};
        return true;
    }

    json getCommunityGroupPostComments(const std::string& postId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        json arr = json::array();
        const char* sql = R"(
            SELECT c.id, c.author_id, u.fullname, u.avatar, c.content, c.created_at
            FROM community_group_post_comments c JOIN users u ON u.id = c.author_id
            WHERE c.post_id = ? ORDER BY c.created_at ASC;
        )";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return arr;
        sqlite3_bind_text(stmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            arr.push_back({
                {"id", (const char*)sqlite3_column_text(stmt, 0)},
                {"author_id", (const char*)sqlite3_column_text(stmt, 1)},
                {"author_name", (const char*)sqlite3_column_text(stmt, 2)},
                {"author_avatar", sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "/img/default-avatar.svg"},
                {"content", (const char*)sqlite3_column_text(stmt, 4)},
                {"created_at", (const char*)sqlite3_column_text(stmt, 5)}
            });
        }
        sqlite3_finalize(stmt);
        return arr;
    }

    // Chia sẻ bài nhóm ra bảng tin chính (tạo post PUBLIC)
    bool shareCommunityGroupPostToFeed(const std::string& groupPostId, const std::string& userId, const std::string& thoughts, std::string& outPostId) {
        std::lock_guard<std::mutex> lock(dbMutex);
        const char* q = "SELECT content, media_type, media_url, approval_status, group_id FROM community_group_posts WHERE id = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, q, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, groupPostId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return false; }
        std::string content = (const char*)sqlite3_column_text(stmt, 0);
        std::string mediaType = sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "NONE";
        std::string mediaUrl = sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
        std::string status = (const char*)sqlite3_column_text(stmt, 3);
        sqlite3_finalize(stmt);
        if (status != "APPROVED") return false;

        std::string shareText = thoughts.empty() ? ("Đã chia sẻ một bài từ nhóm:\n" + content) : (thoughts + "\n\n" + content);
        std::string postId = "POST" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string now = getCurrentTimeStr();
        const char* ins = "INSERT INTO posts (id, author_id, content, media_type, media_url, created_at, visibility) VALUES (?, ?, ?, ?, ?, ?, 'PUBLIC');";
        sqlite3_stmt* inStmt = nullptr;
        if (sqlite3_prepare_v2(db, ins, -1, &inStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(inStmt, 1, postId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 2, userId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 3, shareText.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 4, mediaType.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 5, mediaUrl.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(inStmt, 6, now.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(inStmt) == SQLITE_DONE;
        sqlite3_finalize(inStmt);
        if (ok) outPostId = postId;
        return ok;
    }

};
