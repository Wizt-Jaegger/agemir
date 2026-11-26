# Native package recipes

Packaging is intentionally gated on a successful Linux GTK window smoke. That
gate is stable locally, but the output formats have different verification
status:

| Platform | Recipe | Status in this checkout |
| --- | --- | --- |
| Linux x86_64 | `linux/appimage/build.sh` | Meson AppDir staging and AppRun UI smoke passed; final `.AppImage` assembly is unverified because linuxdeploy and its GTK plugin are unavailable. |
| Windows x86_64 | `windows/build.sh` | Recipe only; unverified until run in MSYS2 UCRT64 on Windows. Produces a portable folder containing `agemir.exe`, DLL dependencies, GTK data, resources, and a `.cmd` launcher. |
| macOS | `macos/build.sh` | Recipe only; unverified until run in a GTK-OSX jhbuild environment on macOS. Produces an `.app` and `.dmg`. |

All package launchers point the C core at bundled read-only resources and put
mutable config in the user's config directory. Direct CLI and development
launches retain the historical relative `resources/` paths unless the
`AGEMIR_RESOURCE_DIR` or `AGEMIR_CONFIG_FILE` environment variables are set.

## Linux AppImage

Run `packaging/linux/appimage/build.sh` on Linux after installing Meson, Ninja,
linuxdeploy, the linuxdeploy GTK plugin, and AppImage tooling expected by
linuxdeploy. It compiles and tests the Meson build, stages an AppDir, deploys
GTK4 resources (`DEPLOY_GTK_VERSION=4`), and emits
`dist/Agemir-<version>-x86_64.AppImage`. The GTK plugin currently selects its
X11 backend in the AppImage hook.

## Windows portable executable

Run `packaging/windows/build.sh` from the MSYS2 UCRT64 shell. Required packages
are `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-meson`,
`mingw-w64-ucrt-x86_64-ninja`, `mingw-w64-ucrt-x86_64-pkgconf`,
`mingw-w64-ucrt-x86_64-gtk4`, `mingw-w64-ucrt-x86_64-gtkmm-4.0`, and
`mingw-w64-ucrt-x86_64-libadwaita`. The recipe compiles and stages the
executable and its runtime files. It does not install Ollama.

Run `Agemir.cmd` from the output folder to set resource/config paths before
starting `agemir.exe`. The executable is included directly for integrations
that set those environment variables themselves.

## macOS disk image

Run `packaging/macos/build.sh` on macOS from a GTK-OSX jhbuild environment
that has Meson, Ninja, gtkmm4, and libadwaita available. The recipe uses
gtk-mac-bundler to collect the GTK runtime into an `.app`, then create-dmg to
make `dist/Agemir.dmg`. The bundle configuration and launcher are provisional
until a macOS host verifies the resulting app and DMG.

No Windows or macOS artifact has been built or tested in this checkout. No
recipe uploads or publishes an artifact.
