#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/../.." && pwd)"
build_dir="${AGEMIR_BUILD_DIR:-$repo_root/build-macos}"
stage_dir="${AGEMIR_STAGE_DIR:-$repo_root/packaging/out/macos}"
dist_dir="${AGEMIR_DIST_DIR:-$repo_root/dist}"
prefix="${JHBUILD_PREFIX:-}"

if [[ "$(uname -s)" != Darwin ]]; then
    echo "DMG builds must run on macOS." >&2
    exit 2
fi
if [[ -z "$prefix" ]]; then
    echo "Enter the GTK-OSX jhbuild environment before building." >&2
    exit 2
fi
for command_name in jhbuild gtk-mac-bundler create-dmg; do
    command -v "$command_name" >/dev/null || {
        echo "Missing required command: $command_name" >&2
        exit 2
    }
done

if [[ -f "$build_dir/meson-private/coredata.dat" ]]; then
    jhbuild run meson configure "$build_dir" --prefix="$prefix" --buildtype=release
else
    jhbuild run meson setup "$build_dir" "$repo_root" --prefix="$prefix" --buildtype=release
fi
jhbuild run meson compile -C "$build_dir"
jhbuild run meson test -C "$build_dir" --print-errorlogs
jhbuild run meson install -C "$build_dir"
cp "$prefix/bin/agemir" "$prefix/bin/agemir-bin"

rm -rf "$stage_dir"
mkdir -p "$stage_dir" "$dist_dir"
bundle_definition="$stage_dir/bundle-definition"
mkdir -p "$bundle_definition"
cp "$script_dir/Agemir.bundle" "$script_dir/launcher.sh" "$bundle_definition/"
version="$(sed -n "s/^project('agemir'.*version: '\([^']*\)'.*/\1/p" "$repo_root/meson.build" | head -n 1)"
sed "s/@VERSION@/$version/g" "$script_dir/Info.plist" >"$bundle_definition/Info.plist"
export JHBUILD_PREFIX="$prefix"
export AGEMIR_BUNDLE_DEST="$stage_dir"
jhbuild run gtk-mac-bundler "$bundle_definition/Agemir.bundle"
create-dmg "$dist_dir/Agemir.dmg" "$stage_dir/Agemir.app"
