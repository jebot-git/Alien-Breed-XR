#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cmake -S "$root" -B "$root/build-xr" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAB3D_XR=ON "$@"
cmake --build "$root/build-xr" --parallel "${BUILD_JOBS:-4}"
