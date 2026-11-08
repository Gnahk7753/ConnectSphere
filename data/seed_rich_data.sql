-- ============================================================================
-- SEEDER DỮ LIỆU PHONG PHÚ - Mạng Xã Hội Mini C++
-- Chạy nhiều lần an toàn: dùng INSERT OR IGNORE / OR REPLACE có kiểm soát
-- KHÔNG xóa / ghi đè dữ liệu người dùng hiện có (ACC000000–ACC000009)
-- ============================================================================
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;

-- ---------------------------------------------------------------------------
-- 1. NGƯỜI DÙNG MỚI (ACC000010 – ACC000029) — mật khẩu plaintext như hệ thống hiện tại
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO users (id, username, email, password_hash, fullname, avatar, bio, hometown, school, workplace, interests, role, status, google_id, last_active_at, created_at, gender, birthday, residence, user_token) VALUES
('ACC000010','minhthu','minhthu@gmail.com','Thu@123','Nguyễn Minh Thư','/img/default-avatar.svg','Yêu sách và cà phê sáng.','Hà Nội','Đại học Ngoại Thương','FPT Software','Đọc sách,Cà phê,Du lịch,Yoga','USER','ACTIVE','','2026-11-08 08:10:00','2026-01-15 10:00:00','Nữ','1999-03-12','Hà Nội','tok_minhthu01'),
('ACC000011','ducmanh','ducmanh@gmail.com','Manh@123','Trần Đức Mạnh','/img/default-avatar.svg','Backend C++ & hệ thống phân tán.','Hải Phòng','Đại học Bách Khoa Hà Nội','Viettel','Lập trình,Bóng đá,Cờ vua,Linux','USER','ACTIVE','','2026-11-08 09:20:00','2026-01-20 11:00:00','Nam','1998-07-22','Hà Nội','tok_ducmanh01'),
('ACC000012','hoailinh','hoailinh@gmail.com','Linh@123','Lê Hoài Linh','/img/default-avatar.svg','Designer UI/UX, thích màu pastel.','Huế','Đại học Mỹ thuật Công nghiệp','MoMo','Thiết kế,Nhiếp ảnh,Phim,Âm nhạc','USER','ACTIVE','','2026-11-08 07:45:00','2026-02-01 09:00:00','Nữ','2000-11-05','TP. Hồ Chí Minh','tok_hoailinh01'),
('ACC000013','vanlong','vanlong@gmail.com','Long@123','Phạm Văn Long','/img/default-avatar.svg','Runner 5km mỗi sáng.','Đà Nẵng','Đại học Đà Nẵng','Shopee','Chạy bộ,Thể thao,Du lịch,Nấu ăn','USER','ACTIVE','','2026-11-07 22:00:00','2026-02-10 14:00:00','Nam','1997-01-18','Đà Nẵng','tok_vanlong01'),
('ACC000014','kimanh','kimanh@gmail.com','Anh@1234','Hoàng Kim Anh','/img/default-avatar.svg','Marketing & content creator.','Cần Thơ','Đại học Kinh tế TP.HCM','TikTok Vietnam','Marketing,Làm vlog,Du lịch,Makeup','USER','ACTIVE','','2026-11-08 10:05:00','2026-02-15 16:00:00','Nữ','2001-05-30','TP. Hồ Chí Minh','tok_kimanh01'),
('ACC000015','quocbao','quocbao@gmail.com','Bao@123','Đặng Quốc Bảo','/img/default-avatar.svg','Data science & AI.','Hà Nội','Đại học Công nghệ - ĐHQGHN','VinAI','AI,Machine Learning,Cờ tướng,Cà phê','USER','ACTIVE','','2026-11-08 11:30:00','2026-03-01 08:00:00','Nam','1998-09-09','Hà Nội','tok_quocbao01'),
('ACC000016','thanhtrang','thanhtrang@gmail.com','Trang@123','Vũ Thanh Trang','/img/default-avatar.svg','Giáo viên tiếng Anh, IELTS 8.0.','Nam Định','Đại học Hà Nội','British Council','Ngoại ngữ,Đọc sách,Du lịch,Yoga','USER','ACTIVE','','2026-11-07 18:40:00','2026-03-05 10:00:00','Nữ','1999-12-25','Hà Nội','tok_thanhtrang01'),
('ACC000017','anhtuan','anhtuan2@gmail.com','Tuan@456','Ngô Anh Tuấn','/img/default-avatar.svg','Fullstack JS, thích open source.','Bắc Ninh','Đại học FPT','Shopee','JavaScript,Game,Bóng rổ,Nhạc rock','USER','ACTIVE','','2026-11-08 06:50:00','2026-03-12 12:00:00','Nam','2000-04-14','Hà Nội','tok_anhtuan02'),
('ACC000018','ngocdiep','ngocdiep@gmail.com','Diep@123','Bùi Ngọc Diệp','/img/default-avatar.svg','Sinh viên Y, thích nấu ăn healthy.','Thanh Hóa','Đại học Y Hà Nội','','Y học,Nấu ăn,Chạy bộ,Đọc sách','USER','ACTIVE','','2026-11-06 21:00:00','2026-03-20 09:00:00','Nữ','2001-08-08','Hà Nội','tok_ngocdiep01'),
('ACC000019','hoangson','hoangson@gmail.com','Son@123','Mai Hoàng Sơn','/img/default-avatar.svg','Photographer phong cảnh Việt Nam.','Lào Cai','Đại học Khoa học Xã hội & Nhân văn','Freelance','Nhiếp ảnh,Du lịch,Trekking,Cà phê','USER','ACTIVE','','2026-11-08 12:00:00','2026-04-01 15:00:00','Nam','1996-06-16','Hà Nội','tok_hoangson01'),
('ACC000020','thuyduong','thuyduong@gmail.com','Duong@123','Đinh Thùy Dương','/img/default-avatar.svg','Product Manager tại startup fintech.','Hà Nội','Đại học Ngoại Thương','TiKi','Sản phẩm,Startup,Yoga,Podcast','USER','ACTIVE','','2026-11-08 09:55:00','2026-04-08 11:00:00','Nữ','1997-10-02','Hà Nội','tok_thuyduong01'),
('ACC000021','khaihoan','khaihoan@gmail.com','Hoan@123','Lý Khải Hoàn','/img/default-avatar.svg','DevOps & Cloud AWS.','Quảng Ninh','Đại học Bách Khoa Hà Nội','FPT Cloud','DevOps,Cloud,Linux,Bóng đá','USER','ACTIVE','','2026-11-08 08:30:00','2026-04-15 13:00:00','Nam','1998-02-28','Hà Nội','tok_khaihoan01'),
('ACC000022','baochau','baochau@gmail.com','Chau@123','Trịnh Bảo Châu','/img/default-avatar.svg','Luật sư tập sự, thích tranh luận.','TP. Hồ Chí Minh','Đại học Luật TP.HCM','VCI Legal','Pháp luật,Đọc sách,Cà phê,Phim','USER','ACTIVE','','2026-11-07 20:15:00','2026-04-22 10:00:00','Nữ','1999-07-19','TP. Hồ Chí Minh','tok_baochau01'),
('ACC000023','minhkhang','minhkhang@gmail.com','Khang@123','Hà Minh Khang','/img/default-avatar.svg','Game developer Unity.','Đà Nẵng','Đại học Duy Tân','VNG Games','Game,Lập trình,Anime,Esports','USER','ACTIVE','','2026-11-08 13:10:00','2026-05-01 09:00:00','Nam','2000-12-01','Đà Nẵng','tok_minhkhang01'),
('ACC000024','lanhuong','lanhuong@gmail.com','Huong@123','Cao Lan Hương','/img/default-avatar.svg','Biên tập viên báo điện tử.','Hà Nội','Đại học Khoa học Xã hội & Nhân văn','VnExpress','Báo chí,Viết lách,Du lịch,Cà phê','USER','ACTIVE','','2026-11-08 07:20:00','2026-05-10 14:00:00','Nữ','1998-03-27','Hà Nội','tok_lanhuong01'),
('ACC000025','trongtin','trongtin@gmail.com','Tin@123','Phan Trọng Tín','/img/default-avatar.svg','Kỹ sư xây dựng cầu đường.','Nghệ An','Đại học Giao thông Vận tải','Coteccons','Xây dựng,Bóng đá,Du lịch,Nấu ăn','USER','ACTIVE','','2026-11-05 19:00:00','2026-05-18 16:00:00','Nam','1997-11-11','Hà Nội','tok_trongtin01'),
('ACC000026','yennhi','yennhi@gmail.com','Nhi@123','Tạ Yến Nhi','/img/default-avatar.svg','Sinh viên năm 3 Marketing.','Hải Dương','Đại học Kinh tế Quốc dân','','Marketing,Thời trang,Makeup,Nhạc Kpop','USER','ACTIVE','','2026-11-08 10:40:00','2026-06-01 08:00:00','Nữ','2003-01-09','Hà Nội','tok_yennhi01'),
('ACC000027','ducanh','ducanh2@gmail.com','Anh@789','Võ Đức Anh','/img/default-avatar.svg','Security researcher, CTF player.','Hà Nội','Đại học Bách Khoa Hà Nội','Viettel Cyber Security','Bảo mật,CTF,Linux,Cờ vua','USER','ACTIVE','','2026-11-08 11:00:00','2026-06-08 10:00:00','Nam','1999-09-15','Hà Nội','tok_ducanh02'),
('ACC000028','hoangyen','hoangyen@gmail.com','Yen@123','Nguyễn Hoàng Yến','/img/default-avatar.svg','HR tại công ty công nghệ.','Bình Dương','Đại học Kinh tế - Luật','Shopee','Nhân sự,Yoga,Đọc sách,Du lịch','USER','ACTIVE','','2026-11-07 17:30:00','2026-06-15 12:00:00','Nữ','1998-05-21','TP. Hồ Chí Minh','tok_hoangyen01'),
('ACC000029','phucan','phucan@gmail.com','An@123','Đỗ Phúc An','/img/default-avatar.svg','Nhạc sĩ indie, guitar acoustic.','Huế','Học viện Âm nhạc Huế','Freelance','Âm nhạc,Guitar,Cà phê,Phim','USER','ACTIVE','','2026-11-08 14:00:00','2026-06-20 15:00:00','Nam','2000-08-03','Huế','tok_phucan01');

