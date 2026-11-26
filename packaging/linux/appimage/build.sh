#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/../../.." && pwd)"
build_dir="${AGEMIR_BUILD_DIR:-$repo_root/build-appimage}"
output_dir="${AGEMIR_DIST_DIR:-$repo_root/dist}"
appdir="${AGEMIR_APPDIR:-$repo_root/packaging/out/Agemir.AppDir}"

if [[ "$(uname -s)" != Linux ]]; then
    echo "AppImage builds must run on Linux." >&2
    exit 2
fi
for command_name in meson ninja linuxdeploy; do
    command -v "$command_name" >/dev/null || {
        echo "Missing required command: $command_name" >&2
        exit 2
    }
done

if [[ -f "$build_dir/meson-private/coredata.dat" ]]; then
    meson configure "$build_dir" --prefix=/usr --buildtype=release
else
    meson setup "$build_dir" "$repo_root" --prefix=/usr --buildtype=release
fi
meson compile -C "$build_dir"
meson test -C "$build_dir" --print-errorlogs

rm -rf "$appdir"
mkdir -p "$appdir" "$output_dir"
DESTDIR="$appdir" meson install -C "$build_dir"
install -m 755 "$script_dir/AppRun" "$appdir/AppRun"

version="$(sed -n "s/^project('agemir'.*version: '\([^']*\)'.*/\1/p" "$repo_root/meson.build" | head -n 1)"
if [[ -z "$version" ]]; then
    echo "Could not read project version from meson.build." >&2
    exit 2
fi

OUTPUT="$output_dir/Agemir-$version-x86_64.AppImage" \
    linuxdeploy --appdir "$appdir" \
        --desktop-file "$script_dir/agemir.desktop" \
        --plugin gtk --output appimage
