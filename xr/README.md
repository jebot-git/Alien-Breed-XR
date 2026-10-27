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
  head-oriented analog movement, smooth/snap turning, physical crouch, use,
  previous/next weapon buttons, pause and trigger haptic feedback.
  Controller position and orientation determine the firing origin/yaw/pitch;
  the player's locomotion pose is restored after the original shooting routine.
- The native XR input/background lease used by Tempest XR: validated DomeD
  launch records and Wayland peer, `/api/native-xr` ownership, heartbeat,
  focus/lock suspension, background restoration, and explicit resume. The game
  pauses when backgrounded and closes its XR session; it never restarts DomeD.
  The current SDL Wayland surface is resolved again before requesting ownership
  because SDL can remap it during graphics initialization.
- A head-locked stereo pause overlay with Resume, smooth/snap turn selection,
  Save, Load and Quit. The menu pauses gameplay without closing the XR session.
  Its HUD shows vitality percentage, current weapon ammunition and key-card
  ownership, reusing the original key-card sprites and the upstream port's
  bitmap font/digit atlases. Overlay assets are uploaded once and drawn on GPU.
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

F8 switches between XR and desktop. Left Menu or Escape toggles the pause
overlay; move either stick up/down to select, then press A/X or the trigger to
activate. The same Menu button closes the overlay. Keyboard arrows/Enter also
operate it. After background suspension, focus the game and press Enter to
reacquire the XR session. R recentres horizontal tracking while retaining the
standing-height calibration. Ctrl+Q quits.

In XR, the left stick moves relative to the horizontal head direction, with a
radial deadzone and continuous speed from walking to running. Full deflection
uses the original run-speed setting; diagonal input does not increase speed.
Physical lowering of the head continuously reduces the body collider within
the original standing/crouched height range. The view compensates for that body
height change so the camera does not crouch twice. Start the XR session standing
to calibrate height; authored low ceilings retain their original forced crouch.
The right stick turns (30-degree snap or smooth 90 degrees/second at full input).

Right trigger fires, **right A / left X use**, **left Y selects the previous
weapon**, and **right B selects the next weapon**. Only owned, selectable weapons
are cycled; button holds do not repeatedly cycle. Short use/weapon button presses
are latched until the original game logic consumes them. Touch Pro is explicitly
bound when `XR_FB_touch_controller_pro` is available, with the existing Touch
profile as fallback. Index uses B on the respective hands for previous/next and
left-stick click for the menu. Vive retains grip-to-use and its trackpad axes.
The simple-controller profile supports right select to fire, left select to use,
and Menu; it has no stick locomotion or Y/B weapon buttons. Controller pointers
remain simple 3D lines.

Sprite quads use the original centre height and class-specific dimensions, so
ground enemies meet their authored floor while flying enemies retain their
height. XR footsteps advance from accepted movement after collision, including
strafing; pushing a stationary player against a wall produces no footsteps.
Synthetic camera bob is disabled in XR and analog acceleration matches the
original engine's sine-table amplitude.

Both predicted eye positions are checked against the collision rooms and live
door/lift heights. Approaching a wall fades the world in both eyes; at 12.5 cm
clearance it becomes fully black, before the 5 cm near plane can expose the
other side. Head tracking and the HUD remain active with a move-back prompt.
Backing away restores the view. These checks do not activate wall switches,
produce collision sounds, or displace the physical head pose.

Save/Load uses the original full game/level-state format in
`$XDG_STATE_HOME/alien-breed-xr/savegame.bin` (default
`~/.local/state/alien-breed-xr/savegame.bin`). Save writes a temporary file and
renames it over the slot after successful completion. Loading restores the
original engine's pending full save through a level reload without reopening
the XR session. `ABXR_SAVE_DIR` can override the directory for isolated tests.

`--frames N`, `--report FILE.json` and `--capture FILE.ppm` are test hooks.
Production rendering has no framebuffer readback; capture is explicit.
`-DAB3D_XR=OFF` retains the upstream software-rendered executable.

## Limits of this milestone

This is a working source-port prototype, **not a completed VR conversion**.
Curved-wall records, polygon models, complete sprite frame/type fallbacks,
water/bump effects, lighting/material accuracy, sky, weapon models and the
original title/options menu presentation are not fully ported. Unknown/unavailable sprite
frames have coloured markers. Floor texture scale and wall UV fidelity need
further comparison with the original renderer. The original collision/AI still
uses its authored room/zone representation; physical leaning has no new head or
hand collision model. Shot origin changes are limited to a tracked controller
within 1.5 m and the player's current zone. Vertical aim retains the original
engine's signed aim-speed range (approximately ±45 degrees).

There is no hand interaction/grabbing, two-handed weapon model, save-format
migration or new FLTK settings window. Existing desktop settings remain in
use; XR game-state saves are kept in the user state directory. The pause
menu is controller-operated, while returning after compositor background
suspension still requires focusing the game and pressing Enter. Campaign completion and every weapon/enemy combination have not
been play-tested. All 16 level loading/render smoke checks are distinct from
full campaign compatibility. An initial Quest Pro/WiVRn headset session ran at 2160x2376 per eye and exposed
movement/control issues addressed by the following revision. The revised
controls, physical crouch and overlays have passed isolated tests but await
another physical-headset test. Comfort, complete tracking-loss recovery and
sustained performance at headset resolution remain unverified.

## Verification

```sh
python3 xr/tests/test_native.py
uv run --no-project --with python-xlib python xr/tests/test_domed.py
```

The native test uses offscreen EGL and the actual GPU, loads all 16 levels,
then runs a private test-only OpenXR API fixture. It verifies 110 stereo frames,
different left/right eye images, changing head/view images, head-directed
analog movement, physical crouch and recovery, previous/next weapon changes,
short A/X Use presses and controller shot pose overrides. A second scripted
run navigates the real XR pause overlay, changes turning mode, saves, loads
through a level reload and quits while retaining the same XR session. The
standalone locomotion CTest covers cardinal directions, rotated/recentered head
poses, analog speed, diagonal limits, pitch/roll and crouch-view compensation. Its readbacks occur in the test fixture only. It does not
connect to a physical headset or change the installed runtime.

The DomeD test starts its own Xvfb, D-Bus session and nested compositor in a
private runtime directory. It verifies native ownership, focus-loss pause,
background restoration, Enter-to-resume and graceful exit. Its software
renderer is intentional isolation and is not GPU performance evidence.
Results and captures are under `artifacts/native-test/` and
`artifacts/domed-test/`. The build remains in this workspace; no active desktop
or display-manager session is restarted.

`xr-collision-grounding` exercises solid walls, stationary eye clearance, open
portals, live door closure, floor/ceiling intrusion, grounded and flying sprite
metadata, and accepted-distance footstep timing. The GPU fixture also checks
head obstruction and recovery without restarting the XR session. The latest
grounding and collision changes still need physical headset verification.

See [SOURCE_NOTICES.md](SOURCE_NOTICES.md) for provenance.


Controller profile paths follow the [Meta OpenXR binding documentation](https://developers.meta.com/vr/documentation/native/android/mobile-openxr-actions-actionsets-bindings/).
