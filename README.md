# ConnectSphere

ConnectSphere là ứng dụng mạng xã hội mini, cung cấp các chức năng kết nối và tương tác trong một giao diện web thân thiện. Dự án được xây dựng với máy chủ C++ và cơ sở dữ liệu SQLite, kết hợp giao diện HTML, CSS và JavaScript.

## Tính năng

- **Tài khoản và hồ sơ:** đăng nhập bằng Google hoặc bằng email/tên đăng nhập và mật khẩu; tài khoản mới được tạo qua Google.
- **Quyền riêng tư:** người dùng cần đồng ý Điều khoản sử dụng và Chính sách quyền riêng tư trước khi đăng nhập.
- **Bảng tin:** bài viết tối đa 1.500 ký tự, nội dung dài có thể mở rộng; chia sẻ hình ảnh/video và bình luận bằng văn bản, ảnh, video hoặc GIF.
- **Kết nối bạn bè:** gửi và xử lý lời mời kết bạn, tìm kiếm người dùng và xem gợi ý kết bạn.
- **Nhóm cộng đồng:** tạo nhóm, tham gia nhóm và đăng bài trong nhóm.
- **Nhắn tin:** trò chuyện trực tiếp và nhắn tin nhóm.
- **GIF:** tìm và gửi GIF qua GIPHY, lưu GIF yêu thích theo tài khoản và xóa khỏi danh sách đã lưu.
- **Tìm kiếm và thông báo:** tìm người dùng, nhóm, bài viết; nhận thông báo về các hoạt động liên quan.
- **Quản trị:** bảng điều khiển quản trị hỗ trợ theo dõi hoạt động và xử lý báo cáo.
- **Tích hợp:** Google Sign-In dùng để tạo tài khoản và đăng nhập; Gemini hỗ trợ kiểm duyệt nội dung khi được cấu hình.

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
   - Thêm `GOOGLE_CLIENT_ID` để bật đăng nhập/tạo tài khoản bằng Google. Có thể thêm `GEMINI_API_KEY` để bật kiểm duyệt nội dung bằng Gemini.
   - Thêm `GIPHY_API_KEY` để bật tìm kiếm GIF trong bình luận.
4. Gắn **Persistent Disk** cho service. Trong Environment, đặt `DATABASE_MOUNT_PATH` đúng bằng **Mount Path** của disk và đặt `DATABASE_PATH` trỏ đến file bên trong đó. Ví dụ, nếu Mount Path là `/var/data`, đặt `DATABASE_MOUNT_PATH=/var/data` và `DATABASE_PATH=/var/data/social_network.db`. Máy chủ production sẽ từ chối khởi động nếu thiếu các biến này, nếu DB nằm ngoài mount path hoặc nếu path chưa phải disk đã mount.
5. Deploy. Ứng dụng tự đọc `PORT` do Render cấp; không cần đặt cổng cố định.

Không bật `SEED_DEMO_DATA` trên production. Cơ sở dữ liệu local bị loại khỏi Docker image. Chỉ đặt `DATABASE_PATH` thôi không tạo vùng lưu trữ bền vững; phải gắn Persistent Disk và để `DATABASE_PATH` nằm dưới `DATABASE_MOUNT_PATH`.

## Cấu hình tùy chọn

Có thể truyền cấu hình qua biến môi trường hoặc file `.env` cục bộ. Một số tích hợp dùng các biến sau:

| Biến | Mục đích |
| --- | --- |
| `GEMINI_API_KEY` | Kết nối Gemini để kiểm duyệt bài viết, bình luận và báo cáo |
| `GEMINI_MODEL` | Tùy chọn chọn model Gemini; mặc định `gemini-2.0-flash` |
| `GOOGLE_CLIENT_ID` | Bắt buộc để đăng nhập Google và tạo tài khoản mới |
| `GIPHY_API_KEY` | Bật tìm kiếm GIF trong bình luận |
| `DATABASE_PATH` | Đường dẫn đầy đủ tới file SQLite; trên Render phải nằm dưới mount path của Persistent Disk |
| `DATABASE_MOUNT_PATH` | Mount Path của Render Persistent Disk; bắt buộc khi `PRODUCTION=1` |
| `ADMIN_PASSWORD` | Đặt mật khẩu tài khoản quản trị |
| `SEED_DEMO_DATA=1` | Nạp dữ liệu mẫu khi khởi tạo cơ sở dữ liệu trống |

Không đưa `.env`, mật khẩu hoặc API key vào repository.

Tài khoản mới chỉ được tạo qua Google. Lần đầu đăng nhập Google, người dùng cần đặt tên đăng nhập và mật khẩu riêng của ConnectSphere; sau đó có thể đăng nhập bằng email Google hoặc tên đăng nhập cùng mật khẩu. Mật khẩu Google không được yêu cầu.

## Kiểm duyệt nội dung

Máy chủ kiểm tra văn bản bài viết, bình luận (bao gồm nội dung nhóm) và nội dung được người dùng báo cáo. Khi có `GEMINI_API_KEY`, văn bản được gửi tới Gemini để đánh giá; nếu không có key hoặc API tạm lỗi, máy chủ dùng bộ lọc từ khóa cục bộ nên khả năng nhận diện sẽ hạn chế hơn. Nội dung bị đánh giá vi phạm sẽ bị từ chối hoặc gỡ và người đăng nhận thông báo cảnh cáo. Vi phạm được ghi nhận theo cửa sổ 30 ngày; từ lần thứ tư trong 30 ngày, tài khoản bị khóa và các phiên đăng nhập bị thu hồi. Kết quả báo cáo chưa xác nhận vi phạm vẫn được chuyển cho quản trị viên xem xét.

## Cấu trúc dự án

```text
src/       Mã nguồn máy chủ và các dịch vụ
include/   Header thư viện dùng chung
static/    Trang HTML, stylesheet, JavaScript và hình ảnh
data/      Script SQL và cơ sở dữ liệu local
tests/     Kiểm thử
```
