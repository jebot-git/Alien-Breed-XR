FROM docker.io/library/ubuntu:24.04
USER 0
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends cmake ninja-build pkg-config python3 nlohmann-json3-dev ffmpeg ca-certificates g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 && rm -rf /var/lib/apt/lists/*
RUN mkdir -p /work && chown 1000:1000 /work
USER 1000:1000
WORKDIR /work
