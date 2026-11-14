#pragma once
// ============================================================================
//  Security.h - Các thành phần bảo mật dùng chung cho toàn dự án
//  - SHA-256 / HMAC-SHA256 / PBKDF2 (băm mật khẩu có salt, không lưu plaintext)
//  - Sinh số ngẫu nhiên an toàn (CSPRNG) cho token / OTP / salt
//  - Rate limiter (token bucket) + bộ đếm đăng nhập sai (chống brute-force)
//  - Hàm kiểm tra / làm sạch dữ liệu đầu vào (email, username, tên, URL media)
//  Không phụ thuộc thư viện ngoài, chạy được trên Windows (MinGW) và Linux.
// ============================================================================

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
// RtlGenRandom (advapi32) - CSPRNG của Windows. Khai báo tay để tránh xung đột thứ tự include windows.h
extern "C" unsigned char __stdcall SystemFunction036(void* buffer, unsigned long length);
#endif

namespace sec {

// ----------------------------------------------------------------------------
// CSPRNG
// ----------------------------------------------------------------------------
inline void randomBytes(uint8_t* out, size_t n) {
#ifdef _WIN32
    if (!SystemFunction036(out, static_cast<unsigned long>(n))) throw std::runtime_error("CSPRNG failed");
#else
    static std::mutex m;
    std::lock_guard<std::mutex> lk(m);
    static std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
    if (!urandom) throw std::runtime_error("CSPRNG unavailable");
    urandom.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(n));
    if (!urandom) throw std::runtime_error("CSPRNG failed");
#endif
}

inline std::string toHex(const uint8_t* d, size_t n) {
    static const char* hx = "0123456789abcdef";
    std::string s;
    s.resize(n * 2);
    for (size_t i = 0; i < n; ++i) { s[2 * i] = hx[d[i] >> 4]; s[2 * i + 1] = hx[d[i] & 15]; }
    return s;
}

inline std::string randomToken(size_t bytes = 32) {
    std::vector<uint8_t> b(bytes);
    randomBytes(b.data(), bytes);
    return toHex(b.data(), bytes);
}

// Chuỗi 6 chữ số (có thể bắt đầu bằng 0), lấy mẫu loại bỏ lệch (rejection sampling)
inline std::string randomDigits6() {
    const uint32_t limit = (0xFFFFFFFFu / 1000000u) * 1000000u;
    uint32_t v = 0;
    do {
        uint8_t b[4];
        randomBytes(b, 4);
        v = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
    } while (v >= limit);
    v %= 1000000u;
    std::string s = std::to_string(v);
    return std::string(6 - s.size(), '0') + s;
}

// So sánh chuỗi trong thời gian không phụ thuộc nội dung (chống timing attack)
inline bool ctEquals(const std::string& a, const std::string& b) {
    unsigned char r = (a.size() != b.size()) ? 1 : 0;
    const size_t n = std::max(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char ca = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char cb = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        r |= static_cast<unsigned char>(ca ^ cb);
    }
    return r == 0;
}

