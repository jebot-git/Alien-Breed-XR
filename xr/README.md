# Alien Breed XR — native port attempt

This branch extends retro-foundry's native C translation of **Alien Breed 3D I**,
including its original Amiga source reference and data. It runs the translated
gameplay directly; it does not launch an emulator or render a flat game screen
inside a headset.

The upstream baseline is `61a13bc47a22bae885b44de2ed03b4ba1246d420` from
<https://github.com/retro-foundry/Alien-Breed-HD>. Original source and data remain
in `amiga/`; upstream documentation is retained at the repository root.

## Implemented

- Native OpenGL world rendering of the loaded level's walls, floor/roof polygons,
  original polygon props/robots and animated sprite billboards, with depth testing.
  Original +Z view depth is converted consistently to OpenXR/OpenGL -Z forward.
  Floor polygon
  topology is tessellated once and cached in GPU display lists; texture and
  sprite frames are decoded/uploaded on first use. Live wall/floor heights are
  reread so the original door/lift logic can update them.
  Authored wall texture offsets/masks/repeat scale and see-through cutouts are
  respected; original vector meshes and their materials are cached on the GPU.
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
  Reset Height / Floor, Save, Load and Quit. Smooth turning is the default.
  The menu pauses gameplay without closing the XR session.
  Its HUD shows vitality percentage, current weapon name/ammunition and key-card
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
The pause menu's Reset Height / Floor treats the current headset height as the
standing baseline, restoring the standing body while preserving its feet
position where ceiling clearance allows. It leaves horizontal position and
heading unchanged. Stand upright before activating it.
The right stick uses smooth turning at 90 degrees/second at full input by
default; the menu can switch to 30-degree snap turning.

Right trigger fires, **right A / left X use**, **left Y selects the previous
weapon**, and **right B selects the next weapon**. Only owned, selectable weapons
are cycled; button holds do not repeatedly cycle. Short use/weapon button presses
are latched until the original game logic consumes them. Touch Pro is explicitly
bound when `XR_FB_touch_controller_pro` is available, with the existing Touch
profile as fallback. Index uses B on the respective hands for previous/next and
left-stick click for the menu. Vive uses the right grip for Use and retains its trackpad axes.
The simple-controller profile supports right select to fire, left select to use,
and Menu; it has no stick locomotion or Y/B weapon buttons. Controller pointers
use the right aim pose for the selected glTF weapon; the left pointer remains
available when not holding the weapon. All five models and their embedded textures upload once
to GPU buffers. Original gun countdown ticks drive the Fire clip, discrete sprite
states and pump mechanisms. Models are loaded from executable-local
`data/weapons/*.glb`, staged by CMake; missing assets are reported as an error.
Controller shots originate at the named muzzle socket and are blocked when its
path crosses a wall. The source/editable assets are in `xr/assets/weapons/`.
The weapon projects a short, depth-tested laser (up to 1.25 m) from that same
socket and direction, clipped against nearby walls and floor/ceiling openings.
The socket follows the full animated hierarchy, including recoil and the
double-barrel breech. The five current models have solid inferred side/bottom
surfaces, recessed bores and original-palette pixel UV atlases, with painted
hand pixels excluded from weapon materials. Their editable Blender scene,
depth assumptions and source references are in `xr/assets/weapons/`.
It is a visual aiming guide, hidden in the pause menu or at a blocked muzzle.
Projectile launch follows the full tracked direction; grenade gravity still
curves the path after launch. Incoming damage shows the original brief red
hit flash beneath the readable HUD.
The loader supports this unskinned GLB asset set (embedded PNGs, TRS/matrix
nodes, opaque/blended materials, STEP/LINEAR animation), rather than every glTF
extension. Geometry and textures remain on the GPU during normal playback.

For optional two-hand aiming, hold the **left grip/squeeze near the weapon's
fore-end** (within 12 cm of its support socket). Touch/Touch Pro, Index and Vive
support this; the simple-controller profile remains one-handed. A small amber
ring indicates a nearby grip; green and a brief left-hand vibration confirm the
hold. Release the left grip to return to one-handed aiming. The pause menu's
**Two-hand Hold** option enables/disables this feature; it starts enabled.
The firing hand remains the position anchor and supplies roll, while the physical
line between the hands controls orientation. The model, laser and shot ray share
that pose. There is no added smoothing, aim assistance, damage, recoil, spread,
movement or fire-rate modifier. Tracking loss, excessive separation, switching
weapons or opening the menu releases the hold and requires releasing the grip
before grabbing again. The support socket stays in its rest pose so firing
animation does not pull the weapon's aim.