-- ---------------------------------------------------------------------------
-- 2. BẠN BÈ (hai chiều) — INSERT OR IGNORE để không trùng PK
-- ---------------------------------------------------------------------------
-- Kết nối user mới với user cũ + mạng lưới giữa user mới
INSERT OR IGNORE INTO friendships (user_id_1, user_id_2, status, created_at) VALUES
('ACC000001','ACC000010','FRIEND','2026-10-01 10:00:00'),('ACC000010','ACC000001','FRIEND','2026-10-01 10:00:00'),
('ACC000001','ACC000011','FRIEND','2026-10-02 10:00:00'),('ACC000011','ACC000001','FRIEND','2026-10-02 10:00:00'),
('ACC000001','ACC000015','FRIEND','2026-10-03 10:00:00'),('ACC000015','ACC000001','FRIEND','2026-10-03 10:00:00'),
('ACC000001','ACC000017','FRIEND','2026-10-04 10:00:00'),('ACC000017','ACC000001','FRIEND','2026-10-04 10:00:00'),
('ACC000001','ACC000021','FRIEND','2026-10-05 10:00:00'),('ACC000021','ACC000001','FRIEND','2026-10-05 10:00:00'),
('ACC000002','ACC000010','FRIEND','2026-10-01 11:00:00'),('ACC000010','ACC000002','FRIEND','2026-10-01 11:00:00'),
('ACC000002','ACC000012','FRIEND','2026-10-06 10:00:00'),('ACC000012','ACC000002','FRIEND','2026-10-06 10:00:00'),
('ACC000002','ACC000014','FRIEND','2026-10-07 10:00:00'),('ACC000014','ACC000002','FRIEND','2026-10-07 10:00:00'),
('ACC000002','ACC000016','FRIEND','2026-10-08 10:00:00'),('ACC000016','ACC000002','FRIEND','2026-10-08 10:00:00'),
('ACC000002','ACC000020','FRIEND','2026-10-09 10:00:00'),('ACC000020','ACC000002','FRIEND','2026-10-09 10:00:00'),
('ACC000003','ACC000011','FRIEND','2026-10-10 10:00:00'),('ACC000011','ACC000003','FRIEND','2026-10-10 10:00:00'),
('ACC000003','ACC000015','FRIEND','2026-10-11 10:00:00'),('ACC000015','ACC000003','FRIEND','2026-10-11 10:00:00'),
('ACC000003','ACC000023','FRIEND','2026-10-12 10:00:00'),('ACC000023','ACC000003','FRIEND','2026-10-12 10:00:00'),
('ACC000004','ACC000012','FRIEND','2026-10-13 10:00:00'),('ACC000012','ACC000004','FRIEND','2026-10-13 10:00:00'),
('ACC000004','ACC000014','FRIEND','2026-10-14 10:00:00'),('ACC000014','ACC000004','FRIEND','2026-10-14 10:00:00'),
('ACC000004','ACC000019','FRIEND','2026-10-15 10:00:00'),('ACC000019','ACC000004','FRIEND','2026-10-15 10:00:00'),
('ACC000005','ACC000011','FRIEND','2026-10-16 10:00:00'),('ACC000011','ACC000005','FRIEND','2026-10-16 10:00:00'),
('ACC000005','ACC000017','FRIEND','2026-10-17 10:00:00'),('ACC000017','ACC000005','FRIEND','2026-10-17 10:00:00'),
('ACC000005','ACC000027','FRIEND','2026-10-18 10:00:00'),('ACC000027','ACC000005','FRIEND','2026-10-18 10:00:00'),
('ACC000006','ACC000010','FRIEND','2026-10-19 10:00:00'),('ACC000010','ACC000006','FRIEND','2026-10-19 10:00:00'),
('ACC000006','ACC000016','FRIEND','2026-10-20 10:00:00'),('ACC000016','ACC000006','FRIEND','2026-10-20 10:00:00'),
('ACC000006','ACC000026','FRIEND','2026-10-21 10:00:00'),('ACC000026','ACC000006','FRIEND','2026-10-21 10:00:00'),
('ACC000007','ACC000013','FRIEND','2026-10-22 10:00:00'),('ACC000013','ACC000007','FRIEND','2026-10-22 10:00:00'),
('ACC000007','ACC000021','FRIEND','2026-10-23 10:00:00'),('ACC000021','ACC000007','FRIEND','2026-10-23 10:00:00'),
('ACC000008','ACC000012','FRIEND','2026-10-24 10:00:00'),('ACC000012','ACC000008','FRIEND','2026-10-24 10:00:00'),
('ACC000008','ACC000024','FRIEND','2026-10-25 10:00:00'),('ACC000024','ACC000008','FRIEND','2026-10-25 10:00:00'),
('ACC000009','ACC000015','FRIEND','2026-10-26 10:00:00'),('ACC000015','ACC000009','FRIEND','2026-10-26 10:00:00'),
('ACC000009','ACC000023','FRIEND','2026-10-27 10:00:00'),('ACC000023','ACC000009','FRIEND','2026-10-27 10:00:00'),
-- Mạng giữa user mới
('ACC000010','ACC000012','FRIEND','2026-10-28 10:00:00'),('ACC000012','ACC000010','FRIEND','2026-10-28 10:00:00'),
('ACC000010','ACC000016','FRIEND','2026-10-29 10:00:00'),('ACC000016','ACC000010','FRIEND','2026-10-29 10:00:00'),
('ACC000011','ACC000015','FRIEND','2026-10-30 10:00:00'),('ACC000015','ACC000011','FRIEND','2026-10-30 10:00:00'),
('ACC000011','ACC000021','FRIEND','2026-10-31 10:00:00'),('ACC000021','ACC000011','FRIEND','2026-10-31 10:00:00'),
('ACC000012','ACC000014','FRIEND','2026-11-01 10:00:00'),('ACC000014','ACC000012','FRIEND','2026-11-01 10:00:00'),
('ACC000013','ACC000019','FRIEND','2026-11-01 11:00:00'),('ACC000019','ACC000013','FRIEND','2026-11-01 11:00:00'),
('ACC000013','ACC000023','FRIEND','2026-11-01 12:00:00'),('ACC000023','ACC000013','FRIEND','2026-11-01 12:00:00'),
('ACC000014','ACC000020','FRIEND','2026-11-02 10:00:00'),('ACC000020','ACC000014','FRIEND','2026-11-02 10:00:00'),
('ACC000015','ACC000017','FRIEND','2026-11-02 11:00:00'),('ACC000017','ACC000015','FRIEND','2026-11-02 11:00:00'),
('ACC000015','ACC000027','FRIEND','2026-11-02 12:00:00'),('ACC000027','ACC000015','FRIEND','2026-11-02 12:00:00'),
('ACC000016','ACC000018','FRIEND','2026-11-03 10:00:00'),('ACC000018','ACC000016','FRIEND','2026-11-03 10:00:00'),
('ACC000017','ACC000021','FRIEND','2026-11-03 11:00:00'),('ACC000021','ACC000017','FRIEND','2026-11-03 11:00:00'),
('ACC000018','ACC000024','FRIEND','2026-11-03 12:00:00'),('ACC000024','ACC000018','FRIEND','2026-11-03 12:00:00'),
('ACC000019','ACC000024','FRIEND','2026-11-04 10:00:00'),('ACC000024','ACC000019','FRIEND','2026-11-04 10:00:00'),
('ACC000020','ACC000022','FRIEND','2026-11-04 11:00:00'),('ACC000022','ACC000020','FRIEND','2026-11-04 11:00:00'),
('ACC000020','ACC000026','FRIEND','2026-11-04 12:00:00'),('ACC000026','ACC000020','FRIEND','2026-11-04 12:00:00'),
('ACC000021','ACC000027','FRIEND','2026-11-05 10:00:00'),('ACC000027','ACC000021','FRIEND','2026-11-05 10:00:00'),
('ACC000022','ACC000028','FRIEND','2026-11-05 11:00:00'),('ACC000028','ACC000022','FRIEND','2026-11-05 11:00:00'),
('ACC000023','ACC000029','FRIEND','2026-11-05 12:00:00'),('ACC000029','ACC000023','FRIEND','2026-11-05 12:00:00'),
('ACC000024','ACC000026','FRIEND','2026-11-06 10:00:00'),('ACC000026','ACC000024','FRIEND','2026-11-06 10:00:00'),
('ACC000025','ACC000013','FRIEND','2026-11-06 11:00:00'),('ACC000013','ACC000025','FRIEND','2026-11-06 11:00:00'),
('ACC000026','ACC000028','FRIEND','2026-11-06 12:00:00'),('ACC000028','ACC000026','FRIEND','2026-11-06 12:00:00'),
('ACC000027','ACC000011','FRIEND','2026-11-07 10:00:00'),('ACC000011','ACC000027','FRIEND','2026-11-07 10:00:00'),
('ACC000028','ACC000014','FRIEND','2026-11-07 11:00:00'),('ACC000014','ACC000028','FRIEND','2026-11-07 11:00:00'),
('ACC000029','ACC000019','FRIEND','2026-11-07 12:00:00'),('ACC000019','ACC000029','FRIEND','2026-11-07 12:00:00');