// ----------------------------------------------------------------------------
// SHA-256 / HMAC / PBKDF2
// ----------------------------------------------------------------------------
class Sha256 {
public:
    Sha256() { reset(); }
    void reset() {
        static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::memcpy(st_, init, sizeof(st_));
        len_ = 0;
        bl_ = 0;
    }
    void update(const uint8_t* d, size_t n) {
        len_ += n;
        while (n > 0) {
            const size_t take = std::min(n, size_t(64) - bl_);
            std::memcpy(buf_ + bl_, d, take);
            bl_ += take; d += take; n -= take;
            if (bl_ == 64) { compress(buf_); bl_ = 0; }
        }
    }
    void update(const std::string& s) { update(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }
    void final(uint8_t out[32]) {
        const uint64_t bits = len_ * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        pad = 0;
        while (bl_ != 56) update(&pad, 1);
        uint8_t lb[8];
        for (int i = 0; i < 8; ++i) lb[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
        update(lb, 8);
        for (int i = 0; i < 8; ++i) {
            out[4 * i] = static_cast<uint8_t>(st_[i] >> 24);
            out[4 * i + 1] = static_cast<uint8_t>(st_[i] >> 16);
            out[4 * i + 2] = static_cast<uint8_t>(st_[i] >> 8);
            out[4 * i + 3] = static_cast<uint8_t>(st_[i]);
        }
    }

private:
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void compress(const uint8_t* p) {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) | (uint32_t(p[4 * i + 2]) << 8) | uint32_t(p[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = st_[0], b = st_[1], c = st_[2], d = st_[3], e = st_[4], f = st_[5], g = st_[6], h = st_[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = h + S1 + ch + K[i] + w[i];
            const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + mj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        st_[0] += a; st_[1] += b; st_[2] += c; st_[3] += d; st_[4] += e; st_[5] += f; st_[6] += g; st_[7] += h;
    }
    uint32_t st_[8];
    uint64_t len_;
    uint8_t buf_[64];
    size_t bl_;
};

inline std::string sha256Hex(const std::string& s) {
    Sha256 h;
    h.update(s);
    uint8_t out[32];
    h.final(out);
    return toHex(out, 32);
}

class HmacSha256 {
public:
    explicit HmacSha256(const std::string& key) {
        uint8_t k[64] = {0};
        if (key.size() > 64) {
            Sha256 h; h.update(key);
            uint8_t d[32]; h.final(d);
            std::memcpy(k, d, 32);
        } else {
            std::memcpy(k, key.data(), key.size());
        }
        uint8_t ip[64], op[64];
        for (int i = 0; i < 64; ++i) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
        inner_.update(ip, 64);
        outer_.update(op, 64);
    }
    void mac(const uint8_t* msg, size_t n, uint8_t out[32]) const {
        Sha256 in = inner_;
        in.update(msg, n);
        uint8_t ih[32];
        in.final(ih);
        Sha256 ou = outer_;
        ou.update(ih, 32);
        ou.final(out);
    }

private:
    Sha256 inner_, outer_;
};

// PBKDF2-HMAC-SHA256, độ dài khóa 32 byte
inline std::array<uint8_t, 32> pbkdf2(const std::string& password, const std::vector<uint8_t>& salt, uint32_t iterations) {
    HmacSha256 hm(password);
    std::vector<uint8_t> msg(salt);
    msg.push_back(0); msg.push_back(0); msg.push_back(0); msg.push_back(1);
    uint8_t u[32], t[32];
    hm.mac(msg.data(), msg.size(), u);
    std::memcpy(t, u, 32);
    for (uint32_t i = 1; i < iterations; ++i) {
        hm.mac(u, 32, u);
        for (int j = 0; j < 32; ++j) t[j] ^= u[j];
    }
    std::array<uint8_t, 32> r;
    std::memcpy(r.data(), t, 32);
    return r;
}

constexpr uint32_t kPbkdf2Iterations = 100000;

// Định dạng lưu: pbkdf2$<số vòng>$<salt hex>$<hash hex>
inline std::string hashPassword(const std::string& password, uint32_t iterations = kPbkdf2Iterations) {
    std::vector<uint8_t> salt(16);
    randomBytes(salt.data(), salt.size());
    const auto dk = pbkdf2(password, salt, iterations);
    return "pbkdf2$" + std::to_string(iterations) + "$" + toHex(salt.data(), salt.size()) + "$" + toHex(dk.data(), dk.size());
}

inline bool isHashedPassword(const std::string& stored) { return stored.rfind("pbkdf2$", 0) == 0; }

inline bool hexToBytes(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto hv = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int a = hv(hex[i]), b = hv(hex[i + 1]);
        if (a < 0 || b < 0) return false;
        out.push_back(static_cast<uint8_t>(a * 16 + b));
    }
    return true;
}

inline bool verifyPassword(const std::string& password, const std::string& stored) {
    if (!isHashedPassword(stored)) return false;  // plaintext / "!" / GOOGLE_AUTH -> không bao giờ khớp
    std::vector<std::string> parts;
    std::stringstream ss(stored);
    std::string item;
    while (std::getline(ss, item, '$')) parts.push_back(item);
    if (parts.size() != 4) return false;
    uint32_t iters = 0;
    try { iters = static_cast<uint32_t>(std::stoul(parts[1])); } catch (...) { return false; }
    if (iters < 1000 || iters > 5000000) return false;
    std::vector<uint8_t> salt;
    if (!hexToBytes(parts[2], salt)) return false;
    const auto dk = pbkdf2(password, salt, iters);
    return ctEquals(toHex(dk.data(), dk.size()), parts[3]);
}

inline bool needsRehash(const std::string& stored) {
    if (!isHashedPassword(stored)) return true;
    const size_t p1 = stored.find('$');
    const size_t p2 = stored.find('$', p1 + 1);
    if (p2 == std::string::npos) return true;
    try { return std::stoul(stored.substr(p1 + 1, p2 - p1 - 1)) < kPbkdf2Iterations; } catch (...) { return true; }
}

// ----------------------------------------------------------------------------
// Thời gian
// ----------------------------------------------------------------------------
inline double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
inline long long epochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// ----------------------------------------------------------------------------
// Rate limiter (token bucket). allow() trả false khi vượt giới hạn.
// ----------------------------------------------------------------------------
class RateLimiter {
public:
    bool allow(const std::string& key, double capacity, double refillPerSec, double cost = 1.0) {
        const double now = nowSeconds();
        std::lock_guard<std::mutex> lk(m_);
        maybePurge(now);
        auto it = map_.find(key);
        if (it == map_.end()) {
            it = map_.emplace(key, Bucket{capacity, now}).first;
        }
        Bucket& b = it->second;
        b.tokens = std::min(capacity, b.tokens + (now - b.last) * refillPerSec);
        b.last = now;
        if (b.tokens < cost) return false;
        b.tokens -= cost;
        return true;
    }

    // Kiểm tra còn lượt hay không mà KHÔNG trừ lượt
    bool peek(const std::string& key, double capacity, double refillPerSec) {
        const double now = nowSeconds();
        std::lock_guard<std::mutex> lk(m_);
        auto it = map_.find(key);
        if (it == map_.end()) return true;
        const double tokens = std::min(capacity, it->second.tokens + (now - it->second.last) * refillPerSec);
        return tokens >= 1.0;
    }

private:
    struct Bucket { double tokens; double last; };
    void maybePurge(double now) {
        if (now - lastPurge_ < 300 && map_.size() < 200000) return;
        lastPurge_ = now;
        for (auto it = map_.begin(); it != map_.end();) {
            if (now - it->second.last > 900) it = map_.erase(it); else ++it;
        }
        if (map_.size() > 300000) map_.clear();  // chặn tràn bộ nhớ khi bị giả mạo hàng loạt khóa
    }
    std::mutex m_;
    std::unordered_map<std::string, Bucket> map_;
    double lastPurge_ = 0;
};

// ----------------------------------------------------------------------------
// Bộ đếm lần thất bại (đăng nhập / OTP) với khóa tạm thời
// ----------------------------------------------------------------------------
class FailTracker {
public:
    // true nếu khóa này đang bị khóa
    bool isLocked(const std::string& key, double* retryAfter = nullptr) {
        const double now = nowSeconds();
        std::lock_guard<std::mutex> lk(m_);
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        if (it->second.lockedUntil > now) {
            if (retryAfter) *retryAfter = it->second.lockedUntil - now;
            return true;
        }
        return false;
    }
    void recordFail(const std::string& key, int maxFails, double windowSec, double lockSec) {
        const double now = nowSeconds();
        std::lock_guard<std::mutex> lk(m_);
        if (map_.size() > 200000) map_.clear();
        Entry& e = map_[key];
        if (now - e.first > windowSec) { e.fails = 0; e.first = now; }
        if (e.fails == 0) e.first = now;
        ++e.fails;
        if (e.fails >= maxFails) { e.lockedUntil = now + lockSec; e.fails = 0; }
    }
    void reset(const std::string& key) {
        std::lock_guard<std::mutex> lk(m_);
        map_.erase(key);
    }

private:
    struct Entry { int fails = 0; double first = 0; double lockedUntil = 0; };
    std::mutex m_;
    std::unordered_map<std::string, Entry> map_;
};

// ----------------------------------------------------------------------------
// Cấu hình từ biến môi trường / file .env
// ----------------------------------------------------------------------------
inline std::string envOr(const char* name, const std::string& def = "") {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : def;
}
inline bool envFlag(const char* name) {
    const std::string v = envOr(name);
    return v == "1" || v == "true" || v == "TRUE" || v == "yes";
}
inline long long envInt(const char* name, long long def) {
    const std::string v = envOr(name);
    if (v.empty()) return def;
    try { return std::stoll(v); } catch (...) { return def; }
}
inline void loadDotEnv(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos || line[s] == '#') continue;
        const size_t eq = line.find('=', s);
        if (eq == std::string::npos) continue;
        std::string k = line.substr(s, eq - s), v = line.substr(eq + 1);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
        while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
        while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
        if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) v = v.substr(1, v.size() - 2);
        if (k.empty() || std::getenv(k.c_str())) continue;  // biến môi trường thật được ưu tiên
#ifdef _WIN32
        _putenv_s(k.c_str(), v.c_str());
#else
        setenv(k.c_str(), v.c_str(), 0);
#endif
    }
}

