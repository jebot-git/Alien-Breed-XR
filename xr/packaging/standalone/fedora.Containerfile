FROM docker.io/library/fedora:44
RUN dnf -y install gcc gcc-c++ cmake ninja-build pkgconf-pkg-config python3 SDL2-devel openxr-devel libglvnd-devel mesa-libGLU-devel libX11-devel glew-devel libpng-devel zlib-devel json-devel libopenmpt ffmpeg-free rpm-build file findutils diffutils tar gzip xz zstd && dnf clean all
RUN useradd -m -u 1000 builder && mkdir -p /work && chown 1000:1000 /work
USER 1000:1000
WORKDIR /work
