#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/../.." && pwd)"
build_dir="${AGEMIR_BUILD_DIR:-$repo_root/build-windows-ucrt64}"
stage_dir="${AGEMIR_STAGE_DIR:-$repo_root/packaging/out/windows-stage}"
bundle_dir="${AGEMIR_DIST_DIR:-$repo_root/dist/Agemir-windows-x86_64}"

if [[ "${MSYSTEM:-}" != UCRT64 || -z "${MINGW_PREFIX:-}" ]]; then
    echo "Run this script inside the MSYS2 UCRT64 shell." >&2
    exit 2
fi
for command_name in meson ninja ldd; do
    command -v "$command_name" >/dev/null || {
        echo "Missing required command: $command_name" >&2
        exit 2
    }
done

if [[ -f "$build_dir/meson-private/coredata.dat" ]]; then
    meson configure "$build_dir" --prefix=/ --buildtype=release
else
    meson setup "$build_dir" "$repo_root" --prefix=/ --buildtype=release
fi
meson compile -C "$build_dir"

rm -rf "$stage_dir" "$bundle_dir"
mkdir -p "$stage_dir" "$bundle_dir"
DESTDIR="$stage_dir" meson install -C "$build_dir"
cp "$stage_dir/bin/agemir.exe" "$bundle_dir/agemir.exe"
cp -R "$stage_dir/share/agemir/resources" "$bundle_dir/resources"

ldd "$bundle_dir/agemir.exe" | awk '$2 == "=>" && $3 ~ /^\// { print $3 }' |
while IFS= read -r dependency; do
    case "${dependency,,}" in
        */windows/system32/*|*/windows/syswow64/*) continue ;;
    esac
    cp -n "$dependency" "$bundle_dir/"
done

for relative_path in \
    share/glib-2.0/schemas \
    share/gtk-4.0 \
    share/icons \
    lib/gtk-4.0 \
    lib/gdk-pixbuf-2.0; do
    if [[ -d "$MINGW_PREFIX/$relative_path" ]]; then
        mkdir -p "$bundle_dir/$(dirname "$relative_path")"
        cp -R "$MINGW_PREFIX/$relative_path" "$bundle_dir/$(dirname "$relative_path")/"
    fi
done

cat >"$bundle_dir/Agemir.cmd" <<'EOF'
@echo off
setlocal
set "AGEMIR_RESOURCE_DIR=%~dp0resources"
set "AGEMIR_CONFIG_FILE=%APPDATA%\Agemir\config.txt"
set "GTK_DATA_PREFIX=%~dp0"
set "GTK_EXE_PREFIX=%~dp0"
set "GSETTINGS_SCHEMA_DIR=%~dp0share\glib-2.0\schemas"
"%~dp0agemir.exe" %*
EOF
