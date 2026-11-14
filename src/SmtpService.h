#pragma once

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "Security.h"

using json = nlohmann::json;

// ============================================================================
//  Gửi OTP qua Gmail SMTP.
//  BẢO MẬT:
//   - Địa chỉ nhận được kiểm tra bằng sec::isValidEmail (không có nháy, khoảng trắng, ký tự shell).
//   - Nội dung thư chỉ gồm chuỗi cố định + 6 chữ số do server sinh ra (không chứa dữ liệu người dùng).
//   - Tên file tạm ngẫu nhiên, xoá ngay sau khi gửi (bản cũ dùng tên cố định -> race condition).
//   - Cấu hình đọc từ biến môi trường / .env (SMTP_EMAIL, SMTP_PASSWORD), file json chỉ là phương án dự phòng.
// ============================================================================
class SmtpService {
public:
    static std::string generate6DigitOtp() { return sec::randomDigits6(); }

    struct Config {
        std::string user;
        std::string pass;
        bool ok() const { return !user.empty() && !pass.empty(); }
    };

    static Config loadConfig() {
        Config c;
        c.user = sec::envOr("SMTP_EMAIL");
        c.pass = sec::envOr("SMTP_PASSWORD");
        if (!c.ok()) {
            // Dự phòng: các file json cũ (hỗ trợ cả 2 cặp khoá vì bản cũ đặt tên không khớp file mẫu)
            for (const char* path : {"config.json", "email_config.json"}) {
                std::ifstream f(path);
                if (!f.is_open()) continue;
                try {
                    json j = json::parse(f);
                    std::string u = j.value("smtp_email", j.value("sender_email", ""));
                    std::string p = j.value("smtp_password", j.value("app_password", ""));
                    if (!u.empty() && !p.empty()) { c.user = u; c.pass = p; break; }
                } catch (...) {}
            }
        }
        std::string normalizedPassword;
        for (unsigned char ch : c.pass) {
            if (!std::isspace(ch)) normalizedPassword.push_back(static_cast<char>(ch));
        }
        c.pass = normalizedPassword;
        return c;
    }

    static bool isConfigured() {
        Config c = loadConfig();
        return c.ok() && sec::isValidEmail(c.user);
    }

    /** Trả về true khi email đã được gửi (hoặc mã được ghi ra console ở chế độ dev). */
    static bool sendOtpEmail(const std::string& toEmail, const std::string& otpCode, const std::string& purposeText = "Đăng ký tài khoản") {
        if (!sec::isValidEmail(toEmail)) return false;
        for (char c : otpCode) if (c < '0' || c > '9') return false;
        if (otpCode.size() != 6) return false;

        const Config cfg = loadConfig();
        const bool configured = cfg.ok() && sec::isValidEmail(cfg.user) && cfg.pass.find_first_of("\r\n\0") == std::string::npos;
        const bool production = sec::envFlag("PRODUCTION");

        if (!configured) {
            // Chế độ phát triển: in mã ra console của máy chủ (KHÔNG bao giờ trả về qua API trừ khi DEV_SHOW_OTP=1)
            if (!production || sec::envFlag("DEV_SHOW_OTP")) {
                std::cout << "[SMTP] Chưa cấu hình Gmail (SMTP_EMAIL/SMTP_PASSWORD). Mã OTP cho " << toEmail << ": " << otpCode << std::endl;
            } else {
                std::cerr << "[SMTP] Chưa cấu hình SMTP_EMAIL/SMTP_PASSWORD trong .env: không thể gửi OTP." << std::endl;
            }
            return !production || sec::envFlag("DEV_SHOW_OTP");
        }

        const std::string rnd = sec::randomToken(8);
        const std::string subject = "[Mang Xa Hoi Mini] Ma OTP: " + otpCode;
        const std::string body = "Xin chao,\n\nMa xac thuc OTP cua ban cho thao tac [" + purposeText + "] la: " + otpCode +
                                 "\nMa co hieu luc trong 5 phut. Khong chia se ma nay cho bat ky ai.\n";
        int ret = -1;
#ifdef _WIN32
        // PowerShell: mật khẩu đặt trong nháy đơn -> nhân đôi nháy đơn để không thể thoát khỏi chuỗi
        auto psq = [](const std::string& s) {
            std::string o;
            for (char c : s) { if (c == '\'') o += "''"; else o += c; }
            return o;
        };
        const std::string psPath = "send_otp_" + rnd + ".ps1";
        {
            std::ofstream ps(psPath);
            if (!ps) {
                std::cerr << "[SMTP] Không thể tạo file lệnh gửi OTP." << std::endl;
                return false;
            }
            ps << "$sec = ConvertTo-SecureString '" << psq(cfg.pass) << "' -AsPlainText -Force\n";
            ps << "$cred = New-Object System.Management.Automation.PSCredential ('" << psq(cfg.user) << "', $sec)\n";
            ps << "Send-MailMessage -From '" << psq(cfg.user) << "' -To '" << psq(toEmail) << "' -Subject '" << psq(subject)
               << "' -Body '" << psq(body) << "' -SmtpServer 'smtp.gmail.com' -Port 587 -UseSsl -Credential $cred -ErrorAction Stop\n";
        }
        ret = std::system(("powershell -NoProfile -ExecutionPolicy Bypass -File " + psPath + " > nul 2>&1").c_str());
        std::remove(psPath.c_str());
#else
        // Linux/macOS: dùng curl; thông tin đăng nhập nằm trong file cấu hình tạm (không lộ trên dòng lệnh)
        const std::string mailPath = "/tmp/otp_mail_" + rnd + ".txt";
        const std::string cfgPath = "/tmp/otp_curl_" + rnd + ".cfg";
        {
            std::ofstream m(mailPath);
            std::ofstream c(cfgPath);
            if (!m || !c) {
                std::cerr << "[SMTP] Không thể tạo file tạm để gửi OTP." << std::endl;
                m.close();
                c.close();
                std::remove(mailPath.c_str());
                std::remove(cfgPath.c_str());
                return false;
            }
            m << "From: " << cfg.user << "\r\nTo: " << toEmail << "\r\nSubject: " << subject << "\r\n\r\n" << body;
            std::string safePass;
            for (char ch : cfg.pass) { if (ch == '"' || ch == '\\') safePass += '\\'; safePass += ch; }
            c << "user = \"" << cfg.user << ":" << safePass << "\"\n";
            c << "url = \"smtps://smtp.gmail.com:465\"\n";
            c << "mail-from = \"" << cfg.user << "\"\n";
            c << "mail-rcpt = \"" << toEmail << "\"\n";
            c << "upload-file = \"" << mailPath << "\"\n";
        }
        const int chmodResult = std::system(("chmod 600 " + cfgPath + " " + mailPath).c_str());
        if (chmodResult != 0) {
            std::cerr << "[SMTP] Không thể giới hạn quyền file cấu hình SMTP." << std::endl;
            std::remove(mailPath.c_str());
            std::remove(cfgPath.c_str());
            return false;
        }
        ret = std::system(("curl -sS --max-time 30 -K " + cfgPath + " > /dev/null").c_str());
        std::remove(mailPath.c_str());
        std::remove(cfgPath.c_str());
#endif
        if (ret == 0) {
            std::cout << "[SMTP] Đã gửi OTP tới " << toEmail << std::endl;
            return true;
        }
        std::cerr << "[SMTP] Gửi OTP thất bại (mã thoát: " << ret << "). Kiểm tra Gmail App Password và log SMTP." << std::endl;
        return false;
    }
};
