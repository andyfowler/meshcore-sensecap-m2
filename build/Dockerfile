# OpenWrt 24.10 build environment for meshcore-m2
FROM debian:bookworm
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential clang flex bison g++ gawk gettext git libncurses-dev libssl-dev \
      python3 python3-setuptools python3-dev rsync swig unzip zlib1g-dev file wget curl \
      xz-utils zstd ca-certificates patch quilt time bzip2 less vim-tiny \
    && rm -rf /var/lib/apt/lists/*
RUN useradd -m -u 1000 build && mkdir -p /work && chown build:build /work
USER build
WORKDIR /work
