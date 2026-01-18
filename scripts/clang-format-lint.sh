#!/usr/bin/env sh

set -eu

CLANG_FORMAT_BIN=${CLANG_FORMAT_BIN:-}
USE_DOCKER=0

if [ -z "$CLANG_FORMAT_BIN" ]; then
    if command -v clang-format-9 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN=clang-format-9
    elif command -v docker >/dev/null 2>&1; then
        if docker info >/dev/null 2>&1; then
            USE_DOCKER=1
        elif command -v clang-format >/dev/null 2>&1; then
            echo "Warning: docker is installed but not accessible (is the daemon running, and are you in the 'docker' group?). Falling back to local clang-format." >&2
            CLANG_FORMAT_BIN=clang-format
        else
            echo "Warning: docker is installed but not accessible (is the daemon running, and are you in the 'docker' group?)." >&2
            echo "Error: clang-format not found. Install clang-format 9 or set CLANG_FORMAT_BIN." >&2
            exit 2
        fi
    elif command -v clang-format >/dev/null 2>&1; then
        CLANG_FORMAT_BIN=clang-format
    else
        echo "Error: clang-format not found. Install clang-format 9 or set CLANG_FORMAT_BIN." >&2
        exit 2
    fi
fi

if [ "$USE_DOCKER" -eq 1 ]; then
    if docker run --rm \
        -e GITHUB_WORKSPACE=/github/workspace \
        -e INPUT_SOURCE=. \
        -e INPUT_EXCLUDE=./third_party \
        -e INPUT_EXTENSIONS=h,c \
        -e INPUT_CLANGFORMATVERSION=9 \
        -v "$(pwd)":/github/workspace -w /github/workspace \
        doozy/clang-format-lint:0.5; then
        rc=0
    else
        rc=$?
    fi
    case "$rc" in
        0|1)
            # 0 = ok, 1 = formatting mismatch. Both are meaningful results.
            exit "$rc"
            ;;
        125|126|127)
            echo "Warning: docker-based clang-format 9 check failed (network/registry/daemon). Falling back to local clang-format." >&2
            ;;
        *)
            # Unknown error code; don't mask it.
            exit "$rc"
            ;;
    esac

    if [ -z "$CLANG_FORMAT_BIN" ]; then
        if command -v clang-format >/dev/null 2>&1; then
            CLANG_FORMAT_BIN=clang-format
        else
            echo "Error: no local clang-format available for fallback. Install clang-format-9 or clang-format." >&2
            exit 2
        fi
    fi
fi

version_str=$($CLANG_FORMAT_BIN --version 2>/dev/null || true)
if [ -n "$version_str" ]; then
    echo "Using: $version_str" >&2
    case "$version_str" in
        *"version 9"*) ;;
        *) echo "Warning: CI uses clang-format 9. Install clang-format-9 (or docker) to match CI output." >&2 ;;
    esac
fi

if command -v git >/dev/null 2>&1; then
    FILES=$(git ls-files '*.c' '*.h' | grep -v '^third_party/' || true)
else
    echo "Error: git not found; cannot determine file list." >&2
    exit 2
fi

if [ -z "$FILES" ]; then
    exit 0
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT INT TERM

failed=0

for f in $FILES; do
    out="$tmpdir/$(printf '%s' "$f" | tr '/\\' '__')"
    "$CLANG_FORMAT_BIN" "$f" > "$out"
    if ! cmp -s "$f" "$out"; then
        echo "clang-format mismatch: $f" >&2
        diff -u "$f" "$out" || true
        failed=1
    fi
done

exit $failed
