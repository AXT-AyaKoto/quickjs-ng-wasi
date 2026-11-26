#!/usr/bin/env bash
# Build dist/qjs.wasm for WASI preview 1 with wasi-sdk.
# wasm32-wasip1 is the current target name; older SDKs still call it wasm32-wasi.
# Requires WASI_SDK_PATH. Works on Linux and macOS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ -z "${WASI_SDK_PATH:-}" ]]; then
    echo "WASI_SDK_PATH is not set" >&2
    exit 1
fi

CC="$WASI_SDK_PATH/bin/clang"
SYSROOT="$WASI_SDK_PATH/share/wasi-sysroot"

if [[ ! -x "$CC" ]]; then
    echo "clang not found: $CC" >&2
    exit 1
fi
if [[ ! -d "$SYSROOT" ]]; then
    echo "wasi sysroot not found: $SYSROOT" >&2
    exit 1
fi
if [[ ! -f vendor/quickjs/quickjs.c ]]; then
    echo "vendor/quickjs is missing. Run: git submodule update --init" >&2
    exit 1
fi

# wasi-sdk 34 keeps headers under include/<triple>/ and dropped the
# wasm32-wasi alias's sysroot. Older SDKs still use a single include/ tree.
if [[ -f "$SYSROOT/include/wasm32-wasip1/stdlib.h" ]]; then
    TARGET="wasm32-wasip1"
elif [[ -f "$SYSROOT/include/wasm32-wasi/stdlib.h" || -f "$SYSROOT/include/stdlib.h" ]]; then
    TARGET="wasm32-wasi"
else
    echo "no wasi sysroot headers under $SYSROOT" >&2
    exit 1
fi

# 128 MiB. QuickJS itself is capped at 120 MiB in src/main.c.
STACK_SIZE=134217728

CFLAGS=(
    --target="$TARGET"
    --sysroot="$SYSROOT"
    -O2
    -std=gnu11
    -funsigned-char
    -D_GNU_SOURCE
    -DQUICKJS_NG_BUILD
    -D_WASI_EMULATED_PROCESS_CLOCKS
    -D_WASI_EMULATED_SIGNAL
    -I"$ROOT/vendor/quickjs"
)
LDFLAGS=(
    --target="$TARGET"
    --sysroot="$SYSROOT"
    -O2
    -Wl,-z,stack-size="$STACK_SIZE"
)
LIBS=(
    -lm
    -lwasi-emulated-process-clocks
    -lwasi-emulated-signal
)

mkdir -p build dist

stamp_new="build/cflags.new"
stamp="build/cflags.stamp"
{
    printf '%s\n' "$TARGET"
    printf '%s\n' "${CFLAGS[@]}"
    printf '%s\n' "${LDFLAGS[@]}"
    printf '%s\n' "${LIBS[@]}"
} >"$stamp_new"
if [[ ! -f "$stamp" ]] || ! cmp -s "$stamp" "$stamp_new"; then
    rm -f build/*.o
    mv "$stamp_new" "$stamp"
else
    rm -f "$stamp_new"
fi

objs=()
compile() {
    local src="$1"
    local obj="$2"
    if [[ ! -f "$obj" || "$src" -nt "$obj" ]]; then
        echo "cc $src"
        "$CC" "${CFLAGS[@]}" -c "$src" -o "$obj"
    fi
    objs+=("$obj")
}

compile "$ROOT/vendor/quickjs/dtoa.c" "$ROOT/build/dtoa.o"
compile "$ROOT/vendor/quickjs/libregexp.c" "$ROOT/build/libregexp.o"
compile "$ROOT/vendor/quickjs/libunicode.c" "$ROOT/build/libunicode.o"
compile "$ROOT/vendor/quickjs/quickjs.c" "$ROOT/build/quickjs.o"
compile "$ROOT/vendor/quickjs/quickjs-libc.c" "$ROOT/build/quickjs-libc.o"
compile "$ROOT/src/main.c" "$ROOT/build/main.o"

echo "ld dist/qjs.wasm ($TARGET)"
"$CC" "${LDFLAGS[@]}" -o "$ROOT/dist/qjs.wasm" "${objs[@]}" "${LIBS[@]}"

(
    cd "$ROOT/dist"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum qjs.wasm > qjs.wasm.sha256
    else
        shasum -a 256 qjs.wasm > qjs.wasm.sha256
    fi
)
cat "$ROOT/dist/qjs.wasm.sha256"
