AB3D I sprite weapon reconstructions — editable prototype asset set

Six self-contained glTF 2.0 binary files (.glb): pistol, plasma, rocket,
flamethrower, grenade and shotgun. Open ab3di-sprite-weapons.blend in system
Blender to edit the models; overview.png is the Blender studio preview.

The XR runtime uses only the five original weapons. Legacy asset names map to:
pistol = Mark IV Pulse Rifle; shotgun = Double Barrel Shotgun;
plasma = Machine Gun; grenade = Grenade Launcher; rocket = Rocket Launcher.
The sixth flamethrower prototype remains archived in this editable source set,
but is neither staged nor loaded into the XR game. Slots and filenames are
retained for compatibility with the original data and existing saves.

Created in the installed DomeD Blender 5.4 Alpha using Blender MCP. These are
new low-poly reconstructions, not recovered high-detail original meshes.
Original first-person sprites supply every texture colour, the four appearance
states, flash cutouts and pose measurements. AB3D II models guide plausible
body shapes and part layouts for the five corresponding weapons. Their original
geometry is retained in a hidden reference-only Blender collection and excluded
from the GLBs. The flamethrower has no confirmed complete TKG counterpart.
Unseen surfaces and controller-scale dimensions are approximations. Some sprite
colours come from hands/gloves in the first-person artwork; unseen material
assignments are artistic interpretations, not newly discovered source detail.

Animation:
- Each GLB has a Fire clip. Timings follow the current C engine's 50 Hz gun
  countdown tables, including C zero-initialized trailing entries.
- Body motion is estimated from sprite metal-pixel centroids and principal
  axes. Foregrip travel and unseen mechanical details are authored estimates.
- Four UV variants switch via core glTF STEP scale channels, preserving original
  sprite appearance changes without a material animation extension. Only one
  body variant is visible at a time; source Blender uses tiny nonsingular scales
  while sampling, finalized GLBs use exactly zero/one visibility scales.
- Static parts are batched. Pump/foregrip geometry stays separately animated.
- Embedded pixel textures use nearest filtering. Original flash sprites use
  crossed alpha planes; opaque body geometry is fully three-dimensional.
- Neutral pose is the default when no animation plays. Fire clips last 0.06 s
  (pistol), 0.10 s (plasma/rocket/flamethrower), 0.24 s (grenade), 1.26 s (shotgun).

Integration convention:
Metres, grip origin at weapon root. Blender forward +Y exports to glTF -Z,
with glTF +Y up. Named grip and muzzle nodes also carry socket extras. Root
extras record the AB3D I weapon slot and reconstruction status. World/controller
tracking should drive the grip root; use the muzzle node for barrel effects.
The XR renderer loads these models into GPU buffers and attaches the selected
weapon to the right controller aim pose. Engine gun_frame drives the Fire clip;
shots use the model muzzle socket. Headset review is still needed for perceived
size, silhouette and hand alignment.

Files:
- sprites/: 24 original RGBA reference frames; slots 5/6 are intentionally omitted.
- textures/: four-frame sprite atlases, original-palette swatches, flash masks.
- reconstruction.json: source hashes, exact tick sequences, pose measurements,
  scale assumptions, and TKG reference geometry/hashes.
- build-report.json: mesh counts, stored and visible triangle counts.
- validation.json: Khronos glTF Validator 2.0.0-dev.3.10 results; all six files
  have zero errors and warnings. Informational entries describe marker nodes
  and original non-power-of-two sprite texture dimensions.
- roundtrip.json: system Blender re-import checks for geometry, UVs, sockets
  and mutually exclusive sprite states across sampled animation frames.

Rebuild from the repository root (Python preparation needs Pillow):
python3 xr/tools/prepare_weapon_reconstruction.py --data build-xr/data \
  --tkg <pinned-TKG-checkout>/media/vectobj

Then execute xr/tools/build_sprite_weapons_blender.py in a fresh system Blender
through Blender MCP. It creates its own scene and preserves an existing scene;
a duplicate reconstruction scene causes an error to protect hand edits.
The TKG directory is optional, and supplies reference geometry only.

Check exported timing and portability:
python3 xr/tools/check_weapon_assets.py
node xr/tools/validate_weapon_glb.cjs <absolute-path-to-gltf-validator-module>
The validator module is KhronosGroup/glTF-Validator's gltf-validator npm package;
it is a validation tool, not a game/runtime dependency.

See NOTICE.txt for provenance. No asset license is changed by this work.
