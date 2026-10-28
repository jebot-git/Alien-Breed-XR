FROM docker.io/library/archlinux:base-devel
RUN pacman -Syu --noconfirm cmake ninja python sdl2-compat openxr libglvnd glu libx11 glew libpng zlib nlohmann-json libopenmpt ffmpeg && pacman -Scc --noconfirm
RUN useradd -m -u 1000 builder && mkdir -p /work && chown 1000:1000 /work
USER 1000:1000
WORKDIR /work