-- Lời mời kết bạn đang chờ
INSERT OR IGNORE INTO friend_requests (requester_id, receiver_id, status, created_at) VALUES
('ACC000018','ACC000001','PENDING','2026-11-07 09:00:00'),
('ACC000019','ACC000001','PENDING','2026-11-07 10:00:00'),
('ACC000025','ACC000002','PENDING','2026-11-07 11:00:00'),
('ACC000029','ACC000002','PENDING','2026-11-07 12:00:00'),
('ACC000022','ACC000003','PENDING','2026-11-07 13:00:00'),
('ACC000026','ACC000001','PENDING','2026-11-08 08:00:00'),
('ACC000010','ACC000005','PENDING','2026-11-08 09:00:00'),
('ACC000024','ACC000003','PENDING','2026-11-08 09:30:00');

-- ---------------------------------------------------------------------------
-- 3. BÀI ĐĂNG (SEEDP...) — nhiều chủ đề, visibility, media
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO posts (id, author_id, content, media_type, media_url, shared_post_id, shared_author_name, shared_content, created_at, title, visibility, category, category_label, moderation_status, moderation_action, moderation_reason) VALUES
('SEEDP001','ACC000010','Buổi sáng Hà Nội se lạnh, cầm ly cà phê ngồi góc quán đọc xong chương cuối tiểu thuyết. Cảm giác thật bình yên.','IMAGE','https://images.unsplash.com/photo-1495474472287-4d71bcdd2085?auto=format&fit=crop&w=800&q=80','','','','2026-11-01 07:30:00','Cà phê & sách','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP002','ACC000011','Chia sẻ snippet C++17 dùng std::optional và structured bindings — code sạch hơn hẳn so với C++11. Ai đang migrate project không?','NONE','','','','','2026-11-01 14:20:00','C++17 tips','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP003','ACC000012','Moodboard tone pastel cho app fintech mình đang design. Feedback giúp mình với ạ!','IMAGE','https://images.unsplash.com/photo-1561070791-2526d30994b5?auto=format&fit=crop&w=800&q=80','','','','2026-11-02 09:15:00','UI moodboard','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP004','ACC000013','Chạy 10km dọc sông Hàn sáng nay, PB mới 52 phút. Ai ở Đà Nẵng cùng chạy nhóm không?','IMAGE','https://images.unsplash.com/photo-1476480862126-209bfaa8edc8?auto=format&fit=crop&w=800&q=80','','','','2026-11-02 06:45:00','Morning run','PUBLIC','the_thao','Thể thao','SAFE','ALLOW',''),
('SEEDP005','ACC000014','Series vlog 7 ngày sống chậm ở Đà Lạt sắp lên sóng. Spoiler: có một quán bánh mì siêu ngon gần Hồ Xuân Hương.','IMAGE','https://images.unsplash.com/photo-1528127269322-539801943592?auto=format&fit=crop&w=800&q=80','','','','2026-11-02 20:00:00','Vlog Đà Lạt','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP006','ACC000015','Paper mới về transformer efficiency — sparse attention giảm 40% compute mà accuracy gần như không đổi. Link trong comment.','NONE','','','','','2026-11-03 11:00:00','AI paper notes','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP007','ACC000016','Tips học từ vựng IELTS: spaced repetition + context sentence. 3 tháng tăng 1.5 band vocabulary.','NONE','','','','','2026-11-03 16:30:00','IELTS tips','PUBLIC','hoc_tap','Học tập','SAFE','ALLOW',''),
('SEEDP008','ACC000017','Deploy microservices bằng Docker Compose trên VPS 2GB RAM — checklist mình vừa viết xong.','NONE','','','','','2026-11-03 21:10:00','Docker notes','FRIENDS','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP009','ACC000018','Công thức salad quinoa + ức gà cho người bận. 15 phút là xong, đủ protein cho bữa trưa.','IMAGE','https://images.unsplash.com/photo-1512621776951-a57141f2eefd?auto=format&fit=crop&w=800&q=80','','','','2026-11-04 12:00:00','Healthy lunch','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP010','ACC000019','Hoàng hôn Fansipan hôm qua. Máy Fuji + 23mm f/1.4, ISO 200. Không chỉnh màu nhiều.','IMAGE','https://images.unsplash.com/photo-1506905925346-21bda4d32df4?auto=format&fit=crop&w=800&q=80','','','','2026-11-04 18:40:00','Fansipan sunset','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP011','ACC000020','Roadmap product Q1: ưu tiên onboarding flow, giảm 3 bước đăng ký. Ai làm PM cũng từng đau với retention đúng không?','NONE','','','','','2026-11-05 09:00:00','PM thoughts','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP012','ACC000021','Terraform module cho VPC AWS mình open-source hôm nay. PR welcome!','NONE','','','','','2026-11-05 15:20:00','Terraform VPC','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP013','ACC000022','Góc nhìn pháp lý về hợp đồng lao động thử việc — nhiều bạn trẻ ký mà không đọc kỹ điều khoản đơn phương chấm dứt.','NONE','','','','','2026-11-05 19:00:00','Pháp lý việc làm','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP014','ACC000023','Demo gameplay pixel art 2D mình làm cuối tuần. Engine Unity 2022 LTS.','IMAGE','https://images.unsplash.com/photo-1550745165-9bc0b252726f?auto=format&fit=crop&w=800&q=80','','','','2026-11-06 10:30:00','Indie game demo','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP015','ACC000024','Phóng sự ngắn về chợ sách cũ Hà Nội — những người giữ nghề giữa thời số hóa.','IMAGE','https://images.unsplash.com/photo-1481627834876-b7833e8f5570?auto=format&fit=crop&w=800&q=80','','','','2026-11-06 14:00:00','Chợ sách cũ','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP016','ACC000025','Cầu Nhật Tân lúc 5h sáng, sương mù mỏng. Công trình mình từng tham gia giám sát hạng mục phụ.','IMAGE','https://images.unsplash.com/photo-1477959858617-67f85cf4f1df?auto=format&fit=crop&w=800&q=80','','','','2026-11-06 05:30:00','Cầu Nhật Tân','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP017','ACC000026','Outfit đi học tuần này: áo len + chân váy midi. Shop nào bán len dày giá sinh viên không ạ?','IMAGE','https://images.unsplash.com/photo-1483985988355-763728e1935b?auto=format&fit=crop&w=800&q=80','','','','2026-11-06 17:00:00','OOTD','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP018','ACC000027','Write-up challenge pwn cuối tuần — heap overflow + glibc 2.35. Đừng spoil nếu chưa solve nhé.','NONE','','','','','2026-11-07 01:20:00','CTF write-up','FRIENDS','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP019','ACC000028','3 câu hỏi phỏng vấn HR hay dùng để đánh giá culture fit — và cách ứng viên nên trả lời thật.','NONE','','','','','2026-11-07 10:00:00','HR tips','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP020','ACC000029','Demo cover bài mới trên guitar nylon. Mic SM58 + interface Focusrite. Góp ý giúp mình với!','IMAGE','https://images.unsplash.com/photo-1510915361894-db8b60106cb1?auto=format&fit=crop&w=800&q=80','','','','2026-11-07 21:30:00','Guitar cover','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP021','ACC000010','Cuối tuần chỉ muốn ở nhà nấu phở gà và xem phim cũ. Ai có list phim Việt đáng xem không?','NONE','','','','','2026-11-08 08:00:00','Cuối tuần','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP022','ACC000011','Benchmark SQLite vs PostgreSQL cho workload đọc nhiều trên VPS nhỏ — SQLite vẫn rất đáng cân nhắc.','NONE','','','','','2026-11-08 09:30:00','DB benchmark','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP023','ACC000015','Workshop intro LLM tại trường CNTT tuần sau. Slot còn 15 chỗ.','NONE','','','','','2026-11-08 10:15:00','Workshop LLM','PUBLIC','hoc_tap','Học tập','SAFE','ALLOW',''),
('SEEDP024','ACC000001','Team vừa ship xong tính năng ranking bảng tin. Feedback giúp mình nhé!','NONE','','','','','2026-11-08 11:00:00','Ship ranking','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP025','ACC000002','Hồ Gươm đêm mưa — ánh đèn vàng phản trên mặt nước. Ảnh chụp bằng điện thoại thôi.','IMAGE','https://images.unsplash.com/photo-1559592413-7eec8e0f4c1a?auto=format&fit=crop&w=800&q=80','','','','2026-11-07 22:40:00','Hồ Gươm mưa','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP026','ACC000012','Private journal: hôm nay client feedback hơi nặng nhưng học được nhiều.','NONE','','','','','2026-11-06 23:00:00','Journal','PRIVATE','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP027','ACC000014','Clip hậu trường quay vlog 15 giây — camera rung vì cười quá.','VIDEO','https://www.w3schools.com/html/mov_bbb.mp4','','','','2026-11-05 16:00:00','BTS vlog','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP028','ACC000003','Graph BFS cho gợi ý bạn bè — độ sâu 2 cho kết quả khá ổn trên dataset nhỏ.','NONE','','','','','2026-11-04 13:00:00','BFS friends','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP029','ACC000006','Bánh mì thịt nướng tự làm cuối tuần. Công thức nước sốt trong comment.','IMAGE','https://images.unsplash.com/photo-1509440159596-0249088772ff?auto=format&fit=crop&w=800&q=80','','','','2026-11-03 18:00:00','Bánh mì homemade','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP030','ACC000020','Repost vì bài này quá hợp với team mình đang làm.','NONE','','SEEDP011','Đinh Thùy Dương','Roadmap product Q1: ưu tiên onboarding flow, giảm 3 bước đăng ký. Ai làm PM cũng từng đau với retention đúng không?','2026-11-07 08:00:00','','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP031','ACC000017','Ai biết chỗ thuê co-working giá sinh viên quanh Cầu Giấy không?','NONE','','','','','2026-11-08 07:00:00','Hỏi co-working','PUBLIC','doi_song','Đời sống','SAFE','ALLOW',''),
('SEEDP032','ACC000023','Patch 0.3.1: fix collision, thêm 2 enemy types. Playtest tối nay.','NONE','','','','','2026-11-08 12:30:00','Game patch','FRIENDS','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP033','ACC000016','Flashcard Anki deck IELTS Academic Writing mình share miễn phí. Inbox nếu cần.','NONE','','','','','2026-11-02 11:00:00','Anki IELTS','PUBLIC','hoc_tap','Học tập','SAFE','ALLOW',''),
('SEEDP034','ACC000019','Bộ ảnh mùa lúa chín Yên Bái. Lightroom preset mình tự chỉnh.','IMAGE','https://images.unsplash.com/photo-1500382017468-9049fed747ef?auto=format&fit=crop&w=800&q=80','','','','2026-10-28 17:00:00','Mùa lúa Yên Bái','PUBLIC','giai_tri','Giải trí','SAFE','ALLOW',''),
('SEEDP035','ACC000021','Incident postmortem: disk full trên node logging — lesson learned về retention policy.','NONE','','','','','2026-11-01 22:00:00','Postmortem','FRIENDS','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP036','ACC000004','Workshop sketch UI trên giấy trước khi nhảy vào Figma — vẫn là thói quen tốt.','IMAGE','https://images.unsplash.com/photo-1586717791821-3f44a563fa4c?auto=format&fit=crop&w=800&q=80','','','','2026-10-30 14:00:00','Paper sketch','PUBLIC','cong_nghe','Công nghệ','SAFE','ALLOW',''),
('SEEDP037','ACC000008','Review sách Atomic Habits bản tiếng Việt — 3 ý áp dụng được ngay.','NONE','','','','','2026-10-25 20:00:00','Book review','PUBLIC','hoc_tap','Học tập','SAFE','ALLOW',''),
('SEEDP038','ACC000013','Giải half-marathon Đà Nẵng 2026 mình đăng ký xong rồi. Ai cùng team không?','NONE','','','','','2026-11-08 06:00:00','Half marathon','PUBLIC','the_thao','Thể thao','SAFE','ALLOW',''),
('SEEDP039','ACC000027','Lab môi trường Kali + Windows victim trên VirtualBox cho lớp security.','NONE','','','','','2026-10-20 09:00:00','Security lab','PUBLIC','hoc_tap','Học tập','SAFE','ALLOW',''),
('SEEDP040','ACC000001','Cảm ơn mọi người đã test tính năng tin nhắn và tìm kiếm. Bug report gửi admin giúp mình nhé.','NONE','','','','','2026-11-08 13:00:00','Thanks testers','PUBLIC','doi_song','Đời sống','SAFE','ALLOW','');

