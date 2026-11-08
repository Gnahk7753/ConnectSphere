# Dockerfile chuẩn để deploy lên VPS Linux / Render / Railway
FROM ubuntu:22.04

# Cài đặt trình biên dịch g++, cmake và thư viện SQLite3
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy mã nguồn dự án
COPY . /app

# Biên dịch ứng dụng C++
RUN g++ -std=c++17 src/main.cpp -Iinclude -Isrc -lsqlite3 -lpthread -O2 -o server

# Mở cổng 8080
EXPOSE 8080

# Chạy server
CMD ["./server"]