The XR roster is the original five weapons, cycled in this order: Mark IV Pulse
Rifle, Double Barrel Shotgun, Machine Gun, Grenade Launcher and Rocket Launcher.
The optional PC flamethrower and unused slots remain excluded even with an old
INI or save. Internal slot numbers and legacy asset filenames remain compatible:
pulse rifle is slot 0 / `pistol.glb`, shotgun 7 / `shotgun.glb`, machine gun
1 / `plasma.glb`, grenade launcher 4 / `grenade.glb`, and rocket launcher
2 / `rocket.glb`.

Sprite quads use the original centre height and class-specific dimensions, so
ground enemies meet their authored floor while flying enemies retain their
height. XR footsteps advance from accepted movement after collision, including
strafing, once per 64 world units (two metres, half the previous cadence); pushing a stationary player against a wall produces no footsteps.
Synthetic camera bob is disabled in XR and analog acceleration matches the
original engine's sine-table amplitude.

Both predicted eye positions are checked against the collision rooms and live
door/lift heights. Approaching a wall fades the world in both eyes; at 12.5 cm
clearance it becomes fully black, before the 5 cm near plane can expose the
other side. Head tracking and the HUD remain active with a move-back prompt.
Backing away restores the view. These checks do not activate wall switches,
produce collision sounds, or displace the physical head pose.
XR body movement uses a swept circular wall collider with sliding, while open
doors, room transitions, stairs and low ceilings retain the original rules.

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
Curved-wall records, complete sprite frame/type fallbacks,
water/bump effects, exact original lighting/material accuracy, sky and the
original title/options menu presentation are not fully ported. Unknown/unavailable sprite
frames have coloured markers. Floor texture scale and wall UV fidelity need
further comparison with the original renderer. The original collision/AI still
uses its authored room/zone representation; physical leaning has no new head or
hand collision model. Shot origin changes are limited to a tracked controller
within 1.5 m; unobstructed muzzles may extend into adjacent open rooms. Combat
retains the original hit volumes and distance-dependent hitscan accuracy rules.

There is no general world-object grabbing, rendered hand model, save-format
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
run checks two-hand grip, tracking-loss/out-of-reach release, re-grabbing and
unchanged Use/fire cadence. Another scripted run navigates the real XR pause
overlay, changes turning mode, resets height, toggles two-hand holding, saves, loads
through a level reload and quits while retaining the same XR session. The
standalone locomotion CTest covers cardinal directions, rotated/recentered head
poses, analog speed, diagonal limits, pitch/roll and crouch-view compensation. Its readbacks occur in the test fixture only. It does not
connect to a physical headset or change the installed runtime.
The `two-hand-aiming` CTest checks physical orientation, the fixed firing-hand
position, unchanged aligned aim, roll preservation and safe release for menu,
weapon, option and invalid-tracking changes. Physical comfort and socket placement
still need headset testing.

The DomeD test starts its own Xvfb, D-Bus session and nested compositor in a
private runtime directory. It verifies native ownership, focus-loss pause,
background restoration, Enter-to-resume and graceful exit. Its software
renderer is intentional isolation and is not GPU performance evidence.
Results and captures are under `artifacts/native-test/` and
`artifacts/domed-test/`. The build remains in this workspace; no active desktop
or display-manager session is restarted.

`xr-collision-grounding` exercises solid walls, stationary eye clearance, open
portals, live door closure, floor/ceiling intrusion, grounded and flying sprite
metadata, accepted-distance footstep timing, standing/low-ceiling height resets
and the five-weapon roster. The GPU fixture also checks
head obstruction and recovery without restarting the XR session. The latest
grounding and collision changes still need physical headset verification.

See [SOURCE_NOTICES.md](SOURCE_NOTICES.md) for provenance.


Controller profile paths follow the [Meta OpenXR binding documentation](https://developers.meta.com/vr/documentation/native/android/mobile-openxr-actions-actionsets-bindings/).

Additional CTest checks cover all five runtime weapons and every firing tick using the
actual offscreen GPU, with visible-pixel checks and captures under
`artifacts/weapon-gpu-test/`. Footstep audio tests verify original sample
durations and normal mixer consumption at 8007 and 48000 Hz. The stereo fixture
checks model uploads, weapon draw calls in both eyes and weapon switching.
The original-vector GPU check renders every frame of all ten original polygon
models with their real materials, checks visible textured pixels and captures
them under `artifacts/vector-gpu-test/`.

Combat uses the controller ray in the original engine's +Z/sine-cosine convention.
Player and enemy shot pools, impact particles and the separate explosion list
share the GPU billboard path. Player weapon checks use a two-PAL-VBlank clock
independent of 50Hz movement and headset presentation. See ORIGINAL_TIMING.txt
for source locations, the variable original cadence, the chosen reference pace,
and damage/rate tests. Reports distinguish actual_shots_fired from controller
aim updates and include simulation VBlanks, elapsed time and rendered effects.

See GAMEPLAY_REFERENCE.txt for the original gameplay recordings inspected,
coordinate/prop/collision/aiming corrections, checks and remaining parity work.