-- ---------------------------------------------------------------------------
-- 4. CẢM XÚC (reactions)
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO reactions (post_id, user_id, type, created_at) VALUES
('SEEDP001','ACC000001','LOVE','2026-11-01 08:00:00'),('SEEDP001','ACC000002','LIKE','2026-11-01 08:10:00'),('SEEDP001','ACC000012','LOVE','2026-11-01 09:00:00'),
('SEEDP002','ACC000001','WOW','2026-11-01 15:00:00'),('SEEDP002','ACC000003','LIKE','2026-11-01 15:20:00'),('SEEDP002','ACC000015','LIKE','2026-11-01 16:00:00'),('SEEDP002','ACC000017','HAHA','2026-11-01 16:30:00'),
('SEEDP003','ACC000002','LOVE','2026-11-02 10:00:00'),('SEEDP003','ACC000004','LIKE','2026-11-02 10:30:00'),('SEEDP003','ACC000014','WOW','2026-11-02 11:00:00'),
('SEEDP004','ACC000007','LIKE','2026-11-02 07:00:00'),('SEEDP004','ACC000001','LIKE','2026-11-02 07:30:00'),('SEEDP004','ACC000019','LOVE','2026-11-02 08:00:00'),
('SEEDP005','ACC000002','LOVE','2026-11-02 21:00:00'),('SEEDP005','ACC000006','LIKE','2026-11-02 21:20:00'),('SEEDP005','ACC000012','LOVE','2026-11-02 22:00:00'),('SEEDP005','ACC000026','LIKE','2026-11-03 08:00:00'),
('SEEDP006','ACC000001','WOW','2026-11-03 12:00:00'),('SEEDP006','ACC000011','LIKE','2026-11-03 12:30:00'),('SEEDP006','ACC000015','LOVE','2026-11-03 13:00:00'),('SEEDP006','ACC000027','LIKE','2026-11-03 14:00:00'),
('SEEDP007','ACC000002','LIKE','2026-11-03 17:00:00'),('SEEDP007','ACC000010','LOVE','2026-11-03 17:30:00'),('SEEDP007','ACC000018','LIKE','2026-11-03 18:00:00'),
('SEEDP010','ACC000004','WOW','2026-11-04 19:00:00'),('SEEDP010','ACC000012','LOVE','2026-11-04 19:30:00'),('SEEDP010','ACC000019','LIKE','2026-11-04 20:00:00'),('SEEDP010','ACC000001','LOVE','2026-11-04 20:30:00'),('SEEDP010','ACC000002','WOW','2026-11-04 21:00:00'),
('SEEDP014','ACC000003','LIKE','2026-11-06 11:00:00'),('SEEDP014','ACC000009','HAHA','2026-11-06 11:30:00'),('SEEDP014','ACC000023','LOVE','2026-11-06 12:00:00'),
('SEEDP020','ACC000029','LOVE','2026-11-07 22:00:00'),('SEEDP020','ACC000012','LIKE','2026-11-07 22:30:00'),('SEEDP020','ACC000002','LOVE','2026-11-07 23:00:00'),
('SEEDP024','ACC000002','LIKE','2026-11-08 11:10:00'),('SEEDP024','ACC000003','WOW','2026-11-08 11:20:00'),('SEEDP024','ACC000011','LIKE','2026-11-08 11:40:00'),('SEEDP024','ACC000015','LOVE','2026-11-08 12:00:00'),('SEEDP024','ACC000017','LIKE','2026-11-08 12:20:00'),
('SEEDP025','ACC000001','LOVE','2026-11-07 23:00:00'),('SEEDP025','ACC000004','LIKE','2026-11-07 23:10:00'),('SEEDP025','ACC000010','LOVE','2026-11-08 07:00:00'),
('SEEDP028','ACC000001','LIKE','2026-11-04 14:00:00'),('SEEDP028','ACC000005','WOW','2026-11-04 14:30:00'),('SEEDP028','ACC000011','LIKE','2026-11-04 15:00:00'),
('SEEDP038','ACC000007','LIKE','2026-11-08 06:30:00'),('SEEDP038','ACC000013','LOVE','2026-11-08 07:00:00'),('SEEDP038','ACC000001','LIKE','2026-11-08 07:15:00'),
('SEEDP040','ACC000002','LOVE','2026-11-08 13:10:00'),('SEEDP040','ACC000010','LIKE','2026-11-08 13:20:00'),('SEEDP040','ACC000011','LIKE','2026-11-08 13:30:00'),
('POST000001','ACC000010','LIKE','2026-11-05 10:00:00'),('POST000001','ACC000011','LOVE','2026-11-05 11:00:00'),('POST000001','ACC000015','WOW','2026-11-05 12:00:00'),
('POST000003','ACC000011','LIKE','2026-11-05 13:00:00'),('POST000003','ACC000015','LOVE','2026-11-05 14:00:00');

