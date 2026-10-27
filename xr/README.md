# Alien Breed XR — native port attempt

This branch extends retro-foundry's native C translation of **Alien Breed 3D I**,
including its original Amiga source reference and data. It runs the translated
gameplay directly; it does not launch an emulator or render a flat game screen
inside a headset.

The upstream baseline is `61a13bc47a22bae885b44de2ed03b4ba1246d420` from
<https://github.com/retro-foundry/Alien-Breed-HD>. Original source and data remain
in `amiga/`; upstream documentation is retained at the repository root.

## Implemented

- Native OpenGL world rendering of the loaded level's walls, floor/roof polygons
  and basic animated sprite billboards, with depth testing. Floor polygon
  topology is tessellated once and cached in GPU display lists; texture and
  sprite frames are decoded/uploaded on first use. Live wall/floor heights are
  reread so the original door/lift logic can update them.
- Original movement, collision, AI, pickups, weapon logic, audio and level data.
  The original software world rasterizer is bypassed in the XR build.
- OpenXR stereo projection layers using both runtime eye poses, asymmetric FOVs,
  predicted display time and separate color/depth targets. Positional head
  tracking, pitch, roll and yaw affect the actual world projection. Scale is
  32 original horizontal world units per metre, giving a 1.5 m original player.
- OpenXR action sets with both tracked controller aim poses, trigger/select,
  movement and snap-turn axes, use, pause and trigger haptic feedback.
  Controller position and orientation determine the firing origin/yaw/pitch;
  the player's locomotion pose is restored after the original shooting routine.
- The native XR input/background lease used by Tempest XR: validated DomeD
  launch records and Wayland peer, `/api/native-xr` ownership, heartbeat,
  focus/lock suspension, background restoration, and explicit resume. The game
  pauses when backgrounded and closes its XR session; it never restarts DomeD.
  The current SDL Wayland surface is resolved again before requesting ownership
  because SDL can remap it during graphics initialization.
- Native Wayland/EGL when launched under DomeD; explicit desktop and XR modes.
  GLX is an optional compatibility route outside DomeD.

## Build and run

Dependencies: C/C++ compilers, CMake, Ninja, Python, SDL2 development files,
OpenGL/GLU/EGL, OpenXR loader/headers, X11 development files (optional GLX binding
compiled into the backend), libcurl, nlohmann-json and threads. The upstream
asset staging step also uses ffmpeg/libopenmpt for tracker-to-WAV conversion.
No dependencies were installed system-wide for this attempt.

```sh
./xr/tools/build.sh
./build-xr/alien-breed-xr --desktop --level 1
./build-xr/alien-breed-xr --xr --level 1
```

Without a mode argument, a validated DomeD XR launch realm requests OpenXR and a
DomeD desktop launch realm stays on the desktop. Outside DomeD, the default is
desktop. `--xr` requires a configured OpenXR runtime/headset; if initialization
fails, the reason is printed and the desktop view remains available.

F8 switches between XR and desktop. Escape pauses/releases input; focus the game
and press Enter to resume. R recentres tracking. Ctrl+Q quits. Original desktop
movement/shooting bindings remain available. In XR, left stick moves using the
original movement physics, right stick snap-turns by 30 degrees, right trigger
fires, A/X uses, and left menu pauses (Index uses left B). The simple-controller
profile supports right select to fire and left select to use; it has no stick
locomotion. Touch, Index and Vive bindings are implemented but not validated on
physical controllers. The controller pointers are simple 3D lines.

`--frames N`, `--report FILE.json` and `--capture FILE.ppm` are test hooks.
Production rendering has no framebuffer readback; capture is explicit.
`-DAB3D_XR=OFF` retains the upstream software-rendered executable.

## Limits of this milestone

This is a working source-port prototype, **not a completed VR conversion**.
Curved-wall records, polygon models, complete sprite frame/type fallbacks,
water/bump effects, lighting/material accuracy, sky, weapon models and the
original HUD/menu presentation are not fully ported. Unknown/unavailable sprite
frames have coloured markers. Floor texture scale and wall UV fidelity need
further comparison with the original renderer. The original collision/AI still
uses its authored room/zone representation; physical leaning has no new head or
hand collision model. Shot origin changes are limited to a tracked controller
within 1.5 m and the player's current zone. Vertical aim retains the original
engine's signed aim-speed range (approximately ±45 degrees).

There is no VR-native menu/weapon-selection interface, controller-only resume,
hand interaction/grabbing, two-handed weapon model, save-format migration or
new FLTK settings window. Original desktop/save paths and settings mechanisms
are retained. Campaign completion and every weapon/enemy combination have not
been play-tested. All 16 level loading/render smoke checks are distinct from
full campaign compatibility. A physical headset/controller session, comfort,
tracking-loss recovery under a real runtime and performance at headset
resolution remain unverified.

## Verification

```sh
python3 xr/tests/test_native.py
uv run --no-project --with python-xlib python xr/tests/test_domed.py
```

The native test uses offscreen EGL and the actual GPU, loads all 16 levels,
then runs a private test-only OpenXR API fixture. It verifies 110 stereo frames,
different left/right eye images, changing head/view images and controller shot
pose overrides. Its readbacks occur in the test fixture only. It does not
connect to a physical headset or change the installed runtime.

The DomeD test starts its own Xvfb, D-Bus session and nested compositor in a
private runtime directory. It verifies native ownership, focus-loss pause,
background restoration, Enter-to-resume and graceful exit. Its software
renderer is intentional isolation and is not GPU performance evidence.
Results and captures are under `artifacts/native-test/` and
`artifacts/domed-test/`. The build remains in this workspace; no active desktop
or display-manager session is restarted.

See [SOURCE_NOTICES.md](SOURCE_NOTICES.md) for provenance.
