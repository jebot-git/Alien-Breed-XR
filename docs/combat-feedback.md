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
   that applies the requested 13/10 multiplier only while running, after the
   original `Plr1Control.s` integer speed/crouch calculation. Walking, friction,
   turning and the run/walk modifier retain their existing behavior.

No replacement gameplay rules or placeholder effects were introduced.

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