-- ---------------------------------------------------------------------------
-- 5. BÌNH LUẬN
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO comments (id, post_id, author_id, content, reply_to_id, reply_to_author, created_at) VALUES
('SEEDC001','SEEDP001','ACC000001','Quán nào thế Thư? Cho xin địa chỉ với!','','','2026-11-01 08:20:00'),
('SEEDC002','SEEDP001','ACC000010','Góc phố Nguyễn Hữu Huân nha Khoa ơi.','SEEDC001','Nguyễn Minh Khoa','2026-11-01 08:40:00'),
('SEEDC003','SEEDP002','ACC000001','Optional đúng là game changer. Mình đang refactor dần.','','','2026-11-01 15:10:00'),
('SEEDC004','SEEDP002','ACC000015','Có so sánh với expected từ C++23 không bro?','','','2026-11-01 15:40:00'),
('SEEDC005','SEEDP003','ACC000002','Tone pastel nhìn dễ thương quá Linh ơi!','','','2026-11-02 10:20:00'),
('SEEDC006','SEEDP004','ACC000007','52 phút 10km đỉnh thật. Cuối tuần chạy chung nhé!','','','2026-11-02 07:20:00'),
('SEEDC007','SEEDP005','ACC000006','Tag mình khi lên sóng nhaaa','','','2026-11-02 21:30:00'),
('SEEDC008','SEEDP006','ACC000011','Gửi link paper đi Bảo!','','','2026-11-03 12:15:00'),
('SEEDC009','SEEDP006','ACC000015','https://arxiv.org/abs/example — demo link.','SEEDC008','Trần Đức Mạnh','2026-11-03 12:45:00'),
('SEEDC010','SEEDP007','ACC000010','Anki + context sentence là combo mình cũng dùng.','','','2026-11-03 17:20:00'),
('SEEDC011','SEEDP010','ACC000002','Ảnh đẹp xuất thần luôn ơi!','','','2026-11-04 19:20:00'),
('SEEDC012','SEEDP010','ACC000012','Composition quá chất Sơn ơi.','','','2026-11-04 19:50:00'),
('SEEDC013','SEEDP014','ACC000009','Pixel art nhìn retro dễ thương. Khi nào public demo?','','','2026-11-06 11:20:00'),
('SEEDC014','SEEDP014','ACC000023','Cuối tháng này nha Huy!','SEEDC013','Bùi Quang Huy','2026-11-06 11:50:00'),
('SEEDC015','SEEDP020','ACC000002','Guitar hay quá, cover bài gì thế?','','','2026-11-07 22:20:00'),
('SEEDC016','SEEDP024','ACC000002','Ranking mượt, bài mình quan tâm hiện đúng hơn.','','','2026-11-08 11:30:00'),
('SEEDC017','SEEDP024','ACC000011','Performance ổn không Khoa?','','','2026-11-08 11:45:00'),
('SEEDC018','SEEDP024','ACC000001','Trên 120 ứng viên rank dưới 50ms nha.','SEEDC017','Trần Đức Mạnh','2026-11-08 12:00:00'),
('SEEDC019','SEEDP025','ACC000001','Ảnh thơ quá Lan ơi.','','','2026-11-07 23:20:00'),
('SEEDC020','SEEDP028','ACC000011','BFS depth 2 mình cũng thử, precision khá tốt.','','','2026-11-04 14:20:00'),
('SEEDC021','SEEDP038','ACC000001','Đăng ký chung team với Long được không?','','','2026-11-08 06:45:00'),
('SEEDC022','SEEDP040','ACC000010','Đã test search history + jump message, ổn!','','','2026-11-08 13:15:00'),
('SEEDC023','POST000001','ACC000010','Giao diện dark mode nhìn professional thật.','','','2026-11-05 10:30:00'),
('SEEDC024','POST000003','ACC000015','BFS + ranking kết hợp sẽ rất mạnh.','','','2026-11-05 14:30:00');

