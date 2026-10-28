# Source provenance

- Alien Breed 3D I, original Amiga source/data and credits: Team17 and the
  original authors. Preserved in the upstream `amiga/` tree with its notices.
- Native C port: retro-foundry/Alien-Breed-HD, baseline
  `61a13bc47a22bae885b44de2ed03b4ba1246d420`. Upstream author history and notices
  remain in this repository. Public source availability is not a new license
  grant; this branch does not relicense the game, art, sound or music.
- `xr/domed.*` and the starting `xr/xr.*` backend are adapted from the local
  de-xr Tempest-XR project at `7a49cf1f52a70116c44a784f559f3f9a2cec0292`.
  Game-specific rendering and action/controller code were added here. The
  helper retains its internal namespace for straightforward source comparison.
- `xr/tests/mock-openxr.cpp` starts from Tempest XR's test-only Comic Chat
  OpenXR fixture. Its MIT notice is reproduced in `tests/MOCK_RUNTIME_LICENSE.txt`.
  It is not a real runtime and is not intended for installation.
- SDL2, OpenGL/Mesa, GLU, EGL, X11, OpenXR, libcurl and nlohmann-json are system
  dependencies and retain their respective licenses. The build uses system
  SDL2 rather than downloading a separate SDL copy.
- The Alien Breed 3D XR launcher emblem in `assets/icons/` is newly generated
  port artwork authored with the built-in imagegen tool. Its transparent master,
  export instructions and full prompts are kept alongside the PNGs. It is not
  extracted original game artwork and does not change existing game licenses.

Standalone Windows releases bundle generic SDL2, OpenXR loader, GLEW and compiler
runtime DLLs from MSYS2. Their license notices are included under
`share/licenses/windows-libraries`. They contain no WiVRn-Pro or DomeD binaries.
