#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "json.hpp"

using json = nlohmann::json;

struct User {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;
    std::string fullname;
    std::string avatar;
    std::string bio;
    std::string hometown;
    std::string school;
    std::string workplace;
    std::string interests;
    std::string role;      // "ADMIN" hoặc "USER"
    std::string status;    // "ACTIVE", "LOCKED"
    std::string google_id;
    std::string last_active_at;
    std::string created_at;
    std::string gender;
    std::string birthday;
    std::string residence;
    std::string user_token;

    bool isOnline() const {
        if (last_active_at.empty()) return false;
        std::tm t = {};
        t.tm_isdst = -1;
        std::istringstream ss(last_active_at);
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) return false;
        t.tm_isdst = -1;
        auto activeTime = std::mktime(&t);
        auto now = std::chrono::system_clock::now();
        auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return (std::abs(nowSec - activeTime) <= 300);
    }

    json toJson(bool includePrivate = false) const {
        std::string av = avatar.empty() ? "/img/default-avatar.svg" : avatar;
        json j = {
            {"id", id},
            {"username", username},
            {"email", email},
            {"fullname", fullname},
            {"avatar", av},
            {"bio", bio},
            {"hometown", hometown},
            {"school", school},
            {"workplace", workplace},
            {"interests", interests},
            {"role", role},
            {"status", status},
            {"last_active_at", last_active_at},
            {"is_online", isOnline()},
            {"created_at", created_at},
            {"gender", gender},
            {"birthday", birthday},
            {"residence", residence},
            {"user_token", user_token}
        };
        if (includePrivate) {
            j["google_id"] = google_id;
        }
        return j;
    }
};

struct Suggestion {
    std::string id;
    std::string name;
    std::string avatar;
    int distance = 0;
    int mutual = 0;
    int friendCount = 0;
    std::string reason;
    int score = 0;
    std::string hometown;
    std::string school;
    std::string workplace;
    std::string interests;
    std::string request_status; // "NONE", "PENDING_SENT", "PENDING_RECEIVED", "FRIEND"
    std::string user_token;

    json toJson() const {
        std::string av = avatar.empty() ? "/img/default-avatar.svg" : avatar;
        return {
            {"id", id},
            {"name", name},
            {"avatar", av},
            {"distance", distance},
            {"mutual", mutual},
            {"friendCount", friendCount},
            {"reason", reason},
            {"score", score},
            {"hometown", hometown},
            {"school", school},
            {"workplace", workplace},
            {"interests", interests},
            {"request_status", request_status},
            {"user_token", user_token}
        };
    }
};

struct Comment {
    std::string id;
    std::string post_id;
    std::string author_id;
    std::string author_name;
    std::string author_avatar;
    std::string content;
    std::string reply_to_id;
    std::string reply_to_author;
    std::string created_at;
    std::vector<Comment> replies;

    json toJson() const {
        std::string av = author_avatar.empty() ? "/img/default-avatar.svg" : author_avatar;
        json repArr = json::array();
        for (const auto& r : replies) {
            repArr.push_back(r.toJson());
        }
        return {
            {"id", id},
            {"post_id", post_id},
            {"author_id", author_id},
            {"author_name", author_name},
            {"author_avatar", av},
            {"content", content},
            {"reply_to_id", reply_to_id},
            {"reply_to_author", reply_to_author},
            {"created_at", created_at},
            {"replies", repArr}
        };
    }
};

struct ReactionUser {
    std::string type; // "LIKE", "LOVE", "HAHA", "WOW", "SAD", "ANGRY"
    std::string user_id;
    std::string user_name;
};

struct TopReaction {
    std::string type;
    std::string icon;
    int count = 0;
    std::vector<std::string> user_names;

    json toJson() const {
        return {
            {"type", type},
            {"icon", icon},
            {"count", count},
            {"user_names", user_names}
        };
    }
};

struct SearchHistoryItem {
    int id = 0;
    std::string user_id;
    std::string query;
    std::string created_at;

    json toJson() const {
        return {
            {"id", id},
            {"user_id", user_id},
            {"query", query},
            {"created_at", created_at}
        };
    }
};

