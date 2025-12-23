#!/usr/bin/env bash
set -e

OUT_FILE="$1"
if [ -z "$OUT_FILE" ]; then
    echo "Usage: $0 <output_header>"
    exit 1
fi

mkdir -p "$(dirname "$OUT_FILE")"

# ——————————————————————————————
# Получение тега
# ——————————————————————————————
if git describe --tags --abbrev=0 >/dev/null 2>&1; then
    TAG=$(git describe --tags --abbrev=0)
else
    TAG="0.0"
fi

# Парсинг major.minor
MAJOR=$(echo "$TAG" | cut -d'.' -f1)
MINOR=$(echo "$TAG" | cut -d'.' -f2)

MAJOR=${MAJOR:-0}
MINOR=${MINOR:-0}

# ——————————————————————————————
# Revision = count of commits
# ——————————————————————————————
if git rev-list --count HEAD >/dev/null 2>&1; then
    REVISION=$(git rev-list --count HEAD)
else
    REVISION=0
fi

# ——————————————————————————————
# Полный hash
# ——————————————————————————————
FULL_HASH=$(git rev-parse HEAD 2>/dev/null || echo "0000000000000000000000000000000000000000")

# ——————————————————————————————
# Короткий хэш "как на GitHub":
#   GitHub показывает 7–8 символов
#   мы берём строго 8 hex → uint32_t = 4 байта
# ——————————————————————————————
SHORT_HASH_HEX=$(echo "$FULL_HASH" | cut -c1-8)
SHORT_HASH=$((16#$SHORT_HASH_HEX))

# ——————————————————————————————
# Dirty flag
# ——————————————————————————————
if git diff --quiet --ignore-submodules HEAD 2>/dev/null; then
    DIRTY=0
else
    DIRTY=1
fi

# ——————————————————————————————
# Timestamp
# ——————————————————————————————
BUILD_TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# ——————————————————————————————
# Генерация файла
# ——————————————————————————————
cat <<EOF > "$OUT_FILE"
#pragma once
#include <cstdint>

static constexpr uint8_t   DEVICE_HW_REVISION = 1;
static constexpr uint8_t   DEVICE_SW_MAJOR    = $MAJOR;
static constexpr uint8_t   DEVICE_SW_MINOR    = $MINOR;
static constexpr uint32_t  DEVICE_SW_REVISION = $REVISION;

// Полная строка SHA-1
static constexpr const char* DEVICE_GIT_HASH_FULL = "$FULL_HASH";

// Короткий 4-байтный hash (8 hex символов)
static constexpr uint32_t DEVICE_GIT_HASH_SHORT = 0x$SHORT_HASH_HEX;

// Dirty flag: 1 = есть изменения, 0 = clean
static constexpr uint8_t DEVICE_GIT_DIRTY = $DIRTY;

// Timestamp сборки
static constexpr const char* DEVICE_BUILD_TIMESTAMP = "$BUILD_TIMESTAMP";

EOF

echo "Generated $OUT_FILE"
