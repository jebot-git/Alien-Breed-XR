#!/bin/sh
# Build and verify the Runtime component as the regular user before invoking.
set -eu
[ "$(id -u)" -eq 0 ] || { echo 'Run this install step with sudo -A.' >&2; exit 1; }
abxr_build=${1:?Usage: install-system.sh RELEASE_BUILD_DIRECTORY}
[ -f "$abxr_build/CMakeCache.txt" ] || { echo 'Missing configured build.' >&2; exit 1; }
abxr_build_type=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$abxr_build/CMakeCache.txt")
[ "$abxr_build_type" = Release ] || { echo 'A Release build is required.' >&2; exit 1; }
# A private development umask must not make the shared installation unreadable.
umask 022
cmake --install "$abxr_build" --component Runtime
if command -v update-desktop-database >/dev/null 2>&1; then
    abxr_prefix=$(sed -n 's/^CMAKE_INSTALL_PREFIX:PATH=//p' "$abxr_build/CMakeCache.txt")
    update-desktop-database "$abxr_prefix/share/applications"
fi