struct OriginalPostInfo {
    std::string id;
    std::string author_id;
    std::string author_name;
    std::string author_avatar;
    std::string author_token;
    std::string title;
    std::string content;
    std::string media_type;
    std::string media_url;
    std::string created_at;
    std::string visibility;
    int total_reactions = 0;
    int comments_count = 0;

    json toJson() const {
        std::string av = author_avatar.empty() ? "/img/default-avatar.svg" : author_avatar;
        return {
            {"id", id},
            {"author_id", author_id},
            {"author_name", author_name},
            {"author_avatar", av},
            {"author_token", author_token},
            {"title", title},
            {"content", content},
            {"media_type", media_type.empty() ? "NONE" : media_type},
            {"media_url", media_url},
            {"created_at", created_at},
            {"visibility", visibility.empty() ? "PUBLIC" : visibility},
            {"total_reactions", total_reactions},
            {"comments_count", comments_count}
        };
    }
};

struct Post {
    std::string id;
    std::string author_id;
    std::string author_name;
    std::string author_avatar;
    std::string author_token;
    std::string title;
    std::string visibility; // "PUBLIC", "FRIENDS", "PRIVATE"
    std::string content;
    std::string media_type; // "NONE", "IMAGE", "VIDEO"
    std::string media_url;
    std::string shared_post_id;
    std::string shared_author_name;
    std::string shared_content;
    std::string created_at;
    int total_reactions = 0;
    std::string my_reaction; // "", "LIKE", "LOVE", "HAHA", "WOW", "SAD", "ANGRY"
    std::vector<TopReaction> top_reactions; // Tối đa 3 reaction nhiều nhất kèm danh sách tên người thả (Hình 1)
    std::vector<Comment> comments;
    bool has_original_post = false;
    OriginalPostInfo original_post;
    // AI Gemini moderation & ranking
    std::string category = "doi_song";       // giai_tri | hoc_tap | cong_nghe | doi_song | the_thao | khac
    std::string category_label = "Đời sống";
    std::string moderation_status = "SAFE";  // SAFE | FLAGGED | BLOCKED
    std::string moderation_action = "ALLOW"; // ALLOW | FLAG | BLOCK
    std::string moderation_reason;
    double feed_score = 0.0;                 // Điểm xếp hạng bảng tin (runtime)
    int share_count = 0;
    int comments_count_cached = 0;
    std::string author_school;
    std::string author_residence;
    std::string author_hometown;

    json toJson() const {
        std::string av = author_avatar.empty() ? "/img/default-avatar.svg" : author_avatar;
        json commentsJson = json::array();
        for (const auto& c : comments) {
            commentsJson.push_back(c.toJson());
        }
        json topReactJson = json::array();
        for (const auto& tr : top_reactions) {
            topReactJson.push_back(tr.toJson());
        }

        json j = {
            {"id", id},
            {"author_id", author_id},
            {"author_name", author_name},
            {"author_avatar", av},
            {"author_token", author_token},
            {"title", title},
            {"visibility", visibility.empty() ? "PUBLIC" : visibility},
            {"content", content},
            {"media_type", media_type},
            {"media_url", media_url},
            {"shared_post_id", shared_post_id},
            {"shared_author_name", shared_author_name},
            {"shared_content", shared_content},
            {"created_at", created_at},
            {"total_reactions", total_reactions},
            {"my_reaction", my_reaction},
            {"top_reactions", topReactJson},
            {"comments_count", comments.empty() ? comments_count_cached : (int)comments.size()},
            {"comments", commentsJson},
            {"category", category},
            {"category_label", category_label},
            {"moderation_status", moderation_status},
            {"moderation_action", moderation_action},
            {"moderation_reason", moderation_reason},
            {"feed_score", feed_score},
            {"share_count", share_count}
        };
        if (!shared_post_id.empty() && has_original_post) {
            j["original_post"] = original_post.toJson();
        } else {
            j["original_post"] = nullptr;
        }
        return j;
    }
};

struct Message {
    int id = 0;
    std::string conversation_id;
    std::string sender_id;
    std::string sender_name;
    std::string sender_avatar;
    std::string recipient_id;
    std::string content;
    std::string shared_post_id;
    std::string media_type; // "NONE", "IMAGE", "VIDEO"
    std::string media_url;
    std::string reaction;
    int reply_to_id = 0;
    std::string reply_to_text;
    int is_revoked = 0;
    int is_read = 0;
    std::string created_at;

