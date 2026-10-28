# Standalone releases

These Release builds exclude the DomeD client (`DE_XR_STANDALONE=ON`) and do not
link libcurl, inspect compositor sockets, or call `/api/native-xr`. The normal
Linux developer build retains optional DomeD integration. No Qt frontend is added.

Linux packages use the distribution's SDL2, OpenGL and Khronos OpenXR loader.
Install/configure a compatible OpenXR runtime separately. No WiVRn-Pro library,
private runtime extension, server executable or client build is required.
Launch with `--xr` for the headset or `--desktop` for a monitor. Without the
optional `XR_MNDX_egl_enable` graphics binding, the standalone build selects X11/GLX
when a display is available, including for a later desktop-to-XR toggle. Otherwise
native Wayland/EGL is preferred. Explicit `SDL_VIDEODRIVER` overrides are preserved.
The renderer requires an OpenXR runtime supporting `XR_KHR_opengl_enable`.

The Windows x86_64 ZIP uses standard Win32/WGL and contains the generic Khronos
OpenXR loader plus required runtime DLLs and their notices. Extract the whole ZIP
and run the executable in `bin`. Select your headset's OpenXR runtime using its
normal system configuration; no Linux desktop, DomeD or WiVRn-Pro build is needed.
The Windows build is cross-compiled with MinGW and smoke-tested under Wine;
physical Windows/headset tracking and vendor runtime validation remain separate.

## Rebuild

Run as a regular user with rootless Podman, Python 3, Git and GNU tar/zstd.
From this directory:

```sh
python3 build-release.py ubuntu
python3 build-release.py fedora
python3 build-release.py arch
python3 fetch-windows-sdk.py /absolute/path/to/windows-sdk
python3 build-release.py windows --windows-sdk /absolute/path/to/windows-sdk
```

The default output is the repository’s `dist/` directory; `--output` can select an absolute directory. Builders use Ubuntu 24.04 LTS, Fedora 44 and
current Arch. Compilation runs as the caller; native `.deb`, `.rpm` and
`.pkg.tar.zst` metadata declares system dependencies. Windows SDK downloads are
pinned by URL/version/SHA256 in `windows-sdk-manifest.json`. No mock runtime ships.
`--image` reuses a prebuilt builder image. Build logs, staged files and per-build
provenance remain in the output work directory.

The second Alien Breed game additionally needs its original converted local
`assets/` tree with `assets/native/manifest.json` before packaging. The build
never downloads original disks or executes a conversion toolchain. Existing
source and game-asset notices remain applicable and accompany every package;
these release recipes do not grant new rights over original game materials.

## Check generic OpenXR

Configure a local Release build with `-DDE_XR_STANDALONE=ON`, tests enabled, and
(for the second Alien Breed game) `-DAB3D2_BUILD_GPU_TESTS=ON`. With Xvfb available,
CTest adds `standalone-generic-openxr`. It checks stereo frames through EGL and
GLX with vendor extensions absent and no DomeD service. Existing gameplay,
asset and renderer suites continue to apply. Tests use private displays, data
and runtime fixtures and do not connect to the user's headset/session.
