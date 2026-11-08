# Mạng Xã Hội MIni - Nền Tảng C++ Web Engine & SQL Database

Dự án Web Application mạng xã hội hiện đại được xây dựng hoàn toàn trên nền tảng **C++17** và **SQLite Database**, tích hợp giao diện người dùng tương tác bằng chuột và bàn phím, hoạt động mượt mà trên trình duyệt web.

---

## 🌟 Các tính năng mới và nâng cấp nổi bật

1. **Trang cá nhân độc lập (Dedicated Profile Page - `profile.html?id=...`):**
   - Mỗi người dùng sở hữu một trang web profile riêng biệt.
   - Hiển thị ảnh đại diện, ảnh bìa, họ tên, tiểu sử, thông tin chi tiết (quê quán, trường học, nơi làm việc, sở thích).
   - Danh sách bạn bè kèm liên kết trực tiếp tới trang cá nhân của từng người.
   - Toàn bộ bài viết do người dùng đó đăng tải.
   - Nút hành động trực quan: Thêm bạn bè, Chấp nhận kết bạn, Nhắn tin, Báo cáo người dùng, hoặc Chỉnh sửa hồ sơ cá nhân nếu là trang của chính mình.

2. **Cụm biểu tượng cảm xúc thông minh (Hình 1):**
   - Hiển thị **tối đa 3 biểu tượng cảm xúc được thả nhiều nhất** (thay vì dàn trải cả 6 icon gây rối mắt).
   - Khi lia chuột (hover) vào cụm biểu tượng, tooltip hiển thị danh sách tên những người đã thả cảm xúc đó (ví dụ: *Nguyễn Minh Khoa, Phạm Hương Lan đã thả ❤️, 😆*).

3. **Cửa sổ xem bình luận chuyên dụng (Comment Modal - Hình 2):**
   - Bài viết trên Bảng tin chính không hiển thị bình luận lộn xộn. Khi bấm vào bình luận, một cửa sổ modal mở lên ở giữa màn hình.
   - Hiển thị đầy đủ thông tin bài đăng, hình ảnh/video, thanh thống kê cảm xúc và danh sách bình luận.
   - Hỗ trợ **phản hồi bình luận lồng nhau (Hình 5)**: Tên người được phản hồi hiển thị nổi bật với thẻ `@Tên_Người_Dùng` màu xanh in đậm.
   - Nút **Báo cáo bình luận** (🚩) gửi trực tiếp tới Quản trị viên.
   - Chủ bài viết và người viết bình luận có quyền **Xóa bình luận** (🗑️).

4. **Tìm kiếm thời gian thực & Trang kết quả có bộ lọc (Hình 3 & Hình 4):**
   - **Gợi ý trực tiếp (Hình 3):** Nhập ký tự tới đâu, danh sách gợi ý người dùng và nhóm xuất hiện ngay dưới thanh tìm kiếm.
   - **Trang tìm kiếm riêng (`search.html` - Hình 4):** Khi nhấn Enter, hệ thống chuyển sang trang tìm kiếm chuyên sâu với cột bộ lọc bên trái (*Tất cả, Mọi người, Nhóm, Bài viết*).

5. **Hệ thống thông báo hình cái chuông (Hình 4):**
   - Biểu tượng 🔔 góc trên bên phải thanh điều hướng kèm huy hiệu đếm số thông báo chưa đọc.
   - Bấm vào mở dropdown xem danh sách thông báo: lời mời kết bạn, ai đó thích bài viết, bình luận bài viết của bạn.

6. **Nhắn tin nhóm & Chia sẻ bài viết vào chat:**
   - Hỗ trợ **nhắn tin trong nhóm chat** (`group_messages`).
   - Nút **Chia sẻ bài viết** mở modal cho phép gửi trực tiếp bài viết vào hộp thoại trò chuyện của một người bạn hoặc một nhóm cụ thể.

7. **Thời gian đăng bài chuẩn phong cách Console:**
   - Định dạng thân thiện kế thừa từ mã nguồn console gốc: *"Vừa xong"*, *"X phút trước"*, *"X giờ trước"*, *"X ngày trước"*, *"X tháng trước"*, hoặc ngày tháng năm `DD/MM/YYYY`.

