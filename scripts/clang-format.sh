#!/usr/bin/env sh

set -eu

CLANG_FORMAT_BIN=${CLANG_FORMAT_BIN:-}
USE_DOCKER=0

DOCKER_UID=${SUDO_UID:-}
DOCKER_GID=${SUDO_GID:-}
if [ -z "$DOCKER_UID" ]; then
    DOCKER_UID=$(id -u)
fi
if [ -z "$DOCKER_GID" ]; then
    DOCKER_GID=$(id -g)
fi

if [ "$(id -u)" -eq 0 ] && [ -n "${SUDO_UID:-}" ]; then
    echo "Warning: running via sudo can cause files to become owned by root. Prefer running 'make format' without sudo." >&2
fi

if [ -z "$CLANG_FORMAT_BIN" ]; then
    if command -v clang-format-9 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN=clang-format-9
    elif command -v docker >/dev/null 2>&1; then
        if docker info >/dev/null 2>&1; then
            USE_DOCKER=1
        else
            echo "Warning: docker is installed but not accessible (is the daemon running, and are you in the 'docker' group?). Falling back to local clang-format." >&2
        fi
    elif command -v clang-format >/dev/null 2>&1; then
        CLANG_FORMAT_BIN=clang-format
    else
        echo "Error: clang-format not found. Install clang-format 9 or set CLANG_FORMAT_BIN." >&2
        exit 2
    fi
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

if [ "$USE_DOCKER" -eq 1 ]; then
    if docker run --rm --user "$DOCKER_UID:$DOCKER_GID" --entrypoint sh -v "$(pwd)":/github/workspace -w /github/workspace \
        doozy/clang-format-lint:0.5 -c '/clang-format/clang-format9 -i "$@"' sh $FILES; then
        exit 0
    fi

    echo "Warning: docker-based clang-format 9 format failed (network/registry/daemon). Falling back to local clang-format." >&2

    if [ -z "$CLANG_FORMAT_BIN" ]; then
        if command -v clang-format >/dev/null 2>&1; then
            CLANG_FORMAT_BIN=clang-format
        else
            echo "Error: no local clang-format available for fallback. Install clang-format-9 or clang-format." >&2
            exit 2
        fi
    fi
fi

for f in $FILES; do
    "$CLANG_FORMAT_BIN" -i "$f"
done