// ----------------------------------------------------------------------------
// Kiểm tra / làm sạch dữ liệu đầu vào
// ----------------------------------------------------------------------------
inline size_t utf8Length(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

inline std::string utf8Truncate(const std::string& s, size_t maxCp) {
    size_t cp = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (cp == maxCp) return s.substr(0, i);
            ++cp;
        }
    }
    return s;
}

// Ký tự điều khiển hướng văn bản / zero-width (dùng để giả mạo tên hiển thị)
inline bool isInvisibleUtf8At(const std::string& s, size_t i) {
    if (i + 2 >= s.size() || static_cast<unsigned char>(s[i]) != 0xE2) return false;
    const unsigned char b1 = static_cast<unsigned char>(s[i + 1]), b2 = static_cast<unsigned char>(s[i + 2]);
    return (b1 == 0x80 && ((b2 >= 0x8B && b2 <= 0x8F) || (b2 >= 0xAA && b2 <= 0xAE))) || (b1 == 0x81 && b2 >= 0xA6 && b2 <= 0xA9);
}

// Làm sạch văn bản 1 dòng (tên, tiêu đề, nơi ở...): bỏ ký tự điều khiển và các ký tự nguy hiểm cho HTML/JS
inline std::string sanitizeLine(const std::string& in, size_t maxCp) {
    std::string out;
    out.reserve(in.size());
    bool lastSpace = true;
    for (size_t i = 0; i < in.size();) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (isInvisibleUtf8At(in, i)) { i += 3; continue; }
        if (c < 0x20 || c == 0x7f || c == '<' || c == '>' || c == '"' || c == '\'' || c == '`' || c == '\\') { ++i; continue; }
        if (c == ' ') {
            if (!lastSpace) out.push_back(' ');
            lastSpace = true; ++i; continue;
        }
        lastSpace = false;
        out.push_back(static_cast<char>(c));
        ++i;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return utf8Truncate(out, maxCp);
}

