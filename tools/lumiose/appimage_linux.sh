#!/bin/bash
# Pack the portable Lumiose Linux folder (tools/lumiose/package_linux.sh: build/dist/"Lumiose Online Linux") into one
# AppImage that installs and starts it (tools/lumiose/appimage/AppRun says how and where):
# build/dist/Lumiose-Online-<launcher version>-x86_64.AppImage. No FUSE needed to build (appimagetool runs extracted:
# containers, CI). Uses the same pinned appimagetool and type2 runtime as tools/za/dist-linux.sh.
#   tools/lumiose/appimage_linux.sh [package folder] [output file]
# No game code or game files: the player's own dump is set up on their machine, as with the zip.
set -euo pipefail
cd "$(dirname "$0")/../.."
SRC=${1:-$PWD/build/dist/Lumiose Online Linux}
SRC=$(realpath "$SRC")
for f in "Lumiose Online" usr/bin/lasrecomp usr/bin/python/bin/python3.12 usr/bin/tools/lumiose/entry.py \
         usr/bin/tools/lumiose/linux_launcher_update.py usr/bin/icon.png; do
    [ -e "$SRC/$f" ] || { echo "appimage_linux: $SRC/$f missing (tools/lumiose/package_linux.sh first)" >&2; exit 1; }
done
# (the payload's own version, not this checkout's: the AppRun compares the same line with the installed copy's)
VERSION=$(sed -n "s/^LAUNCHER_VERSION = '\(v[0-9][0-9.]*\)'.*/\1/p" "$SRC/usr/bin/tools/lumiose/entry.py" | head -1)
[ -n "$VERSION" ] || { echo "appimage_linux: no LAUNCHER_VERSION in the package's entry.py" >&2; exit 1; }
ARCH=x86_64
OUT=${2:-$PWD/build/dist/Lumiose-Online-$VERSION-$ARCH.AppImage}
L=$PWD/build/linux/$ARCH
APPDIR=$L/lumiose-AppDir

# A game file or game code must never enter the AppImage (as package_linux.sh checks its folder).
if find "$SRC" -type f \( -iname 'lasgame.*' -o -iname 'main.nso' -o -iname 'main.npdm' -o -iname 'romfs.bin' \
        -o -iname '*.nsp' -o -iname '*.xci' -o -iname '*.nca' -o -iname 'prod.keys' -o -iname 'title.keys' \) | grep -q . ||
   [ -e "$SRC/usr/bin/extracted" ] || [ -e "$SRC/usr/bin/save" ] || [ -e "$SRC/usr/bin/generated" ]; then
    echo "appimage_linux: game data or player data in $SRC: package from a clean package_linux.sh folder" >&2; exit 1
fi

rm -rf "$APPDIR" && mkdir -p "$APPDIR"
cp -a "$SRC" "$APPDIR/payload"
cp tools/lumiose/appimage/AppRun "$APPDIR/AppRun" && chmod 755 "$APPDIR/AppRun"
# (the AppImage's own entry, for AppImageLauncher / Gear Lever integration; the AppRun writes the menu entry
# that starts the installed copy)
cat > "$APPDIR/lumiose-online.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Lumiose Online
Comment=ZA Recomp with Lumiose Online (bring your own game files)
Exec=AppRun
Icon=lumiose-online
Terminal=false
Categories=Game;
EOF
cp docs/icon/icon_1024.png "$APPDIR/lumiose-online.png"
ln -sf lumiose-online.png "$APPDIR/.DirIcon"
echo "$VERSION" > "$APPDIR/VERSION"
command -v desktop-file-validate >/dev/null && desktop-file-validate "$APPDIR/lumiose-online.desktop"

mkdir -p build/linux/src "$(dirname "$OUT")"
fetch() {  # url sha256 file
    [ -f "build/linux/src/$3" ] || { curl -fsSL -o "build/linux/src/$3.part" "$1" && mv "build/linux/src/$3.part" "build/linux/src/$3"; }
    echo "$2  build/linux/src/$3" | sha256sum -c --quiet
}
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
      ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 appimagetool-x86_64.AppImage
fetch https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
      2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d runtime-x86_64
chmod +x build/linux/src/appimagetool-x86_64.AppImage
rm -f "$OUT"
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=$ARCH build/linux/src/appimagetool-x86_64.AppImage --no-appstream \
    --runtime-file build/linux/src/runtime-x86_64 "$APPDIR" "$OUT"
rm -rf "$APPDIR"
ls -la "$OUT"
