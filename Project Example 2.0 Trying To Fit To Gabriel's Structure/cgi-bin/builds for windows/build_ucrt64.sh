#!/usr/bin/env bash
set -euo pipefail

# --- Paths (edit if different) ---
UCRT_BIN="/c/msys64/ucrt64/bin"
UCRT_LIB="/c/msys64/ucrt64/lib"
UCRT_INCLUDE="/c/msys64/ucrt64/include"
MARIADB_INCLUDE="$UCRT_INCLUDE/mariadb"
CGI_OUT="/c/xampp/cgi-bin"       # where the CGI + DLLs land
CGI_NAME="server.cgi"            # final CGI name
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[build] Project: $PROJECT_DIR"
echo "[build] Output : $CGI_OUT/$CGI_NAME"
mkdir -p "$CGI_OUT"

# --- Compile & link ---
/usr/bin/g++ -O2 -std=c++17 \
  -I"$PROJECT_DIR" -I"$MARIADB_INCLUDE" \
  "$PROJECT_DIR"/server.cpp \
  "$PROJECT_DIR"/controllers.cpp \
  "$PROJECT_DIR"/auth.cpp \
  "$PROJECT_DIR"/db.cpp \
  "$PROJECT_DIR"/routes.cpp \
  "$PROJECT_DIR"/utils.cpp \
  "$PROJECT_DIR"/socket_handler.cpp \
  -L"$UCRT_LIB" -lmariadb \
  -static-libstdc++ -static-libgcc \
  -o "$CGI_OUT/$CGI_NAME"

echo "[copy] Resolving runtime DLLs to $CGI_OUT"
# Common runtime + MariaDB + OpenSSL (patterns handle minor version suffixes)
dlls=(
  "libmariadb.dll"
  "libssl-3*.dll"
  "libcrypto-3*.dll"
  "libiconv-2.dll"
  "zlib1.dll"
  "libwinpthread-1.dll"
  "libgcc_s_seh-1.dll"
  "libstdc++-6.dll"
)
for pat in "${dlls[@]}"; do
  for f in "$UCRT_BIN"/$pat; do
    if [ -f "$f" ]; then
      cp -u "$f" "$CGI_OUT"/
      echo "  + $(basename "$f")"
    fi
  done
done

# Optional: show unresolved deps (ntldd ships with MSYS2)
if command -v ntldd >/dev/null 2>&1; then
  echo "[check] Dependency scan (ntldd):"
  ntldd -R "$CGI_OUT/$CGI_NAME" || true
fi

echo "[done] Built $CGI_OUT/$CGI_NAME"
echo "Restart Apache if needed, then open http://localhost/auction/index.html"