// Làm sạch văn bản nhiều dòng (bài viết, bio, tin nhắn): chỉ bỏ ký tự điều khiển nguy hiểm, giới hạn độ dài.
// Việc escape HTML khi hiển thị do frontend (escapeHtml) đảm nhiệm.
inline std::string sanitizeText(const std::string& in, size_t maxCp) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (isInvisibleUtf8At(in, i)) { i += 3; continue; }
        if ((c < 0x20 && c != '\n' && c != '\t' && c != '\r') || c == 0x7f) { ++i; continue; }
        out.push_back(static_cast<char>(c));
        ++i;
    }
    return utf8Truncate(out, maxCp);
}

inline bool isValidUsername(const std::string& u) {
    if (u.size() < 3 || u.size() > 30) return false;
    for (unsigned char c : u) if (!(std::isalnum(c) || c == '_' || c == '.')) return false;
    return true;
}

inline std::string toLowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Email chặt chẽ: chỉ ký tự an toàn, không có dấu nháy/khoảng trắng/ký tự shell
inline bool isValidEmail(const std::string& e) {
    if (e.size() < 6 || e.size() > 254) return false;
    const size_t at = e.find('@');
    if (at == std::string::npos || at != e.rfind('@') || at == 0 || at > 64) return false;
    for (size_t i = 0; i < at; ++i) {
        const unsigned char c = static_cast<unsigned char>(e[i]);
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-')) return false;
    }
    const std::string dom = e.substr(at + 1);
    if (dom.size() < 4 || dom.front() == '.' || dom.back() == '.' || dom.front() == '-') return false;
    bool dot = false;
    char prev = 0;
    for (unsigned char c : dom) {
        if (!(std::isalnum(c) || c == '.' || c == '-')) return false;
        if (c == '.') { dot = true; if (prev == '.') return false; }
        prev = static_cast<char>(c);
    }
    const size_t ld = dom.rfind('.');
    return dot && dom.size() - ld - 1 >= 2;
}

// Mật khẩu mới: 8-128 ký tự, có cả chữ và số
inline bool isStrongPassword(const std::string& p, std::string* why = nullptr) {
    if (p.size() < 8) { if (why) *why = "Mật khẩu phải có ít nhất 8 ký tự."; return false; }
    if (p.size() > 128) { if (why) *why = "Mật khẩu quá dài (tối đa 128 ký tự)."; return false; }
    bool letter = false, digit = false;
    for (unsigned char c : p) { if (std::isalpha(c)) letter = true; if (std::isdigit(c)) digit = true; }
    if (!letter || !digit) { if (why) *why = "Mật khẩu phải gồm cả chữ và số."; return false; }
    return true;
}

