#!/bin/sh
# Compile desktop sizes from the authored transparent raster master.
set -eu
abxr_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
abxr_icons="$abxr_root/xr/assets/icons"
for abxr_size in 16 24 32 48 64 96 128 256 512 1024; do
    abxr_dest="$abxr_icons/hicolor/${abxr_size}x${abxr_size}/apps"
    mkdir -p -- "$abxr_dest"
    magick "$abxr_icons/alien-breed-xr-master.png" -filter Lanczos -resize "${abxr_size}x${abxr_size}" -strip "PNG32:$abxr_dest/alien-breed-xr.png"
done
