#!/usr/bin/env bash
set -euo pipefail

# Build only libstdc++ from the GCC source matching the selected compiler.
source_dir=$(realpath "$1")
mkdir -p "$2"
work=$(realpath "$2")
export CC=${CC:-gcc-14}
export CXX=${CXX:-g++-14}
test "$("$CXX" -dumpfullversion)" = "$(cat "$source_dir/gcc/BASE-VER")"
"$CXX" -v 2> "$work/compiler.log"
grep -F 'Thread model: posix' "$work/compiler.log"
# The GCC top-level build normally creates this POSIX thread selection.
test -e "$source_dir/libgcc/gthr-default.h" || \
    ln -s gthr-posix.h "$source_dir/libgcc/gthr-default.h"

export CFLAGS='-O2 -g -fno-omit-frame-pointer'
export CXXFLAGS='-O2 -g -fsanitize=thread -fno-omit-frame-pointer'
export LDFLAGS="-fsanitize=thread ${LDFLAGS:-}"
mkdir -p "$work/build"
cd "$work/build"
set -x
"$source_dir/libstdc++-v3/configure" \
    --prefix="$work/install" --libdir="$work/install/lib" \
    --disable-multilib --disable-libstdcxx-pch --disable-libstdcxx-backtrace \
    --disable-nls --enable-shared --disable-static
grep -F '#define _GLIBCXX_HAS_GTHREADS 1' include/bits/c++config.h
make -j "${TSAN_STDLIB_JOBS:-8}"
make install
set +x

runtime="$work/install/lib"
nm -D "$runtime/libstdc++.so.6" > "$work/runtime-symbols.log"
grep ' U __tsan_atomic32_fetch_sub' "$work/runtime-symbols.log"
ln -s "$(realpath "$("$CXX" -print-file-name=libtsan.so.2)")" "$runtime/libtsan.so.2"
sha256sum "$runtime/libstdc++.so.6" "$runtime/libtsan.so.2"
readelf -n "$runtime/libstdc++.so.6"
objdump -d --disassemble=_ZNSt15__exception_ptr13exception_ptr10_M_releaseEv \
    "$runtime/libstdc++.so.6" > "$work/exception-release.log"
grep '__tsan_atomic32_fetch_sub' "$work/exception-release.log"
