#!/usr/bin/env bash
# Seeder dữ liệu phong phú — idempotent
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DB="$ROOT/data/social_network.db"
SQL="$ROOT/data/seed_rich_data.sql"
if [[ ! -f "$DB" ]]; then
  echo "Không tìm thấy $DB — hãy chạy server một lần để khởi tạo DB."
  exit 1
fi
echo "Đang seed vào $DB ..."
sqlite3 "$DB" < "$SQL"
echo "Hoàn tất seeder."