-- ---------------------------------------------------------------------------
-- 6. TIN NHẮN 1-1 (conversation_id = min_id_max_id)
-- Có tin đã đọc / chưa đọc, reply, media
-- Xóa tin seed cũ (theo conversation seed) rồi insert lại → chạy nhiều lần không trùng
-- ---------------------------------------------------------------------------
-- Xóa toàn bộ tin trong các hội thoại seed (chỉ các cặp mới tạo trong seeder)
DELETE FROM messages WHERE conversation_id IN (
  'ACC000001_ACC000010','ACC000001_ACC000011','ACC000001_ACC000015',
  'ACC000002_ACC000012','ACC000002_ACC000014','ACC000003_ACC000023',
  'ACC000004_ACC000019','ACC000005_ACC000027','ACC000011_ACC000015',
  'ACC000020_ACC000022'
);
-- Tin seed thêm vào hội thoại Khoa-Lan
DELETE FROM messages WHERE conversation_id='ACC000001_ACC000002'
  AND content IN ('Khoa ơi tối họp sprint không?','Mình có cập nhật design ranking UI.');

-- Khoa (001) ↔ Thư (010) — có unread cho Khoa
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000001_ACC000010','ACC000001','ACC000010','Chào Thư, cuối tuần đi cafe Nguyễn Hữu Huân không?',0,1,'2026-11-06 09:00:00','','NONE','','','',0),
('ACC000001_ACC000010','ACC000010','ACC000001','Được chứ! 9h sáng nhé.',0,1,'2026-11-06 09:05:00','','NONE','','LOVE','Cuối tuần đi cafe Nguyễn Hữu Huân không?',0),
('ACC000001_ACC000010','ACC000001','ACC000010','Ok, mình book bàn trước.',0,1,'2026-11-06 09:08:00','','NONE','','','',0),
('ACC000001_ACC000010','ACC000010','ACC000001','Mình vừa đăng story quán đó, xem thử nhé.',0,0,'2026-11-08 08:20:00','','NONE','','','',0),
('ACC000001_ACC000010','ACC000010','ACC000001','À còn mang theo cuốn sách đang đọc của Khoa luôn.',0,0,'2026-11-08 08:22:00','','NONE','','','',0);

-- Khoa ↔ Mạnh (011)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000001_ACC000011','ACC000011','ACC000001','Khoa ơi, PR ranking review giúp mình với.',0,1,'2026-11-07 14:00:00','','NONE','','','',0),
('ACC000001_ACC000011','ACC000001','ACC000011','Ok, tối nay mình xem.',0,1,'2026-11-07 14:10:00','','NONE','','','',0),
('ACC000001_ACC000011','ACC000011','ACC000001','Cảm ơn! Mình push thêm test case.',0,1,'2026-11-07 14:15:00','','NONE','','','',0),
('ACC000001_ACC000011','ACC000011','ACC000001','Screenshot lỗi SQLite lock lúc peak:',0,0,'2026-11-08 10:00:00','','IMAGE','https://images.unsplash.com/photo-1555949963-aa79dcee981c?auto=format&fit=crop&w=600&q=80','','',0),
('ACC000001_ACC000011','ACC000011','ACC000001','Reproduce được trên máy mình.',0,0,'2026-11-08 10:02:00','','NONE','','','',0);

-- Khoa ↔ Bảo (015)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000001_ACC000015','ACC000001','ACC000015','Workshop LLM tuần sau mình đăng ký được không?',0,1,'2026-11-08 10:20:00','','NONE','','','',0),
('ACC000001_ACC000015','ACC000015','ACC000001','Còn slot, mình giữ chỗ cho Khoa.',0,1,'2026-11-08 10:25:00','','NONE','','LIKE','Workshop LLM tuần sau mình đăng ký được không?',0),
('ACC000001_ACC000015','ACC000001','ACC000015','Thanks Bảo!',0,1,'2026-11-08 10:26:00','','NONE','','','',0);

-- Lan (002) ↔ Linh (012)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000002_ACC000012','ACC000002','ACC000012','Moodboard fintech đẹp quá, cho mình tham khảo spacing được không?',0,1,'2026-11-02 11:00:00','','NONE','','','',0),
('ACC000002_ACC000012','ACC000012','ACC000002','Ok, mình gửi file Figma.',0,1,'2026-11-02 11:10:00','','NONE','','','',0),
('ACC000002_ACC000012','ACC000012','ACC000002','https://figma.com/file/demo — link tạm.',0,1,'2026-11-02 11:12:00','','NONE','','','',0),
('ACC000002_ACC000012','ACC000002','ACC000012','Cảm ơn Linh nhiều!',0,1,'2026-11-02 11:20:00','','NONE','','','',0),
('ACC000002_ACC000012','ACC000012','ACC000002','Lan ơi họp design system lúc 4h nhé.',0,0,'2026-11-08 09:00:00','','NONE','','','',0);

