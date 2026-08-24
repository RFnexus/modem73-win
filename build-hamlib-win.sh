#!/bin/sh
set -eu
HAMLIB_VERSION=4.6.5
HAMLIB_SHA256=90d6f1dba59417c00f8f4545131c7efd31930cd0e178598980a8210425e3852e
REPO=$(cd "$(dirname "$0")" && pwd)
BUILD=${HAMLIB_BUILD_DIR:-$REPO/deps/hamlib-build}
OUT=$REPO/deps/hamlib
SRC=$BUILD/hamlib-$HAMLIB_VERSION
TARBALL=$BUILD/hamlib-$HAMLIB_VERSION.tar.gz
TRIPLE=x86_64-w64-mingw32
JOBS=$(nproc)

mkdir -p "$BUILD"
if [ ! -f "$TARBALL" ]; then
    curl -sL -o "$TARBALL" "https://github.com/Hamlib/Hamlib/releases/download/$HAMLIB_VERSION/hamlib-$HAMLIB_VERSION.tar.gz"
fi
echo "$HAMLIB_SHA256  $TARBALL" | sha256sum -c -
if [ ! -f "$SRC/configure" ]; then
    tar -C "$BUILD" -xzf "$TARBALL"
fi

B=$BUILD/build-win64
PREFIX=$BUILD/prefix-win64
rm -rf "$B" "$PREFIX"
mkdir -p "$B" "$PREFIX"
(
    cd "$B"
    CFLAGS="-O2 -ffunction-sections -fdata-sections" \
    "$SRC/configure" \
        --host=$TRIPLE \
        --prefix="$PREFIX" \
        --disable-shared --enable-static \
        --without-cxx-binding \
        --without-libusb \
        --without-readline \
        --without-indi \
        --disable-winradio \
        --disable-parallel \
        --disable-html-matrix \
        > configure.log 2>&1
    make -j"$JOBS" > make.log 2>&1
    make install > install.log 2>&1
)

rm -rf "$OUT"
mkdir -p "$OUT/lib" "$OUT/include"
cp "$PREFIX/lib/libhamlib.a" "$OUT/lib/libhamlib.a"
cp -r "$PREFIX/include/hamlib" "$OUT/include/hamlib"
for SYM in rig_init rig_open rig_set_ptt rig_list_foreach; do
    ${TRIPLE}-nm --defined-only "$OUT/lib/libhamlib.a" 2>/dev/null | grep -q " T $SYM" \
        || { echo "missing symbol $SYM" >&2; exit 1; }
done
echo "hamlib $HAMLIB_VERSION for win64 ready in deps/hamlib"