8. **Menu thanh bên trái tinh gọn:**
   - 📰 **Bảng tin chính** (`index.html`)
   - 👥 **Bạn bè** (`friends.html`): Trang riêng hiển thị tất cả bạn bè đã kết bạn, lời mời kết bạn và gợi ý kết bạn kèm bộ lọc (*Tất cả, Cùng trường, Cùng quê, Chung sở thích, Bạn của bạn bè*) và nút *Xem thêm*.
   - 🛠️ **Hỗ trợ**: Mở modal báo cáo sự cố hoặc gửi ý kiến đóng góp trực tiếp tới Quản trị viên.
   - 🛡️ **Quản trị viên** (`admin.html`): Dành riêng cho tài khoản Admin.

9. **Trang Quản trị viên toàn diện (Admin Dashboard - `admin.html`):**
   - Thống kê thời gian thực: Tổng người dùng, tài khoản bị khóa, tổng bài viết, tổng tin nhắn, báo cáo chờ xử lý, tổng nhóm chat.
   - Xử lý báo cáo vi phạm: Xem lý do báo cáo, đánh dấu đã giải quyết / bỏ qua, khóa tài khoản vi phạm, xóa bài viết vi phạm.
   - Quản lý người dùng: Bật / tắt khóa tài khoản (Active / Locked).
   - Quản lý bài viết: Xóa bài viết vi phạm trên toàn hệ thống.

10. **Xác thực Gmail OTP 6 số & Đăng nhập Google:**
    - Đăng ký tài khoản xác thực qua mã OTP 6 số gửi về Gmail (tự người dùng nhập, kèm xác nhận mật khẩu).
    - Đăng nhập Google với danh sách tài khoản Google trên trình duyệt web.

---

## 🚀 Hướng dẫn khởi chạy

### Cách 1: Chạy trực tiếp file thực thi
Khởi động file **`server.exe`** hoặc nhấp đúp vào **`run.bat`**. Server sẽ lắng nghe tại cổng `8080`:
👉 Mở trình duyệt truy cập: **`http://localhost:8080`**

### Cách 2: Tự biên dịch lại mã nguồn C++ (nếu cần chỉnh sửa mã nguồn)
Dùng trình biên dịch `g++` hỗ trợ C++17:
```bash
g++ -std=c++17 src/main.cpp -Iinclude -Isrc -lsqlite3 -lws2_32 -lcrypt32 -O2 -o server.exe
./server.exe
```

---

## 👥 Tài khoản thử nghiệm có sẵn trong SQL Database

| Tên đăng nhập | Mật khẩu | Họ và tên | Vai trò | Chức năng nổi bật |
| :--- | :--- | :--- | :--- | :--- |
| **`admin`** | `Admin@123` | Quản Trị Viên Hệ Thống | ADMIN | Truy cập toàn bộ Admin Dashboard (`admin.html`) |
| **`khoanm76`** | `Khoa@2890c` | Nguyễn Minh Khoa | USER | Đã có bạn bè, bài viết và lịch sử chat |
| **`lanph88`** | `Lan@1928x` | Phạm Hương Lan | USER | Đã có lịch sử tương tác và tin nhắn |
| **`tuananh01`** | `Tuan@7734m` | Vũ Tuấn Anh | USER | Thử nghiệm kết bạn và BFS |

---

## 🤖 Tích hợp Gemini AI (kiểm duyệt & phân loại bài viết)

### Cách tạo API Key Gemini và cấu hình `.env`

1. Truy cập **https://aistudio.google.com/apikey** và đăng nhập Google.
2. Bấm **Create API key** → sao chép key.
3. Trong thư mục dự án, sao chép file mẫu:
   ```bash
   copy .env.example .env
   ```
   (Linux/macOS: `cp .env.example .env`)
4. Mở file `.env`, điền:
   ```
   GEMINI_API_KEY=AIza...key_của_bạn
   GEMINI_MODEL=gemini-2.0-flash
   GEMINI_MODERATION_ENABLED=true
   GEMINI_BLOCK_SEVERE=true
   ```
5. **Không** commit file `.env`, **không** đưa API Key ra Frontend. Server C++ đọc key từ `.env` khi khởi động.
6. Đổi model dễ dàng bằng biến `GEMINI_MODEL` (ví dụ `gemini-1.5-flash`, `gemini-2.5-flash`).
7. Nếu chưa có key, hệ thống vẫn chạy với **phân loại/kiểm duyệt từ khóa local (fallback)**.

### News Feed Ranking

Điểm bài viết:
`score = 0.28*viral + 0.22*interest + 0.15*location + 0.15*school + 0.20*recency`

- Viral dùng `log1p` để bài hot không chiếm hết bảng tin.
- Diversity: tối đa 2 bài liên tiếp cùng 1 người, tối đa 3 bài/người mỗi page.
- Bài `BLOCK` không hiện; bài `FLAG` bị giảm điểm nhẹ.