// URL ảnh đại diện / media: chỉ cho phép đường dẫn nội bộ, https, hoặc data: URL có mime trong danh sách trắng
inline bool hasOnlySafeUrlChars(const std::string& u) {
    for (unsigned char c : u) {
        if (c <= 0x20 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '`' || c == '\\' || c == 0x7f) return false;
    }
    return true;
}

inline bool isAllowedDataMime(const std::string& mime, const std::string& kind) {
    static const std::set<std::string> img = {"image/png", "image/jpeg", "image/jpg", "image/gif", "image/webp", "image/bmp", "image/avif", "image/heic", "image/heif"};
    static const std::set<std::string> vid = {"video/mp4", "video/webm", "video/ogg", "video/quicktime", "video/x-matroska", "video/3gpp", "video/x-m4v", "video/mpeg"};
    static const std::set<std::string> aud = {"audio/webm", "audio/mpeg", "audio/ogg", "audio/wav", "audio/mp4"};
    static const std::set<std::string> fil = {"application/pdf", "text/plain", "application/zip", "application/msword",
        "application/vnd.ms-excel", "application/vnd.ms-powerpoint",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        "application/vnd.openxmlformats-officedocument.presentationml.presentation"};
    if (kind == "IMAGE") return img.count(mime) > 0;
    if (kind == "VIDEO") return vid.count(mime) > 0;
    if (kind == "AUDIO") return aud.count(mime) > 0;
    if (kind == "FILE") return fil.count(mime) > 0 || img.count(mime) > 0 || vid.count(mime) > 0 || aud.count(mime) > 0;
    return false;
}

// Host https được phép nhúng ảnh/GIF: mặc định Giphy, Unsplash (dữ liệu mẫu), ảnh đại diện Google.
// Thêm host khác qua biến môi trường EXTRA_MEDIA_HOSTS (phân tách bằng dấu phẩy).
inline bool isAllowedMediaHost(const std::string& url) {
    const size_t s = 8;  // sau "https://"
    size_t e = url.find_first_of("/:?#", s);
    if (e == std::string::npos) e = url.size();
    const std::string host = toLowerAscii(url.substr(s, e - s));
    if (host.empty() || host.find('@') != std::string::npos) return false;
    std::vector<std::string> allowed = {"giphy.com", "images.unsplash.com", "googleusercontent.com"};
    std::stringstream ss(envOr("EXTRA_MEDIA_HOSTS"));
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = toLowerAscii(item);
        while (!item.empty() && item.front() == ' ') item.erase(item.begin());
        while (!item.empty() && item.back() == ' ') item.pop_back();
        if (!item.empty()) allowed.push_back(item);
    }
    for (const auto& a : allowed) {
        if (host == a) return true;
        if (host.size() > a.size() && host.compare(host.size() - a.size(), a.size(), a) == 0 && host[host.size() - a.size() - 1] == '.') return true;
    }
    return false;
}

// kind: IMAGE | VIDEO | AUDIO | FILE
inline bool isSafeMediaUrl(const std::string& url, const std::string& kind) {
    if (url.empty()) return true;
    if (url.rfind("data:", 0) == 0) {
        const size_t semi = url.find(';');
        const size_t comma = url.find(',');
        if (semi == std::string::npos || comma == std::string::npos || semi > comma) return false;
        if (url.compare(semi, comma - semi + 1, ";base64,") != 0) return false;
        const std::string mime = toLowerAscii(url.substr(5, semi - 5));
        if (!isAllowedDataMime(mime, kind)) return false;
        for (size_t i = comma + 1; i < url.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(url[i]);
            if (!(std::isalnum(c) || c == '+' || c == '/' || c == '=')) return false;
        }
        return true;
    }
    if (!hasOnlySafeUrlChars(url) || url.size() > 2048) return false;
    if (url.rfind("/", 0) == 0 && url.rfind("//", 0) != 0) return url.find("..") == std::string::npos;
    if (url.rfind("https://", 0) == 0) return isAllowedMediaHost(url);
    return false;
}

// Số nguyên trong khoảng [lo, hi]; chuỗi không hợp lệ -> def
inline int clampInt(const std::string& s, int def, int lo, int hi) {
    if (s.empty() || s.size() > 9) return def;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!(std::isdigit(static_cast<unsigned char>(s[i])) || (i == 0 && s[i] == '-' && s.size() > 1))) return def;
    }
    long v = std::strtol(s.c_str(), nullptr, 10);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return static_cast<int>(v);
}

}  // namespace sec
