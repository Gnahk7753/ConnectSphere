#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include "Models.h"
#include "Database.h"

class SuggestService {
private:
    static std::vector<std::string> splitInterests(const std::string& str) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, ';')) {
            size_t first = item.find_first_not_of(" \t\r\n");
            size_t last = item.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                result.push_back(item.substr(first, last - first + 1));
            }
        }
        return result;
    }

    static int countCommonInterests(const std::string& int1, const std::string& int2) {
        auto list1 = splitInterests(int1);
        auto list2 = splitInterests(int2);
        int common = 0;
        for (const auto& a : list1) {
            for (const auto& b : list2) {
                if (a == b) {
                    common++;
                    break;
                }
            }
        }
        return common;
    }

public:
    static std::vector<Suggestion> getSuggestions(Database& db, const std::string& myId, int maxResults = 8, const std::string& filter = "ALL") {
        std::vector<Suggestion> results;
        User myProfile;
        if (!db.getUserById(myId, myProfile)) return results;

        std::vector<std::string> myFriends = db.getFriendsList(myId);
        std::set<std::string> directFriends(myFriends.begin(), myFriends.end());

        std::map<std::string, std::vector<std::string>> adjacency;
        auto getNeighbors = [&](const std::string& uid) -> const std::vector<std::string>& {
            auto it = adjacency.find(uid);
            if (it == adjacency.end()) {
                it = adjacency.emplace(uid, db.getFriendsList(uid)).first;
            }
            return it->second;
        };

        // Thuật toán BFS tìm bạn của bạn
        std::map<std::string, int> distance;
        std::vector<std::string> queue;

        distance[myId] = 0;
        queue.push_back(myId);

        for (size_t head = 0; head < queue.size(); ++head) {
            std::string curr = queue[head];
            int d = distance[curr];

            if (d >= 3) continue;

            for (const std::string& neighbor : getNeighbors(curr)) {
                if (distance.find(neighbor) == distance.end()) {
                    distance[neighbor] = d + 1;
                    queue.push_back(neighbor);
                }
            }
        }

        std::set<std::string> candidateIds;
        for (const std::string& candidateId : queue) {
            if (candidateId == myId) continue;
            if (directFriends.count(candidateId)) continue;

            int dist = distance[candidateId];
            if (dist == 2 || dist == 3) {
                candidateIds.insert(candidateId);
            }
        }

        // Lấy thêm tất cả người dùng nếu cần lọc hoặc số lượng ít
        std::vector<User> allUsers = db.getAllUsers();
        for (const auto& u : allUsers) {
            if (u.id == myId) continue;
            if (directFriends.count(u.id)) continue;
            candidateIds.insert(u.id);
        }

        for (const std::string& candidateId : candidateIds) {
            User candidateProfile;
            if (!db.getUserById(candidateId, candidateProfile)) continue;
            if (candidateProfile.status == "LOCKED") continue;
            if (candidateProfile.role == "ADMIN") continue;
            if (db.isUserBlocked(myId, candidateId)) continue;

            const auto& candFriends = getNeighbors(candidateId);
            int mutualCount = 0;
            for (const std::string& cf : candFriends) {
                if (directFriends.count(cf)) mutualCount++;
            }

            int dist = distance.count(candidateId) ? distance[candidateId] : 0;
            int score = (dist == 2 ? 100 : (dist == 3 ? 50 : 10)) + (mutualCount * 15);
            std::string reason = (dist == 2) ? "Bạn của bạn bè" : "Gợi ý kết bạn";

            bool isSameSchool = (!myProfile.school.empty() && myProfile.school == candidateProfile.school);
            bool isSameHometown = (!myProfile.hometown.empty() && myProfile.hometown == candidateProfile.hometown);
            int commonInterests = countCommonInterests(myProfile.interests, candidateProfile.interests);

            if (isSameSchool) {
                score += 35;
                reason = "Cùng trường: " + myProfile.school;
            } else if (isSameHometown) {
                score += 30;
                reason = "Cùng quê: " + myProfile.hometown;
            } else if (!myProfile.workplace.empty() && myProfile.workplace == candidateProfile.workplace) {
                score += 20;
                reason = "Cùng nơi làm việc";
            }

            if (commonInterests > 0) {
                score += (commonInterests * 12);
                if (reason == "Gợi ý kết bạn" || reason == "Bạn của bạn bè") {
                    reason = "Chung " + std::to_string(commonInterests) + " sở thích";
                }
            }

            // Áp dụng bộ lọc người dùng chọn
            if (filter == "SCHOOL" && !isSameSchool) continue;
            if (filter == "HOMETOWN" && !isSameHometown) continue;
            if (filter == "INTERESTS" && commonInterests <= 0) continue;
            if (filter == "MUTUAL" && mutualCount <= 0 && dist != 2) continue;

            // Chỉ hiển thị người có lý do gợi ý rõ ràng, bỏ lý do chung chung
            if (reason == "Gợi ý kết bạn") continue;

            std::string reqStatus = db.getFriendshipRelation(myId, candidateId);

            Suggestion s;
            s.id = candidateProfile.id;
            s.name = candidateProfile.fullname;
            s.avatar = candidateProfile.avatar;
            s.distance = dist;
            s.mutual = mutualCount;
            s.friendCount = (int)candFriends.size();
            s.reason = reason;
            s.score = score;
            s.hometown = candidateProfile.hometown;
            s.school = candidateProfile.school;
            s.interests = candidateProfile.interests;
            s.request_status = reqStatus;
            s.user_token = candidateProfile.user_token;

            results.push_back(s);
        }

        std::sort(results.begin(), results.end(), [](const Suggestion& a, const Suggestion& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.mutual != b.mutual) return a.mutual > b.mutual;
            return a.friendCount > b.friendCount;
        });

        if ((int)results.size() > maxResults) {
            results.resize(maxResults);
        }

        return results;
    }
};
