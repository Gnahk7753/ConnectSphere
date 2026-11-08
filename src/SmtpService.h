#pragma once

#include <iostream>
#include <string>
#include <random>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <thread>
#include "json.hpp"

using json = nlohmann::json;

class SmtpService {
public:
    static std::string generate6DigitOtp() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(100000, 999999);
        return std::to_string(dis(gen));
    }

    /**
     * Gửi mã xác thực OTP 6 số tới Gmail (Tự động gửi qua SMTP Gmail nếu có cấu hình trong config.json)
     */
    static bool sendOtpEmail(const std::string& toEmail, const std::string& otpCode, const std::string& purposeText = "Đăng ký tài khoản") {
        std::cout << "\n========================================================" << std::endl;
        std::cout << " [GMAIL SMTP SERVICE] GỬI EMAIL XÁC THỰC 6 CHỮ SỐ" << std::endl;
        std::cout << " Người nhận : " << toEmail << std::endl;
        std::cout << " Mục đích   : " << purposeText << std::endl;
        std::cout << " MÃ XÁC THỰC: [ " << otpCode << " ] (Hiệu lực 5 phút)" << std::endl;
        std::cout << "========================================================\n" << std::endl;

        // Kiểm tra và khởi tạo file config.json nếu chưa có
        std::string smtpUser = "";
        std::string smtpPass = "";

        std::ifstream inCfg("config.json");
        if (inCfg.is_open()) {
            try {
                json j = json::parse(inCfg);
                smtpUser = j.value("smtp_email", "");
                smtpPass = j.value("smtp_password", "");
            } catch (...) {}
            inCfg.close();
        } else {
            // Tạo file mẫu hướng dẫn
            std::ofstream outCfg("config.json");
            if (outCfg.is_open()) {
                json sample = {
                    {"smtp_email", ""},
                    {"smtp_password", ""},
                    {"huong_dan", "Nhap dia chi Gmail va Mat khau ung dung (16 ky tu tao tai myaccount.google.com/apppasswords) vao day de he thong tu dong gui mail qua Gmail that."}
                };
                outCfg << sample.dump(2);
                outCfg.close();
            }
        }

        // Nếu đã cấu hình Gmail và App Password -> Gửi mail thật qua PowerShell SMTP trong background thread
        if (!smtpUser.empty() && !smtpPass.empty() && toEmail.find("@") != std::string::npos) {
            std::thread([toEmail, otpCode, purposeText, smtpUser, smtpPass]() {
                std::cout << "[GMAIL SMTP] Đang thực hiện gửi email thật tới: " << toEmail << "..." << std::endl;
                std::string subject = "[Mạng Xã Hội Mini] Mã OTP xác thực: " + otpCode;
                std::string body = "Xin chào,\n\nMã xác thực OTP của bạn cho thao tác [" + purposeText + "] là:\n\n   >>> " + otpCode + " <<<\n\nMã này có hiệu lực trong vòng 5 phút. Vui lòng không chia sẻ mã này cho bất kỳ ai.\n\nTrân trọng,\nĐội ngũ Mạng Xã Hội Mini.";

                // Tạo script PowerShell tạm thời để gửi mail an toàn, tránh lỗi escape ký tự đặc biệt
                std::string psScriptPath = "send_otp_temp.ps1";
                std::ofstream psFile(psScriptPath);
                if (psFile.is_open()) {
                    psFile << "$sec = ConvertTo-SecureString '" << smtpPass << "' -AsPlainText -Force\n";
                    psFile << "$cred = New-Object System.Management.Automation.PSCredential ('" << smtpUser << "', $sec)\n";
                    psFile << "$body = @'\n" << body << "\n'@\n";
                    psFile << "Send-MailMessage -From '" << smtpUser << "' -To '" << toEmail << "' -Subject '" << subject << "' -Body $body -Encoding ([System.Text.Encoding]::UTF8) -SmtpServer 'smtp.gmail.com' -Port 587 -UseSsl -Credential $cred -ErrorAction Stop\n";
                    psFile.close();

                    std::string cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File send_otp_temp.ps1 > nul 2>&1";
                    int ret = std::system(cmd.c_str());
                    if (ret == 0) {
                        std::cout << " [GMAIL SMTP] Gửi email thành công tới: " << toEmail << std::endl;
                    } else {
                        std::cout << " [GMAIL SMTP] Gửi thất bại qua SMTP (kiểm tra lại Gmail/App Password trong config.json). Mã OTP vẫn có hiệu lực trên server: [" << otpCode << "]" << std::endl;
                    }
                    std::remove(psScriptPath.c_str());
                }
            }).detach();
        } else {
            std::cout << " [GMAIL SMTP LƯU Ý] Chưa cấu hình Gmail App Password trong file config.json." << std::endl;
            std::cout << "   -> Bạn có thể lấy mã OTP trực tiếp từ console: [" << otpCode << "]" << std::endl;
            std::cout << "   -> Hoặc điền 'smtp_email' và 'smtp_password' vào file config.json để gửi mail thật tự động." << std::endl;
        }

        return true;
    }
};
