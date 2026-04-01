#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
STAGING_DIR="$ROOT_DIR/.deb-package"
OUTPUT_DIR="$ROOT_DIR/dist"
BINARY_NAME="desktop_translate"
PACKAGE_NAME="desktop-translate"
ICON_SOURCE="$ROOT_DIR/Icons8/icons8-translate-100.png"
MAINTAINER="${DEB_MAINTAINER:-Desktop Translate Maintainer <maintainer@example.com>}"

extract_version() {
    sed -nE 's/^project\(desktop_translate VERSION ([^ ]+) LANGUAGES CXX\)$/\1/p' "$ROOT_DIR/CMakeLists.txt" | head -n 1
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "错误: 未找到命令 $1"
        exit 1
    fi
}

require_command cmake
require_command dpkg-deb
require_command dpkg-query

VERSION="${1:-$(extract_version)}"
if [ -z "$VERSION" ]; then
    echo "错误: 无法从 CMakeLists.txt 读取版本号，请通过 ./build_deb.sh <version> 指定版本"
    exit 1
fi

ARCH="$(dpkg --print-architecture)"
PACKAGE_FILE="$OUTPUT_DIR/${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"

echo "==> 构建 Release 版本"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> 准备打包目录"
rm -rf "$STAGING_DIR"
mkdir -p \
    "$STAGING_DIR/DEBIAN" \
    "$OUTPUT_DIR" \
    "$STAGING_DIR/usr/share/applications" \
    "$STAGING_DIR/usr/share/pixmaps" \
    "$STAGING_DIR/usr/share/icons/hicolor/128x128/apps"

echo "==> 安装文件到临时目录"
DESTDIR="$STAGING_DIR" cmake --install "$BUILD_DIR" --prefix /usr

if [ ! -x "$STAGING_DIR/usr/bin/$BINARY_NAME" ]; then
    echo "错误: 未找到可执行文件 $STAGING_DIR/usr/bin/$BINARY_NAME"
    exit 1
fi

if [ ! -f "$ICON_SOURCE" ]; then
    echo "错误: 未找到图标文件 $ICON_SOURCE"
    exit 1
fi

install -m 0644 "$ICON_SOURCE" "$STAGING_DIR/usr/share/pixmaps/${PACKAGE_NAME}.png"
install -m 0644 "$ICON_SOURCE" "$STAGING_DIR/usr/share/icons/hicolor/128x128/apps/${PACKAGE_NAME}.png"

cat > "$STAGING_DIR/usr/share/applications/${PACKAGE_NAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Desktop Translate
Name[zh_CN]=桌面翻译
Comment=Desktop translation tool
Comment[zh_CN]=桌面划词翻译工具
Exec=/usr/bin/${BINARY_NAME}
Icon=/usr/share/pixmaps/${PACKAGE_NAME}.png
Terminal=false
Categories=Utility;
EOF

DEPENDS=""
if command -v dpkg-shlibdeps >/dev/null 2>&1; then
    SHLIBDEPS_OUTPUT="$(dpkg-shlibdeps -O "$STAGING_DIR/usr/bin/$BINARY_NAME" 2>/dev/null || true)"
    DEPENDS="$(printf '%s\n' "$SHLIBDEPS_OUTPUT" | sed -n 's/^shlibs:Depends=//p' | head -n 1)"
fi

if [ -z "$DEPENDS" ]; then
    DEPENDS="libqt6core6, libqt6gui6, libqt6widgets6, libcurl4, libx11-6"
fi

cat > "$STAGING_DIR/DEBIAN/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Depends: ${DEPENDS}
Recommends: xdotool, xsel
Description: Desktop translation tool based on OCR and LLM
 A desktop translation utility built with Qt6, supporting OCR,
 clipboard translation and floating translation bubbles.
EOF

echo "==> 生成 deb 安装包"
if dpkg-deb --help 2>/dev/null | grep -q -- '--root-owner-group'; then
    dpkg-deb --build --root-owner-group "$STAGING_DIR" "$PACKAGE_FILE"
elif command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build "$STAGING_DIR" "$PACKAGE_FILE"
else
    dpkg-deb --build "$STAGING_DIR" "$PACKAGE_FILE"
fi

echo ""
echo "打包完成:"
echo "  $PACKAGE_FILE"
echo ""
echo "安装方式:"
echo "  sudo apt install $PACKAGE_FILE"
