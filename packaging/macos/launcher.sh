#!/bin/sh
set -eu

CONTENTS="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
export AGEMIR_RESOURCE_DIR="$CONTENTS/Resources/agemir/resources"
export AGEMIR_CONFIG_FILE="${HOME:?HOME is not set}/Library/Application Support/Agemir/config.txt"

exec "$CONTENTS/MacOS/agemir-bin" "$@"
