#!/usr/bin/env bash
# Build all CGI binaries with UCRT64 GCC and copy required DLLs to XAMPP cgi-bin.

set -euo pipefail

# --- Config (override via env if needed) ---
XAMPP_ROOT="${XAMPP_ROOT:-/c/xampp}"
CGI_DIR="${CGI_DIR:-$XAMPP_ROOT/cgi-bin}"
SRC_DIR="${SRC_DIR:-$PWD}"              # folder containing *.cpp sources
UCRT_PREFIX="/ucrt64"
INCLUDE_DIR="$UCRT_PREFIX/include"
LIB_DIR="$UCRT_PREFIX/lib"
BIN_DIR="$UCRT_PREFIX/bin"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++17}"
INCS="-I$INCLUDE_DIR"
LIBS="-L$LIB_DIR -lmariadb -lws2_32"

# List of programs to build (add db_probe.cpp if you keep it around)
SOURCES=(
  "auth.cpp"
  "bid_sell.cpp"
  "listings.cpp"
  "transactions.cpp"
)

# DLLs your CGIs typically need (copy if present)
DLLS=(
  libmariadb.dll
  libstdc++-6.dll
  libgcc_s_seh-1.dll
  libwinpthread-1.dll
  # common extras that libmariadb might depend on; copied only if found:
  libcurl-4.dll
  libzstd.dll
  zlib1.dll
  libiconv-2.dll
  libnghttp2-14.dll
  libidn2-0.dll
  libunistring-2.dll
  libssh2-1.dll
  libcrypto-3-x64.dll
  libssl-3-x64.dll
)

echo "==> MSYSTEM: ${MSYSTEM:-unknown}"
if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "!! This script is meant for the MSYS2 UCRT64 shell. You are in: ${MSYSTEM:-unknown}" >&2
  echo "   Open 'MSYS2 UCRT64' from Start Menu and run again." >&2
  exit 1
fi

# Ensure directories exist
mkdir -p "$CGI_DIR"

# Ensure required packages are installed
echo "==> Checking toolchain & client libs..."
if ! pacman -Qq mingw-w64-ucrt-x86_64-gcc >/dev/null 2>&1; then
  echo "==> Installing UCRT64 GCC..."
  pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc
fi
if ! pacman -Qq mingw-w64-ucrt-x86_64-libmariadbclient >/dev/null 2>&1; then
  echo "==> Installing MariaDB client dev libs..."
  pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-libmariadbclient
fi

# Compile all programs
echo "==> Building CGI programs into $CGI_DIR"
for src in "${SOURCES[@]}"; do
  if [[ ! -f "$SRC_DIR/$src" ]]; then
    echo "!! Missing source: $SRC_DIR/$src" >&2
    exit 1
  fi
  out="$CGI_DIR/${src%.cpp}.cgi"
  echo "   - $src -> $(cygpath -w "$out")"
  "$CXX" $CXXFLAGS $INCS "$SRC_DIR/$src" -o "$out" $LIBS
  chmod +x "$out" || true
done

# Copy runtime DLLs next to the CGIs
echo "==> Copying runtime DLLs into $CGI_DIR"
for dll in "${DLLS[@]}"; do
  [[ -f "$BIN_DIR/$dll" ]] || continue
  cp -u "$BIN_DIR/$dll" "$CGI_DIR/" && echo "   + $dll"
done

# If libmysql.dll is present, back it up so libmariadb.dll is used
if [[ -f "$CGI_DIR/libmysql.dll" ]]; then
  ts=$(date +%Y%m%d-%H%M%S)
  mv -f "$CGI_DIR/libmysql.dll" "$CGI_DIR/libmysql.dll.bak.$ts"
  echo "==> Backed up libmysql.dll -> libmysql.dll.bak.$ts"
fi

echo "==> Done!"
echo "CGI dir: $(cygpath -w "$CGI_DIR")"
echo "Try: http://localhost/cgi-bin/auth.cgi"
