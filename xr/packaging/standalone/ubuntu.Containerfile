FROM docker.io/library/ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config python3 libsdl2-dev libopenxr-dev libgl-dev libglu1-mesa-dev libegl-dev libx11-dev libglew-dev libpng-dev zlib1g-dev nlohmann-json3-dev libopenmpt0 ffmpeg dpkg-dev fakeroot xz-utils zstd file ca-certificates && rm -rf /var/lib/apt/lists/*
RUN useradd -m -u 1000 builder || true
RUN mkdir -p /work && chown 1000:1000 /work
USER 1000:1000
WORKDIR /work