-- Lan ↔ Kim Anh (014)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000002_ACC000014','ACC000014','ACC000002','Lan review script vlog Đà Lạt giúp mình với.',0,1,'2026-11-03 08:00:00','','NONE','','','',0),
('ACC000002_ACC000014','ACC000002','ACC000014','Ok, đoạn intro nên ngắn hơn 10s.',0,1,'2026-11-03 09:00:00','','NONE','','','',0),
('ACC000002_ACC000014','ACC000014','ACC000002','Sửa xong rồi, xem lại giúp nhé.',0,0,'2026-11-08 07:30:00','','NONE','','','',0);

-- Tuấn Anh (003) ↔ Khang (023)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000003_ACC000023','ACC000003','ACC000023','Game pixel art nhìn ổn đấy Khang.',0,1,'2026-11-06 12:00:00','','NONE','','','',0),
('ACC000003_ACC000023','ACC000023','ACC000003','Thanks! Cậu playtest giúp được không?',0,1,'2026-11-06 12:10:00','','NONE','','','',0),
('ACC000003_ACC000023','ACC000003','ACC000023','Tối nay được.',0,1,'2026-11-06 12:15:00','','NONE','','','',0),
('ACC000003_ACC000023','ACC000023','ACC000003','Build mới đã lên, link trong drive.',0,0,'2026-11-08 12:40:00','','NONE','','','',0);

-- Ngọc Anh (004) ↔ Sơn (019)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000004_ACC000019','ACC000004','ACC000019','Bộ Fansipan đẹp quá, xin preset Lightroom được không?',0,1,'2026-11-04 20:00:00','','NONE','','','',0),
('ACC000004_ACC000019','ACC000019','ACC000004','Gửi qua mail nhé.',0,1,'2026-11-04 20:15:00','','NONE','','LOVE','xin preset Lightroom được không?',0),
('ACC000004_ACC000019','ACC000019','ACC000004','Ảnh raw Fansipan (sample):',0,1,'2026-11-04 20:20:00','','IMAGE','https://images.unsplash.com/photo-1506905925346-21bda4d32df4?auto=format&fit=crop&w=600&q=80','','',0);

-- Hiếu (005) ↔ Đức Anh (027)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000005_ACC000027','ACC000027','ACC000005','Challenge pwn tuần này khó hơn mọi khi.',0,1,'2026-11-07 02:00:00','','NONE','','','',0),
('ACC000005_ACC000027','ACC000005','ACC000027','Hint heap?',0,1,'2026-11-07 08:00:00','','NONE','','','',0),
('ACC000005_ACC000027','ACC000027','ACC000005','Chỉ vậy thôi, tự train đi :)',0,1,'2026-11-07 08:30:00','','NONE','','HAHA','Hint heap?',0);

-- Mạnh ↔ Bảo
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000011_ACC000015','ACC000011','ACC000015','Model sparse attention implement bằng PyTorch thế nào?',0,1,'2026-11-03 13:00:00','','NONE','','','',0),
('ACC000011_ACC000015','ACC000015','ACC000011','Mình có notebook demo, share drive.',0,1,'2026-11-03 13:20:00','','NONE','','','',0),
('ACC000011_ACC000015','ACC000011','ACC000015','Nhận được rồi, thanks!',0,1,'2026-11-03 14:00:00','','NONE','','','',0),
('ACC000011_ACC000015','ACC000015','ACC000011','Mai pair programming 30 phút nhé.',0,0,'2026-11-08 11:45:00','','NONE','','','',0);

-- Dương (020) ↔ Châu (022)
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000020_ACC000022','ACC000020','ACC000022','Châu ơi, review terms of service app giúp mình được không?',0,1,'2026-11-05 10:00:00','','NONE','','','',0),
('ACC000020_ACC000022','ACC000022','ACC000020','Gửi file đi, chiều mình xem.',0,1,'2026-11-05 10:20:00','','NONE','','','',0),
('ACC000020_ACC000022','ACC000022','ACC000020','Điều 7 cần sửa — quyền thu thập dữ liệu quá rộng.',0,1,'2026-11-05 16:00:00','','NONE','','','',0),
('ACC000020_ACC000022','ACC000020','ACC000022','Noted, thanks Châu!',0,1,'2026-11-05 16:10:00','','NONE','','','',0);

-- Khoa ↔ Lan (đã có lịch sử) — thêm tin mới unread
INSERT INTO messages (conversation_id, sender_id, recipient_id, content, is_revoked, is_read, created_at, shared_post_id, media_type, media_url, reaction, reply_to_text, reply_to_id) VALUES
('ACC000001_ACC000002','ACC000002','ACC000001','Khoa ơi tối họp sprint không?',0,0,'2026-11-08 13:40:00','','NONE','','','',0),
('ACC000001_ACC000002','ACC000002','ACC000001','Mình có cập nhật design ranking UI.',0,0,'2026-11-08 13:41:00','','NONE','','','',0);

-- ---------------------------------------------------------------------------
-- 7. NHÓM CHAT MỚI + THÀNH VIÊN + TIN NHẮN NHÓM
-- ---------------------------------------------------------------------------
DELETE FROM group_messages WHERE group_id IN ('GRPSEED01','GRPSEED02','GRPSEED03','GRPSEED04');

INSERT OR IGNORE INTO groups (id, name, description, avatar, created_by, created_at) VALUES
('GRPSEED01','C++ Backend Devs','Nhóm trao đổi C++, SQLite, hiệu năng server','/img/default-avatar.svg','ACC000001','2026-10-15 10:00:00'),
('GRPSEED02','Đà Nẵng Runners','Chạy bộ cuối tuần tại Đà Nẵng','/img/default-avatar.svg','ACC000013','2026-10-20 08:00:00'),
('GRPSEED03','Design & Product','UI/UX, PM, research','/img/default-avatar.svg','ACC000012','2026-10-25 14:00:00'),
('GRPSEED04','IELTS Study Squad','Luyện IELTS cùng nhau','/img/default-avatar.svg','ACC000016','2026-11-01 09:00:00');

INSERT OR IGNORE INTO group_members (group_id, user_id, role, joined_at) VALUES
('GRPSEED01','ACC000001','ADMIN','2026-10-15 10:00:00'),
('GRPSEED01','ACC000011','MEMBER','2026-10-15 10:05:00'),
('GRPSEED01','ACC000015','MEMBER','2026-10-15 10:10:00'),
('GRPSEED01','ACC000017','MEMBER','2026-10-15 10:15:00'),
('GRPSEED01','ACC000021','MEMBER','2026-10-16 09:00:00'),
('GRPSEED01','ACC000003','MEMBER','2026-10-16 10:00:00'),
('GRPSEED01','ACC000027','MEMBER','2026-10-17 11:00:00'),
('GRPSEED02','ACC000013','ADMIN','2026-10-20 08:00:00'),
('GRPSEED02','ACC000007','MEMBER','2026-10-20 08:10:00'),
('GRPSEED02','ACC000019','MEMBER','2026-10-20 09:00:00'),
('GRPSEED02','ACC000023','MEMBER','2026-10-21 07:00:00'),
('GRPSEED02','ACC000001','MEMBER','2026-10-22 06:00:00'),
('GRPSEED03','ACC000012','ADMIN','2026-10-25 14:00:00'),
('GRPSEED03','ACC000002','MEMBER','2026-10-25 14:10:00'),
('GRPSEED03','ACC000004','MEMBER','2026-10-25 14:20:00'),
('GRPSEED03','ACC000014','MEMBER','2026-10-25 15:00:00'),
('GRPSEED03','ACC000020','MEMBER','2026-10-26 10:00:00'),
('GRPSEED03','ACC000010','MEMBER','2026-10-27 11:00:00'),
('GRPSEED04','ACC000016','ADMIN','2026-11-01 09:00:00'),
('GRPSEED04','ACC000010','MEMBER','2026-11-01 09:10:00'),
('GRPSEED04','ACC000018','MEMBER','2026-11-01 09:20:00'),
('GRPSEED04','ACC000026','MEMBER','2026-11-02 08:00:00'),
('GRPSEED04','ACC000002','MEMBER','2026-11-02 09:00:00');

