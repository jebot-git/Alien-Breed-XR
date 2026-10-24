# Combat feedback changes

The six requested changes are implemented in the PC and web paths:

1. `marine_hitscan_projectiles=0` in both the template and engine defaults.
   `objects.c` already separates the normal rifle's `marine_hitscan_burst`
   from the other ranks' attacks, so their green projectiles and shotgun
   behavior are preserved. Existing INI files with `=1` retain that preference;
   change it to `=0` to select hitscan.
2. The original `AB3DI.s` `hitcol` pulse now drives a translucent red overlay
   across the borderless view. Both players' timers fade independently in
   VBlank units, so batched ticks do not prolong the pulse. HUD draws afterward.
3. Missing keys use alpha 40 instead of 128; collected keys remain fully
   opaque and gain a bright underline, giving an additional ownership cue.
4. Vertical side bars replace the numeric health/ammo block. `EnergyBar` and
   `AmmoBar` in `amiga/AB3DI.s` provide the left/right placement, row-24 offset,
   top-down depletion, 127-health/63-ammo ranges and two rows per ammo unit.
   The original `healthstrip` and `ammostrip` four-plane silhouettes are decoded
   directly, using `ScreenSetup.s`'s `HealthPal` green/yellow/red ramp.
   Following the user's Amiga screenshot, the compact borderless bars occupy
   half the viewport height, use narrow strips at 38% opacity, and have a very
   faint empty track. They centre vertically and anchor to the sides for widescreen.
   The old framebuffer bar writes (before the world render) are removed.
   Options > HUD switches between BARS (default, `hud_bars=1`) and the previous
   NUMBERS display (`hud_bars=0`). The choice persists in the desktop INI or
   browser local storage, and loading a game keeps the current HUD preference.
   Small viewports use filtered bar textures (mipmaps on the GL path), a
   four-pixel minimum width, and upward-rounded fill heights. Non-zero health
   or ammo remains visible, and full values fill the track at every size.
5. `AB3DI.s` `oknothalf` / `nowaterfull` switch the CIA audio filter whenever
   `fillscrnwater` is nonzero. The port follows that same water state, independently
   of `post_tint`, using a 3 kHz two-pole Butterworth filter on the mixed output.
   A 20 ms wet/dry transition avoids abrupt switching. This is an audible
   software approximation, not an analog circuit simulation. Leaving gameplay
   clears the water target so menu/end music is not left muffled.
6. Keyboard and mouse/gamepad control share a movement acceleration helper
   that applies the selected 50-200% multiplier only while running, after the
   original `Plr1Control.s` integer speed/crouch calculation. Walking, friction,
   turning and the run/walk modifier retain their existing behavior. Options >
   Run Speed adjusts in 10-point steps; 100% is original speed and the default
   130% retains the requested PC boost. `run_speed` persists in the desktop INI
   or browser local storage, and loading a game keeps the current preference.

No replacement gameplay rules or placeholder effects were introduced.

Enemy attacks, idle vocals, pain/death cries and wall fire-spouts now share
`SoundPlayer.s` `MakeSomeNoise` distance attenuation instead of fixed mixer
gains. Original enemy handlers and `AlienControl.s` `FireAtPlayer1/2` supply
the sound strengths. Simultaneous sources of the same sample retain the
loudest contribution without restarting the burst. The active local player
determines the listener, including the slave player in multiplayer.

The level 8 walker was checked against the authored `level_h` geometry and
`Robot.s` movement dimensions. It can cross all four starting-room exits;
its movement and size were left unchanged because no blocked gap was identified.

A follow-up to the level 15 worm report found that targeting used the previous
tick's position/aim. `AB3DI.s` refreshes `CalcPLR1InLine` / `CalcPLR2InLine`
after player control, before `Anims.s` invokes `Player1Shot` / `Player2Shot`.
The port had refreshed only after object movement. The public shooting entry
points now refresh immediately before selecting a target. Turning/strafing onto
a worm can hit on that tick; turning/strafing away no longer damages a target
from the stale list. This also corrects controller projectile auto-aim when the
old cached distance was zero. Level 15 (`level_o/twolev.bin`) contains 13 worms
with authored health 80. Their original `HalfWorm.s` quarter-damage rule and
the previously requested run-speed boost are preserved.

Validation: Windows Release and single-threaded Emscripten Release builds;
focused movement and audio checks using the production C routines; browser
level smoke checks through both the OpenGL and SDL display paths, with no
browser errors. The visual harness exercises full/partial bars, collected and
missing keys, and the red hit overlay. Asset paths use the shipped `HealthStrip`
and `AmmoStrip` capitalization for case-sensitive web/Linux filesystems. Run:

```sh
python tools/check_feedback.py
# If needed, select Emscripten's Python entry point explicitly:
python tools/check_feedback.py --emcc /path/to/emscripten/emcc.py
```

The movement checks cover 1,920 combinations of control mode, always-run,
modifier, crouch, direction and tick batching. Audio checks cover desktop
8007 Hz and web 48000 Hz, dry bypass, low/high frequency response, silence,
and entering/leaving water.
HUD checks cover bottom alignment, monotonic fill, empty/full values and a
visible last health/ammo unit at small sizes. Browser pixel comparisons at
160x120, 320x200, 640x480 and 1280x720 pass through both GL and SDL paths.
Shooting checks cover 40 cases: both players, mouse/fixed-view pistol and
shotgun, and controller plasma auto-aim, each turning/strafing into and out
of a worm target. The turn-onto-target case failed before the cache fix
(zero damage, ammo consumed) and passes afterward.
