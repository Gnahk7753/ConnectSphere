# ConnectSphere

ConnectSphere là ứng dụng mạng xã hội mini, cung cấp các chức năng kết nối và tương tác trong một giao diện web thân thiện. Dự án được xây dựng với máy chủ C++ và cơ sở dữ liệu SQLite, kết hợp giao diện HTML, CSS và JavaScript.

## Tính năng

- **Tài khoản và hồ sơ:** đăng ký, đăng nhập, xác thực email bằng OTP và quản lý trang cá nhân.
- **Quyền riêng tư:** người dùng cần đồng ý Điều khoản sử dụng và Chính sách quyền riêng tư trước khi đăng nhập hoặc đăng ký.
- **Bảng tin:** đăng bài, chia sẻ hình ảnh/video, bày tỏ cảm xúc và bình luận, bao gồm phản hồi bình luận.
- **Kết nối bạn bè:** gửi và xử lý lời mời kết bạn, tìm kiếm người dùng và xem gợi ý kết bạn.
- **Nhóm cộng đồng:** tạo nhóm, tham gia nhóm và đăng bài trong nhóm.
- **Nhắn tin:** trò chuyện trực tiếp và nhắn tin nhóm.
- **Tìm kiếm và thông báo:** tìm người dùng, nhóm, bài viết; nhận thông báo về các hoạt động liên quan.
- **Quản trị:** bảng điều khiển quản trị hỗ trợ theo dõi hoạt động và xử lý báo cáo.
- **Tích hợp tùy chọn:** Gemini hỗ trợ phân loại nội dung; Google Sign-In có thể bật khi cấu hình thông tin xác thực.

## Công nghệ

- **Backend:** C++17, `cpp-httplib`
- **Cơ sở dữ liệu:** SQLite
- **Giao diện:** HTML, CSS, JavaScript
- **Dữ liệu API:** JSON (`nlohmann-json`)

Máy chủ phục vụ cả API và các trang giao diện trong thư mục `static/`. Dữ liệu SQLite mặc định nằm tại `data/social_network.db`.

## Chạy ứng dụng trên Windows

### Yêu cầu

- Windows với MSYS2 UCRT64 hoặc MinGW
- g++ hỗ trợ C++17
- SQLite và nlohmann-json

### Build và khởi động

Mở terminal đã cấu hình môi trường biên dịch, tại thư mục dự án chạy:

```bat
build.bat
server.exe
```

Sau khi máy chủ khởi động, mở [http://localhost:8080](http://localhost:8080) trên trình duyệt. Có thể dùng `run.bat` để build khi cần và khởi động máy chủ.

## Deploy lên Render

Dự án có thể build trên Linux bằng CMake và deploy trên Render qua Docker:

1. Đẩy mã nguồn, `Dockerfile` và `.dockerignore` lên GitHub. Không đưa `server.exe`, database cục bộ hoặc file chứa bí mật lên repository.
2. Trên Render, chọn **New → Web Service**, kết nối repository và chọn runtime **Docker**.
3. Trong **Environment**, thiết lập:
   - `PRODUCTION=1`
   - `ADMIN_PASSWORD` với mật khẩu quản trị mạnh
   - `COOKIE_SECURE=1`
   - `TRUST_PROXY=1`
   - Thêm `SMTP_EMAIL` và `SMTP_PASSWORD` nếu cần gửi OTP; `GEMINI_API_KEY` hoặc `GOOGLE_CLIENT_ID` nếu dùng các tích hợp tương ứng.
4. Để dữ liệu không mất sau khi service khởi động lại hoặc deploy, gắn persistent disk tại `/app/data`. Ứng dụng lưu SQLite tại `data/social_network.db`.
5. Deploy. Ứng dụng tự đọc `PORT` do Render cấp; không cần đặt cổng cố định.

Không bật `SEED_DEMO_DATA` trên production. Cơ sở dữ liệu local `data/social_network.db` bị loại khỏi Docker image; ứng dụng khởi tạo database mới khi chạy.

## Cấu hình tùy chọn

Có thể truyền cấu hình qua biến môi trường hoặc file `.env` cục bộ. Một số tích hợp dùng các biến sau:

| Biến | Mục đích |
| --- | --- |
| `SMTP_EMAIL`, `SMTP_PASSWORD` | Gửi email OTP |
| `GEMINI_API_KEY` | Kết nối Gemini API |
| `GOOGLE_CLIENT_ID` | Cấu hình Google Sign-In |
| `ADMIN_PASSWORD` | Đặt mật khẩu tài khoản quản trị |
| `SEED_DEMO_DATA=1` | Nạp dữ liệu mẫu khi khởi tạo cơ sở dữ liệu trống |

Để gửi OTP qua Gmail, `SMTP_PASSWORD` phải là **Google App Password** (cần bật xác minh 2 bước), không phải mật khẩu đăng nhập Gmail thông thường.

Không đưa `.env`, mật khẩu hoặc API key vào repository.

## Cấu trúc dự án

```text
src/       Mã nguồn máy chủ và các dịch vụ
include/   Header thư viện dùng chung
static/    Trang HTML, stylesheet, JavaScript và hình ảnh
data/      Script SQL và cơ sở dữ liệu local
tests/     Kiểm thử
```