INSERT INTO group_messages (group_id, sender_id, content, shared_post_id, created_at, media_type, media_url, is_revoked, reaction, reply_to_text, reply_to_id) VALUES
('GRPSEED01','ACC000001','Chào cả nhà! Nhóm trao đổi C++ backend chính thức mở.','','2026-10-15 10:30:00','NONE','',0,'','',0),
('GRPSEED01','ACC000011','Em có bài về optional/variant, post lên bảng tin rồi ạ.','','2026-10-15 11:00:00','NONE','',0,'LIKE','',0),
('GRPSEED01','ACC000015','Ai optimize SQLite WAL mode chưa? Peak write đang bottleneck.','','2026-10-16 09:30:00','NONE','',0,'','',0),
('GRPSEED01','ACC000021','Mình recommend mmap + busy_timeout 5000.','','2026-10-16 10:00:00','NONE','',0,'','Ai optimize SQLite WAL mode chưa?',0),
('GRPSEED01','ACC000003','BFS gợi ý bạn bè chạy ổn trên 10k nodes.','','2026-10-17 14:00:00','NONE','',0,'WOW','',0),
('GRPSEED01','ACC000017','PR ranking feed merged chưa mọi người?','','2026-11-08 11:30:00','NONE','',0,'','',0),
('GRPSEED01','ACC000001','Đã merge sáng nay, mọi người pull về test nhé.','','2026-11-08 11:35:00','NONE','',0,'LOVE','PR ranking feed merged chưa mọi người?',0),
('GRPSEED02','ACC000013','Sáng CN họp mặt cầu Rồng 5h30 nhé.','','2026-11-01 20:00:00','NONE','',0,'','',0),
('GRPSEED02','ACC000007','Ok anh, em tới.','','2026-11-01 20:10:00','NONE','',0,'','',0),
('GRPSEED02','ACC000019','Mình mang camera theo luôn.','','2026-11-01 20:15:00','NONE','',0,'LIKE','',0),
('GRPSEED02','ACC000013','Ảnh buổi chạy tuần trước:','','2026-11-02 18:00:00','IMAGE','https://images.unsplash.com/photo-1476480862126-209bfaa8edc8?auto=format&fit=crop&w=600&q=80',0,'','',0),
('GRPSEED02','ACC000001','Tuần sau mình cũng join được.','','2026-11-07 21:00:00','NONE','',0,'','',0),
('GRPSEED03','ACC000012','Mọi người review moodboard fintech giúp mình với.','','2026-11-02 09:30:00','NONE','',0,'','',0),
('GRPSEED03','ACC000002','Spacing giữa card hơi chật, +4px sẽ thoáng hơn.','','2026-11-02 10:00:00','NONE','',0,'','',0),
('GRPSEED03','ACC000020','Từ góc PM: CTA primary cần nổi hơn trên mobile.','','2026-11-02 10:30:00','NONE','',0,'LIKE','',0),
('GRPSEED03','ACC000004','Mình sketch thêm 1 biến thể, gửi sau.','','2026-11-02 11:00:00','NONE','',0,'','',0),
('GRPSEED03','ACC000014','Có thể làm A/B test 2 phiên bản onboarding.','','2026-11-05 15:00:00','NONE','',0,'','',0),
('GRPSEED04','ACC000016','Tuần này focus Writing Task 2 — topic environment.','','2026-11-03 09:00:00','NONE','',0,'','',0),
('GRPSEED04','ACC000010','Mình submit essay tối nay ạ.','','2026-11-03 09:20:00','NONE','',0,'','',0),
('GRPSEED04','ACC000018','Ai có template paraphrasing không?','','2026-11-03 10:00:00','NONE','',0,'','',0),
('GRPSEED04','ACC000016','Mình gửi deck Anki trong bài đăng SEEDP033.','','2026-11-03 10:15:00','NONE','',0,'LOVE','',0),
('GRPSEED04','ACC000002','Cảm ơn Trang, deck rất hữu ích!','','2026-11-04 08:00:00','NONE','',0,'','',0);

-- ---------------------------------------------------------------------------
-- 8. THÔNG BÁO MẪU
-- ---------------------------------------------------------------------------
DELETE FROM notifications WHERE link LIKE '%SEEDP%' OR (created_at >= '2026-10-01' AND content LIKE '%lời mời kết bạn%' AND actor_id IN ('ACC000018','ACC000019','ACC000010'));
DELETE FROM notifications WHERE created_at IN ('2026-11-08 11:45:00','2026-11-08 12:00:00','2026-11-07 09:00:00','2026-11-07 10:00:00','2026-11-08 07:30:00','2026-11-08 11:10:00');

INSERT INTO notifications (user_id, actor_id, type, content, link, is_read, created_at) VALUES
('ACC000001','ACC000010','FRIEND_ACCEPT','đã chấp nhận lời mời kết bạn.','/friends.html',1,'2026-10-01 10:05:00'),
('ACC000001','ACC000011','COMMENT','đã bình luận bài viết của bạn.','/index.html#post-SEEDP024',0,'2026-11-08 11:45:00'),
('ACC000001','ACC000015','REACTION','đã thả cảm xúc bài viết của bạn.','/index.html#post-SEEDP024',0,'2026-11-08 12:00:00'),
('ACC000001','ACC000018','FRIEND_REQUEST','đã gửi lời mời kết bạn.','/friends.html',0,'2026-11-07 09:00:00'),
('ACC000001','ACC000019','FRIEND_REQUEST','đã gửi lời mời kết bạn.','/friends.html',0,'2026-11-07 10:00:00'),
('ACC000002','ACC000012','COMMENT','đã bình luận bài viết của bạn.','/index.html#post-SEEDP025',1,'2026-11-07 23:20:00'),
('ACC000002','ACC000014','MESSAGE','đã nhắn tin cho bạn.','',0,'2026-11-08 07:30:00'),
('ACC000010','ACC000001','REACTION','đã thả cảm xúc bài viết của bạn.','/index.html#post-SEEDP001',1,'2026-11-01 08:00:00'),
('ACC000013','ACC000007','COMMENT','đã bình luận bài viết của bạn.','/index.html#post-SEEDP004',1,'2026-11-02 07:20:00'),
('ACC000024','ACC000002','REACTION','đã thả cảm xúc bài viết của bạn.','/index.html#post-SEEDP024',0,'2026-11-08 11:10:00');

COMMIT;
PRAGMA foreign_keys = ON;

-- Xác nhận nhanh
SELECT 'users' AS tbl, COUNT(*) AS cnt FROM users
UNION ALL SELECT 'posts', COUNT(*) FROM posts
UNION ALL SELECT 'friendships', COUNT(*) FROM friendships
UNION ALL SELECT 'messages', COUNT(*) FROM messages
UNION ALL SELECT 'comments', COUNT(*) FROM comments
UNION ALL SELECT 'reactions', COUNT(*) FROM reactions
UNION ALL SELECT 'groups', COUNT(*) FROM groups
UNION ALL SELECT 'group_members', COUNT(*) FROM group_members
UNION ALL SELECT 'group_messages', COUNT(*) FROM group_messages
UNION ALL SELECT 'notifications', COUNT(*) FROM notifications
UNION ALL SELECT 'friend_requests', COUNT(*) FROM friend_requests;
