-- Xóa dữ liệu cũ nếu có
DELETE FROM reports;
DELETE FROM notifications;
DELETE FROM group_messages;
DELETE FROM group_members;
DELETE FROM groups;
DELETE FROM messages;
DELETE FROM comments;
DELETE FROM reactions;
DELETE FROM posts;
DELETE FROM friend_requests;
DELETE FROM friendships;
DELETE FROM users;

-- 1. DANH SÁCH 10 TÀI KHOẢN NGƯỜI DÙNG (CÓ ADMIN VÀ 9 USERS)
INSERT INTO users (id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token) VALUES
('ACC000000', 'admin', 'admin@mangxahoi.vn', 'Admin@123', 'Quản Trị Viên Hệ Thống', '/img/default-avatar.svg', 'Ban quản trị và giám sát hệ thống Mạng Xã Hội Mini', 'Hà Nội', 'Đại học Bách Khoa Hà Nội', 'Admin Team', 'Lập trình;Bảo mật;Thuật toán', 'ADMIN', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-01 00:00:00', 'Nam', '1995-01-01', 'Hà Nội', 'adm_token_999999'),
('ACC000001', 'khoanm', 'khoanm@gmail.com', 'Khoa@123', 'Nguyễn Minh Khoa', '/img/default-avatar.svg', 'Sống chậm lại và tận hưởng từng khoảnh khắc cuộc sống.', 'Đà Nẵng', 'Đại học Bách Khoa Hà Nội', 'Tiki', 'Đọc sách;Nghe nhạc;Lập trình;Bóng đá', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-05 10:00:00', 'Nam', '2001-04-12', 'Hà Nội', '504258007eab0d25'),
('ACC000002', 'lanph', 'lanph@gmail.com', 'Lan@123', 'Phạm Hương Lan', '/img/default-avatar.svg', 'Yêu cái đẹp, thích nhiếp ảnh và những quán cà phê góc phố.', 'Hà Nội', 'Đại học Ngoại Thương', 'VNG Corporation', 'Nhiếp ảnh;Du lịch;Cà phê;Nghe nhạc', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-06 11:30:00', 'Nữ', '2002-08-20', 'Hà Nội', 'b7d2109f3e4a5c81'),
('ACC000003', 'tuananh', 'tuananh@gmail.com', 'Tuan@123', 'Vũ Tuấn Anh', '/img/default-avatar.svg', 'Kỹ sư phần mềm đam mê thuật toán và trí tuệ nhân tạo.', 'Hải Phòng', 'Đại học Bách Khoa Hà Nội', 'FPT Software', 'Lập trình;Chơi game;Bóng rổ;Đọc sách', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-08 09:15:00', 'Nam', '2000-11-05', 'Hà Nội', 'c8e3210a4f5b6d92'),
('ACC000004', 'ngocanh', 'ngocanh@gmail.com', 'Anh@123', 'Phạm Ngọc Anh', '/img/default-avatar.svg', 'Design is thinking made visual. Yêu nghệ thuật và màu sắc.', 'Đà Nẵng', 'Đại học Kiến trúc Hà Nội', 'Shopee', 'Hội họa;Nhiếp ảnh;Du lịch', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-10 14:20:00', 'Nữ', '2002-03-15', 'Đà Nẵng', 'd9f4321b5a6c7e03'),
('ACC000005', 'hieutran', 'hieutran@gmail.com', 'Hieu@123', 'Trần Minh Hiếu', '/img/default-avatar.svg', 'Chạy bộ mỗi sáng và kết nối với những người bạn tích cực.', 'TP. Hồ Chí Minh', 'Đại học Kinh tế TP.HCM', 'MoMo', 'Chạy bộ;Bóng đá;Đọc sách', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-12 16:45:00', 'Nam', '1999-07-28', 'TP. Hồ Chí Minh', 'e0a5432c6b7d8f14'),
('ACC000006', 'myduyen', 'myduyen@gmail.com', 'Duyen@123', 'Ngô Mỹ Duyên', '/img/default-avatar.svg', 'Cuộc đời là những chuyến đi khám phá ẩm thực và văn hóa.', 'Cần Thơ', 'Đại học Cần Thơ', 'Grab', 'Du lịch;Nấu ăn;Yoga;Cà phê', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-15 08:10:00', 'Nữ', '2001-09-09', 'Cần Thơ', 'f1b6543d7c8e9a25'),
('ACC000007', 'hoangnam', 'hoangnam@gmail.com', 'Nam@123', 'Lê Hoàng Nam', '/img/default-avatar.svg', 'Đam mê bóng đá, bơi lội và công nghệ viễn thông mới.', 'Hà Nội', 'Đại học Bách Khoa Hà Nội', 'Viettel Telecom', 'Bóng đá;Bơi lội;Xem phim', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-18 13:00:00', 'Nam', '2000-02-18', 'Hà Nội', 'a2c7654e8d9f0b36'),
('ACC000008', 'thuha', 'thuha@gmail.com', 'Ha@123', 'Đỗ Thu Hà', '/img/default-avatar.svg', 'Thích sự yên bình, viết lách, sách và những giai điệu acoustic.', 'Đà Nẵng', 'Đại học Ngoại Thương', 'VinFast', 'Nghe nhạc;Viết lách;Du lịch', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-20 17:30:00', 'Nữ', '2003-05-22', 'Đà Nẵng', 'b3d8765f9e0a1c47'),
('ACC000009', 'quanghuy', 'quanghuy@gmail.com', 'Huy@123', 'Bùi Quang Huy', '/img/default-avatar.svg', 'Lập trình viên C++, đam mê eSports và thể thao đối kháng.', 'Hải Phòng', 'Đại học Hàng hải Việt Nam', 'CMC Global', 'Chơi game;Cầu lông;Lập trình', 'USER', 'ACTIVE', '', datetime('now', '+7 hours'), '2026-10-22 19:00:00', 'Nam', '2001-12-30', 'Hải Phòng', 'c4e9876a0f1b2d58');

-- 2. MẠNG LƯỚI KẾT BẠN (FRIENDSHIPS 2 CHIỀU)
INSERT INTO friendships (user_id_1, user_id_2, status, created_at) VALUES
('ACC000001', 'ACC000002', 'FRIEND', '2026-10-25 08:00:00'),
('ACC000002', 'ACC000001', 'FRIEND', '2026-10-25 08:00:00'),
('ACC000001', 'ACC000003', 'FRIEND', '2026-10-25 08:30:00'),
('ACC000003', 'ACC000001', 'FRIEND', '2026-10-25 08:30:00'),
('ACC000001', 'ACC000004', 'FRIEND', '2026-10-25 09:00:00'),
('ACC000004', 'ACC000001', 'FRIEND', '2026-10-25 09:00:00'),
('ACC000001', 'ACC000005', 'FRIEND', '2026-10-25 09:30:00'),
('ACC000005', 'ACC000001', 'FRIEND', '2026-10-25 09:30:00'),

('ACC000002', 'ACC000004', 'FRIEND', '2026-10-26 10:00:00'),
('ACC000004', 'ACC000002', 'FRIEND', '2026-10-26 10:00:00'),
('ACC000002', 'ACC000006', 'FRIEND', '2026-10-26 10:30:00'),
('ACC000006', 'ACC000002', 'FRIEND', '2026-10-26 10:30:00'),
('ACC000002', 'ACC000008', 'FRIEND', '2026-10-26 11:00:00'),
('ACC000008', 'ACC000002', 'FRIEND', '2026-10-26 11:00:00'),

('ACC000003', 'ACC000005', 'FRIEND', '2026-10-27 12:00:00'),
('ACC000005', 'ACC000003', 'FRIEND', '2026-10-27 12:00:00'),
('ACC000003', 'ACC000007', 'FRIEND', '2026-10-27 12:30:00'),
('ACC000007', 'ACC000003', 'FRIEND', '2026-10-27 12:30:00'),
('ACC000003', 'ACC000009', 'FRIEND', '2026-10-27 13:00:00'),
('ACC000009', 'ACC000003', 'FRIEND', '2026-10-27 13:00:00'),

('ACC000004', 'ACC000008', 'FRIEND', '2026-10-28 14:00:00'),
('ACC000008', 'ACC000004', 'FRIEND', '2026-10-28 14:00:00'),
('ACC000005', 'ACC000007', 'FRIEND', '2026-10-28 15:00:00'),
('ACC000007', 'ACC000005', 'FRIEND', '2026-10-28 15:00:00'),
('ACC000006', 'ACC000008', 'FRIEND', '2026-10-29 16:00:00'),
('ACC000008', 'ACC000006', 'FRIEND', '2026-10-29 16:00:00'),
('ACC000007', 'ACC000009', 'FRIEND', '2026-10-29 17:00:00'),
('ACC000009', 'ACC000007', 'FRIEND', '2026-10-29 17:00:00');

-- 3. LỜI MỜI KẾT BẠN ĐANG CHỜ (PENDING FRIEND REQUESTS)
INSERT INTO friend_requests (requester_id, receiver_id, status, created_at) VALUES
('ACC000007', 'ACC000001', 'PENDING', '2026-11-04 14:00:00'),
('ACC000008', 'ACC000001', 'PENDING', '2026-11-04 15:30:00');

-- 4. 10 BÀI ĐĂNG PHONG PHÚ (CÓ TIÊU ĐỀ, NỘI DUNG, HÌNH ẢNH, QUYỀN RIÊNG TƯ)
INSERT INTO posts (id, author_id, title, content, media_type, media_url, shared_post_id, shared_author_name, shared_content, created_at, visibility) VALUES
('POST000001', 'ACC000001', 'Ra mắt Mạng Xã Hội Mini nền tảng C++ Web siêu tốc', 'Chào mừng mọi người đến với Mạng Xã Hội Mini phiên bản C++ Web! Tốc độ mượt mà, hỗ trợ gửi hình ảnh, video và thuật toán BFS gợi ý bạn bè chuẩn xác! 🚀✨', 'NONE', '', '', '', '', '2026-11-04 08:30:00', 'PUBLIC'),
('POST000002', 'ACC000002', 'Cà phê sáng mùa thu Hà Nội', 'Hôm nay trời Hà Nội se lạnh, nhâm nhi một tách cà phê trứng và ngắm phố phường thật tuyệt vời. ☕🍂 Chúc cả nhà một ngày làm việc tràn đầy năng lượng và sáng tạo!', 'IMAGE', 'https://images.unsplash.com/photo-1501339847302-ac426a4a7cbb?auto=format&fit=crop&w=800&q=80', '', '', '', '2026-11-04 09:15:00', 'PUBLIC'),
('POST000003', 'ACC000003', 'Tối ưu hóa hiệu năng cấu trúc dữ liệu Graph trong C++', 'Vừa hoàn thành module tính toán BFS cho thuật toán gợi ý bạn bè tầng 2 và tầng 3. Tận dụng Adjacency List kết hợp unordered_set giúp tốc độ duyệt nhanh hơn 10x so với ma trận kề truyền thống! 💻⚡', 'NONE', '', '', '', '', '2026-11-04 10:20:00', 'PUBLIC'),
('POST000004', 'ACC000004', 'Bộ thiết kế UI/UX mới cho ứng dụng di động', 'Mới hoàn thiện xong thiết kế cho dự án mới với tone màu xanh chủ đạo kết hợp typography tối giản. Mọi người cho mình xin một vài góp ý để hoàn thiện hơn nhé! 🎨📱', 'IMAGE', 'https://images.unsplash.com/photo-1507238691740-187a5b1d37b8?auto=format&fit=crop&w=800&q=80', '', '', '', '2026-11-04 11:45:00', 'PUBLIC'),
('POST000005', 'ACC000005', 'Hoàn thành cự ly chạy 10km sáng Chủ Nhật', 'Chạy bộ dọc cung đường bến Bạch Đằng sáng sớm không khí thật trong lành. 10km hoàn thành trong 52 phút, vượt mục tiêu tuần này! Đã có ai dậy sớm rèn luyện sức khỏe chưa nào? 🏃‍♂️🔥', 'IMAGE', 'https://images.unsplash.com/photo-1452626038306-9aae5e071dd3?auto=format&fit=crop&w=800&q=80', '', '', '', '2026-11-04 13:10:00', 'PUBLIC'),
('POST000006', 'ACC000006', 'Thử tài làm bánh mì hoa cúc tại nhà', 'Mẻ bánh mì hoa cúc thơm phức mùi bơ sữa vừa ra lò. Thớ bánh mềm dai chuẩn vị Pháp, thành công ngoài mong đợi trong những ngày cuối tuần thư giãn. 🍞🧈✨', 'IMAGE', 'https://images.unsplash.com/photo-1509440159596-0249088772ff?auto=format&fit=crop&w=800&q=80', '', '', '', '2026-11-04 14:35:00', 'PUBLIC'),
('POST000007', 'ACC000007', 'Trận cầu đỉnh cao cuối tuần', 'Trận bóng đá giao hữu giữa hai chi nhánh diễn ra vô cùng kịch tính đến những phút bù giờ cuối cùng. Đội nhà lội ngược dòng thắng nghẹt thở 4-3! Cảm ơn anh em đã cháy hết mình! ⚽🥅🏆', 'NONE', '', '', '', '', '2026-11-04 15:50:00', 'PUBLIC'),
('POST000008', 'ACC000008', 'Góc làm việc nhỏ và những cuốn sách yêu thích', 'Dành trọn buổi chiều để nghiền ngẫm cuốn "Tâm lý học về tiền" và "Sức mạnh của thói quen". Không gian làm việc ngăn nắp luôn là nguồn cảm hứng lớn nhất giúp mình tập trung cao độ. 📚🌿☕', 'IMAGE', 'https://images.unsplash.com/photo-1499750310107-5fef28a66643?auto=format&fit=crop&w=800&q=80', '', '', '', '2026-11-04 16:40:00', 'PUBLIC'),
('POST000009', 'ACC000009', 'Trải nghiệm lập trình giao diện Web bằng C++', 'Nhiều người nghĩ C++ chỉ để viết embedded hay game engine, nhưng khi kết hợp httplib với kiến trúc RESTful và SQLite, ta có một web server siêu nhẹ, khởi động chỉ mất 10ms và tiêu tốn chưa tới 15MB RAM! 🚀💻', 'NONE', '', '', '', '', '2026-11-04 17:15:00', 'PUBLIC'),
('POST000010', 'ACC000000', 'Thông báo nâng cấp hệ sinh thái Mạng Xã Hội Mini v2.0', 'Ban Quản Trị xin thông báo hệ thống đã hoàn tất nâng cấp các tính năng: Hộp thoại sở thích 283 lựa chọn chuẩn, Phân loại đoạn chat theo bạn bè & nhóm, Khung tìm kiếm thời gian thực thông minh và Báo cáo vi phạm tự động. Chúc các bạn có trải nghiệm tuyệt vời! 🛡️🌐🎉', 'NONE', '', '', '', '', '2026-11-04 18:00:00', 'PUBLIC');

-- 5. BÌNH LUẬN & TRẢ LỜI BÌNH LUẬN CHO TỪNG BÀI ĐĂNG
INSERT INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES
('CMT000001', 'POST000001', 'ACC000002', 'Giao diện web trực quan, chế độ Dark Mode xịn sò quá bạn ơi! 😍', '', '', '2026-11-04 08:45:00'),
('CMT000002', 'POST000001', 'ACC000003', 'Chuẩn luôn, chạy C++ Backend phản hồi tức thì luôn bạn ơi!', 'CMT000001', 'Phạm Hương Lan', '2026-11-04 09:05:00'),
('CMT000003', 'POST000001', 'ACC000005', 'Chúc mừng team, giao diện nhìn rất hiện đại và tinh tế.', '', '', '2026-11-04 09:20:00'),

('CMT000004', 'POST000002', 'ACC000004', 'Quán cà phê góc phố nào thế nàng ơi, nhìn ấm cúng quá! ☕', '', '', '2026-11-04 09:30:00'),
('CMT000005', 'POST000002', 'ACC000002', 'Góc Đinh Tiên Hoàng nhìn ra hồ Hoàn Kiếm đấy cậu ơi.', 'CMT000004', 'Phạm Ngọc Anh', '2026-11-04 09:45:00'),
('CMT000006', 'POST000002', 'ACC000008', 'Hà Nội mùa này thời tiết đẹp nhất trong năm rồi.', '', '', '2026-11-04 10:00:00'),

('CMT000007', 'POST000003', 'ACC000001', 'Tuyệt vời bác ơi, độ phức tạp O(V + E) chạy mượt mà ngay cả với hàng triệu quan hệ!', '', '', '2026-11-04 10:35:00'),
('CMT000008', 'POST000003', 'ACC000009', 'Cho xin link repo tham khảo với bro ơi, mình cũng đang nghiên cứu.', '', '', '2026-11-04 10:50:00'),

('CMT000009', 'POST000004', 'ACC000002', 'Màu sắc hài hòa và khoảng cách các thành phần rất thoáng mắt bạn ạ!', '', '', '2026-11-04 12:00:00'),
('CMT000010', 'POST000004', 'ACC000006', 'Rất thích cách bố trí nút hành động, nhìn rất thuận tay người dùng.', '', '', '2026-11-04 12:15:00'),

('CMT000011', 'POST000005', 'ACC000007', 'Pace 5.12 thế này là quá khủng rồi bác Hiếu ơi, hôm nào giao lưu nhé!', '', '', '2026-11-04 13:30:00'),
('CMT000012', 'POST000005', 'ACC000005', 'Cuối tuần sau làm một cung cầu Phú Mỹ nhé Nam!', 'CMT000011', 'Lê Hoàng Nam', '2026-11-04 13:45:00'),
('CMT000013', 'POST000005', 'ACC000001', 'Đáng nể thật, em vẫn chưa vượt qua được cơn buồn ngủ lúc 5h sáng haha.', '', '', '2026-11-04 14:00:00'),

('CMT000014', 'POST000006', 'ACC000008', 'Nhìn thớ bánh nở bông xốp thèm quá chị Duyên ơi!', '', '', '2026-11-04 14:50:00'),
('CMT000015', 'POST000006', 'ACC000002', 'Chia sẻ công thức làm bánh với nào nàng ơi 😍', '', '', '2026-11-04 15:10:00'),

('CMT000016', 'POST000007', 'ACC000003', 'Quả sút xa ở phút 88 của ông Nam cháy lưới luôn, đỉnh chóp!', '', '', '2026-11-04 16:05:00'),
('CMT000017', 'POST000007', 'ACC000007', 'Nhờ đường chuyền dọn cỗ của Tuấn Anh đấy chứ!', 'CMT000016', 'Vũ Tuấn Anh', '2026-11-04 16:15:00'),

('CMT000018', 'POST000008', 'ACC000001', 'Cuốn "Tâm lý học về tiền" rất đáng đọc, có nhiều góc nhìn về quản lý tài chính rất thực tế.', '', '', '2026-11-04 16:55:00'),
('CMT000019', 'POST000008', 'ACC000004', 'Setup góc bàn làm việc xinh xắn và nhiều ánh sáng tự nhiên quá.', '', '', '2026-11-04 17:05:00'),

('CMT000020', 'POST000009', 'ACC000003', 'C++20 với coroutines còn đỉnh nữa bác ạ, performance ăn đứt Nodejs!', '', '', '2026-11-04 17:30:00'),
('CMT000021', 'POST000009', 'ACC000001', 'Đúng vậy, hệ thống chạy cực kỳ ổn định và kiểm soát tài nguyên tối đa.', 'CMT000020', 'Vũ Tuấn Anh', '2026-11-04 17:40:00'),

('CMT000022', 'POST000010', 'ACC000002', 'Cảm ơn BQT, các tính năng mới dùng rất thích và trực quan!', '', '', '2026-11-04 18:15:00'),
('CMT000023', 'POST000010', 'ACC000003', 'Giao diện mượt mà và không còn lỗi giật lag nào nữa.', '', '', '2026-11-04 18:25:00');

-- 6. CẢM XÚC PHONG PHÚ CHO BÀI ĐĂNG (LIKE, LOVE, HAHA, WOW, SAD, ANGRY)
INSERT INTO reactions (post_id, user_id, type, created_at) VALUES
('POST000001', 'ACC000002', 'LOVE', '2026-11-04 08:35:00'),
('POST000001', 'ACC000003', 'HAHA', '2026-11-04 08:40:00'),
('POST000001', 'ACC000004', 'LIKE', '2026-11-04 08:50:00'),
('POST000002', 'ACC000001', 'LOVE', '2026-11-04 09:20:00'),
('POST000002', 'ACC000004', 'WOW', '2026-11-04 09:25:00'),
('POST000002', 'ACC000006', 'LIKE', '2026-11-04 09:30:00'),
('POST000003', 'ACC000001', 'LIKE', '2026-11-04 10:25:00'),
('POST000003', 'ACC000009', 'WOW', '2026-11-04 10:30:00'),
('POST000003', 'ACC000007', 'HAHA', '2026-11-04 10:40:00'),
('POST000004', 'ACC000002', 'LOVE', '2026-11-04 11:50:00'),
('POST000004', 'ACC000008', 'LIKE', '2026-11-04 12:00:00'),
('POST000005', 'ACC000007', 'LIKE', '2026-11-04 13:15:00'),
('POST000005', 'ACC000001', 'LOVE', '2026-11-04 13:20:00'),
('POST000006', 'ACC000002', 'LOVE', '2026-11-04 14:40:00'),
('POST000006', 'ACC000008', 'WOW', '2026-11-04 14:45:00'),
('POST000007', 'ACC000003', 'LIKE', '2026-11-04 15:55:00'),
('POST000007', 'ACC000005', 'HAHA', '2026-11-04 16:00:00'),
('POST000008', 'ACC000001', 'LOVE', '2026-11-04 16:45:00'),
('POST000008', 'ACC000004', 'LIKE', '2026-11-04 16:50:00'),
('POST000009', 'ACC000001', 'LIKE', '2026-11-04 17:20:00'),
('POST000009', 'ACC000003', 'WOW', '2026-11-04 17:25:00'),
('POST000010', 'ACC000001', 'LOVE', '2026-11-04 18:05:00'),
('POST000010', 'ACC000002', 'LIKE', '2026-11-04 18:10:00'),
('POST000010', 'ACC000005', 'WOW', '2026-11-04 18:12:00');

-- 7. NHÓM CHAT MẪU
INSERT INTO groups (id, name, description, avatar, created_by, created_at) VALUES
('GRP000001', 'Cộng Đồng Lập Trình C++ Việt Nam', 'Nơi giao lưu chia sẻ thuật toán, cấu trúc dữ liệu và phát triển ứng dụng', '/img/default-avatar.svg', 'ACC000001', '2026-10-25 08:00:00'),
('GRP000002', 'Hội Nhiếp Ảnh & Cà Phê Hà Nội', 'Giao lưu các góc chụp đẹp và địa điểm cà phê thư giãn cuối tuần', '/img/default-avatar.svg', 'ACC000002', '2026-10-26 09:00:00');

INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES
('GRP000001', 'ACC000001', 'ADMIN', '2026-10-25 08:00:00'),
('GRP000001', 'ACC000002', 'MEMBER', '2026-10-25 08:10:00'),
('GRP000001', 'ACC000003', 'MEMBER', '2026-10-25 08:20:00'),
('GRP000001', 'ACC000009', 'MEMBER', '2026-10-25 08:30:00'),
('GRP000002', 'ACC000002', 'ADMIN', '2026-10-26 09:00:00'),
('GRP000002', 'ACC000001', 'MEMBER', '2026-10-26 09:15:00'),
('GRP000002', 'ACC000004', 'MEMBER', '2026-10-26 09:30:00'),
('GRP000002', 'ACC000006', 'MEMBER', '2026-10-26 09:45:00');

INSERT INTO group_messages (group_id, sender_id, content, shared_post_id, created_at) VALUES
('GRP000001', 'ACC000001', 'Chào mừng mọi người tham gia nhóm thảo luận công nghệ C++!', '', '2026-11-04 09:00:00'),
('GRP000001', 'ACC000003', 'Xin chào cả nhà! Nhóm hoạt động sôi nổi quá.', '', '2026-11-04 09:05:00'),
('GRP000002', 'ACC000002', 'Cuối tuần này ai rảnh đi chụp ảnh cà phê hồ Tây không?', '', '2026-11-04 10:15:00');

-- 8. TIN NHẮN 1-ON-1 MẪU GIỮA BẠN BÈ (CÓ TIN CHƯA ĐỌC VÀ CHIA SẺ BÀI VIẾT)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, shared_post_id, is_read, created_at) VALUES
('ACC000001:ACC000002', 'ACC000001', 'ACC000002', 'Chào Lan, hôm nay bạn có ghé quán cà phê quen không?', '', 1, '2026-11-04 09:30:00'),
('ACC000001:ACC000002', 'ACC000002', 'ACC000001', 'Chào Khoa, mình đang ngồi đây này, góc hồ Hoàn Kiếm đẹp lắm!', '', 0, '2026-11-04 09:35:00'),
('ACC000001:ACC000003', 'ACC000003', 'ACC000001', 'Mình vừa chia sẻ bài viết về thuật toán Graph, Khoa xem thử nhé!', 'POST000003', 1, '2026-11-04 10:40:00'),
('ACC000001:ACC000004', 'ACC000004', 'ACC000001', 'Khoa ơi xem giúp mình bộ thiết kế UI mới nhé!', 'POST000004', 1, '2026-11-04 12:10:00');

-- 9. THÔNG BÁO MẪU CHO TÀI KHOẢN KHOANM (ACC000001)
INSERT INTO notifications (user_id, actor_id, type, content, link, is_read, created_at) VALUES
('ACC000001', 'ACC000002', 'LIKE', 'Phạm Hương Lan đã thả tim bài viết của bạn.', '/#post-POST000001', 0, '2026-11-04 08:35:00'),
('ACC000001', 'ACC000003', 'COMMENT', 'Vũ Tuấn Anh đã bình luận về bài viết của bạn.', '/#post-POST000001', 0, '2026-11-04 09:05:00'),
('ACC000001', 'ACC000007', 'FRIEND_REQUEST', 'Lê Hoàng Nam đã gửi cho bạn lời mời kết bạn.', 'friends.html?tab=requests', 0, '2026-11-04 14:00:00'),
('ACC000001', 'ACC000008', 'FRIEND_REQUEST', 'Đỗ Thu Hà đã gửi cho bạn lời mời kết bạn.', 'friends.html?tab=requests', 0, '2026-11-04 15:30:00');

-- 10. BÁO CÁO MẪU CHO ADMIN XỬ LÝ (TAB ADMIN)
INSERT INTO reports (reporter_id, reported_user_id, reported_post_id, reported_comment_id, reason, status, created_at) VALUES
('ACC000003', '', 'POST000009', '', 'Yêu cầu kiểm duyệt nội dung kỹ thuật chuyên sâu', 'PENDING', '2026-11-04 17:35:00'),
('ACC000005', 'ACC000007', '', '', 'Báo cáo spam lời mời giao lưu thể thao', 'PENDING', '2026-11-04 16:30:00');
