AB3D I sprite weapon reconstructions — version 2

Five current self-contained glTF 2.0 binary files (.glb): pistol, plasma,
rocket, grenade and shotgun. Open ab3di-sprite-weapons.blend in system Blender
to edit these five models; overview.png is the Blender studio preview.
comparison.png compares original sprites, previous models and current native
GPU rendering from the same side view.

The XR runtime uses only the five original weapons. Legacy asset names map to:
pistol = Mark IV Pulse Rifle; shotgun = Double Barrel Shotgun;
plasma = Machine Gun; grenade = Grenade Launcher; rocket = Rocket Launcher.
The sixth flamethrower prototype remains archived in this editable source set,
but is neither staged nor loaded into the XR game. Slots and filenames are
retained for compatibility with the original data and existing saves.

Created in the installed DomeD Blender 5.4 Alpha using Blender MCP. These are
new solid reconstructions, not recovered original meshes. Original first-person
sprites supply silhouette measurements, every texture colour, appearance
states and flash cutouts. Explicit longitudinal depth sections and rounded or
chamfered cross-sections reconstruct volume; surfaces absent from the sprites
continue the receiver panels, venting, bores, grip ribs and mechanical joints.
AB3D II models guide plausible
body shapes and part layouts for the five corresponding weapons. Their original
geometry is retained in a hidden reference-only Blender collection and excluded
from the GLBs. Unseen surfaces and controller-scale dimensions are authored
inferences. The depth guides are explicit geometric assumptions, not recovered
measurements or neural predictions. Painted hand pixels are removed from weapon
surface sampling; brown gloves no longer become wood/fuel-coloured weapon parts.

Presentation:
- Actual recessed barrel bores/crowns, stepped receivers, inset side panels,
  cooling recesses, fasteners, sights, trigger guards and ribbed composite grips.
- Separate top, side, end, bore and underside UV islands. Top surfaces reuse
  deprojected sprite rows; inferred sides have coherent original-palette pixel
  shading, panel seams, wear, accents and industrial markings.
- 1024x256 atlases contain four 256x256 appearance states and sixteen material
  regions per state. Nearest filtering preserves deliberate coarse pixels.
- About 1,150–1,740 visible triangles per weapon, under the 2,500-triangle test
  budget. Static detail is batched and uploaded once; no runtime reconstruction
  or framebuffer readback is used.

Animation:
- Each GLB has a Fire clip. Timings follow the current C engine's 50 Hz gun
  countdown tables, including C zero-initialized trailing entries.
- Bounded recoil settles quickly. The grenade fore-end slides; the double
  barrel shotgun has a hinged breech rather than a pump. Its barrels close
  before the original cooldown allows the next shot. Travel is inferred.
- Four UV variants switch via core glTF STEP scale channels, preserving original
  sprite appearance changes without a material animation extension. Only one
  body variant is visible at a time; source Blender uses tiny nonsingular scales
  while sampling, finalized GLBs use exactly zero/one visibility scales.
- Moving mechanisms stay separately animated. The muzzle socket follows recoil
  and breech motion; native shots and the short laser use the full animated pose.
- Embedded pixel textures use nearest filtering. Original flash sprites use
  crossed alpha planes; opaque body geometry is fully three-dimensional.
- Neutral pose is the default when no animation plays. Fire clips last 0.06 s
  (pistol), 0.10 s (plasma/rocket), 0.24 s (grenade), 1.26 s (shotgun).

Integration convention:
Metres, grip origin at weapon root. Blender forward +Y exports to glTF -Z,
with glTF +Y up. Named grip and muzzle nodes also carry socket extras. Root
extras record the AB3D I weapon slot and reconstruction status. World/controller
tracking should drive the grip root; use the muzzle node for barrel effects.
Each current weapon also has a support_grip marker at its rest fore-end. The
optional left grip rotates the weapon about the firing-hand root; it does not
change damage, recoil, spread or firing timing. The support marker is outside
the Fire animation hierarchy so recoil cannot feed back into physical aiming.
The XR renderer loads these models into GPU buffers and attaches the selected
weapon to the right controller aim pose, adjusted by the left grip when held.
Engine gun_frame drives the Fire clip;
shots use the model muzzle socket. Headset review is still needed for perceived
size, silhouette and hand alignment.

Files:
- sprites/: 24 original RGBA reference frames; slots 5/6 are intentionally omitted.
- textures/: current surface atlases and depth guides, original sprite atlases
  and flash masks. Depth guides are modelling references, not runtime textures.
- surfaces.json: palette, hand exclusions, silhouette rows and inference notes.
- reconstruction.json: source hashes, exact tick sequences, pose measurements,
  scale assumptions, and TKG reference geometry/hashes.
- build-report.json: mesh counts, stored and visible triangle counts.
- support-grips.json: canonical fore-end support sockets in glTF metres; matching
  editable empties are included in the Blender source scene.
- validation.json: Khronos glTF Validator 2.0.0-dev.3.10 results; all six files
  have zero errors and warnings. Informational entries describe marker nodes
  and original non-power-of-two sprite texture dimensions.
- roundtrip.json: five current models re-imported in system Blender; UVs,
  closed body surfaces and mutually exclusive sprite states are checked.

Rebuild from the repository root (Python preparation needs Pillow):
python3 xr/tools/prepare_weapon_reconstruction.py --data build-xr/data \
  --tkg <pinned-TKG-checkout>/media/vectobj
python3 xr/tools/prepare_weapon_surfaces.py

Then execute xr/tools/build_sprite_weapons_blender.py in a fresh system Blender
through Blender MCP. It creates its own scene and preserves an existing scene;
a duplicate reconstruction scene causes an error to protect hand edits.
The TKG directory is optional, and supplies reference geometry only. The builder
imports weapon_geometry.py for the current five solid reconstructions. The old
flamethrower GLB remains an archived prototype and is not rebuilt or staged.
To update only support sockets in existing GLBs without re-exporting geometry or
animation, run python3 xr/tools/set_weapon_support_grips.py. Update the matching
Blender empties too when changing these authored points.

Check exported timing and portability:
python3 xr/tools/check_weapon_assets.py
Execute xr/tools/check_weapon_blender.py in system Blender for round-trip QA.
node xr/tools/validate_weapon_glb.cjs <absolute-path-to-gltf-validator-module>
The validator module is KhronosGroup/glTF-Validator's gltf-validator npm package;
it is a validation tool, not a game/runtime dependency.

See NOTICE.txt for provenance. No asset license is changed by this work.