    json toJson() const {
        std::string av = sender_avatar.empty() ? "/img/default-avatar.svg" : sender_avatar;
        return {
            {"id", id},
            {"conversation_id", conversation_id},
            {"sender_id", sender_id},
            {"sender_name", sender_name},
            {"sender_avatar", av},
            {"recipient_id", recipient_id},
            {"content", is_revoked ? "Tin nhắn đã được thu hồi" : content},
            {"shared_post_id", shared_post_id},
            {"media_type", media_type.empty() ? "NONE" : media_type},
            {"media_url", is_revoked ? "" : media_url},
            {"reaction", reaction},
            {"reply_to_id", reply_to_id},
            {"reply_to_text", reply_to_text},
            {"is_revoked", is_revoked != 0},
            {"is_read", is_read != 0},
            {"created_at", created_at}
        };
    }
};

struct GroupMessage {
    int id = 0;
    std::string group_id;
    std::string sender_id;
    std::string sender_name;
    std::string sender_avatar;
    std::string content;
    std::string shared_post_id;
    std::string media_type; // "NONE", "IMAGE", "VIDEO"
    std::string media_url;
    std::string reaction;
    int reply_to_id = 0;
    std::string reply_to_text;
    int is_revoked = 0;
    std::string created_at;

    json toJson() const {
        std::string av = sender_avatar.empty() ? "/img/default-avatar.svg" : sender_avatar;
        return {
            {"id", id},
            {"group_id", group_id},
            {"sender_id", sender_id},
            {"sender_name", sender_name},
            {"sender_avatar", av},
            {"content", is_revoked ? "Tin nhắn đã được thu hồi" : content},
            {"shared_post_id", shared_post_id},
            {"media_type", media_type.empty() ? "NONE" : media_type},
            {"media_url", is_revoked ? "" : media_url},
            {"reaction", reaction},
            {"reply_to_id", reply_to_id},
            {"reply_to_text", reply_to_text},
            {"is_revoked", is_revoked != 0},
            {"created_at", created_at}
        };
    }
};

struct Group {
    std::string id;
    std::string name;
    std::string description;
    std::string avatar;
    std::string created_by;
    std::string created_at;
    int member_count = 0;
    std::vector<std::string> member_ids;
    std::string last_message;
    std::string last_message_time;

    json toJson() const {
        std::string av = avatar.empty() ? "/img/default-avatar.svg" : avatar;
        return {
            {"id", id},
            {"name", name},
            {"description", description},
            {"avatar", av},
            {"created_by", created_by},
            {"created_at", created_at},
            {"member_count", member_count},
            {"member_ids", member_ids},
            {"last_message", last_message},
            {"last_message_time", last_message_time}
        };
    }
};

struct Notification {
    int id = 0;
    std::string user_id;
    std::string actor_id;
    std::string actor_name;
    std::string actor_avatar;
    std::string type; // "LIKE", "COMMENT", "FRIEND_REQUEST", "MESSAGE"
    std::string content;
    std::string link;
    int is_read = 0;
    std::string created_at;

    json toJson() const {
        std::string av = actor_avatar.empty() ? "/img/default-avatar.svg" : actor_avatar;
        return {
            {"id", id},
            {"user_id", user_id},
            {"actor_id", actor_id},
            {"actor_name", actor_name},
            {"actor_avatar", av},
            {"type", type},
            {"content", content},
            {"link", link},
            {"is_read", is_read != 0},
            {"created_at", created_at}
        };
    }
};

struct Report {
    int id = 0;
    std::string reporter_id;
    std::string reporter_name;
    std::string reported_user_id;
    std::string reported_user_name;
    std::string reported_post_id;
    std::string reported_comment_id;
    std::string reason;
    std::string status; // "PENDING", "RESOLVED", "DISMISSED"
    std::string created_at;

    json toJson() const {
        return {
            {"id", id},
            {"reporter_id", reporter_id},
            {"reporter_name", reporter_name},
            {"reported_user_id", reported_user_id},
            {"reported_user_name", reported_user_name},
            {"reported_post_id", reported_post_id},
            {"reported_comment_id", reported_comment_id},
            {"reason", reason},
            {"status", status},
            {"created_at", created_at}
        };
    }
};
