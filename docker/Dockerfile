# syntax=docker/dockerfile:1
#
# nd500x + NDIX in a container.
#
# Two stages:
#   builder  - compiles nd500x from this source tree (needs the git submodules
#              external/libdap and external/ndmonlib to be checked out already;
#              ndmonlib is a private repo, so the build MUST run from a local
#              working tree, not from a fresh public clone).
#   runtime  - Debian slim + the binary + the default NDIX disk image + the
#              handful of network tools the entrypoint needs.
#
# WHY DEBIAN AND NOT ALPINE
# -------------------------
# src/frontend/nd500x/uplink_tap.c includes <linux/if.h> together with the libc
# network headers. On musl that pair collides (duplicate struct ifreq/ifmap),
# which is a well known musl/kernel-header clash and would need source changes.
# glibc's headers guard against it. A ~70 MB base is a poor trade against
# patching the emulator to suit the container, so: Debian.
#
# BUILD CONTEXT IS THE REPOSITORY ROOT, not docker/:
#   docker build -f docker/Dockerfile -t ndix .
#
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------- builder ---
FROM debian:bookworm-slim AS builder

# cmake + a C toolchain, plus the two libraries nd500x links: readline (the
# debugger REPL) and cJSON (the debug/DAP JSON surface). ldd on a native build
# shows exactly these, which is where the runtime list further down comes from.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        pkg-config \
        libreadline-dev \
        libcjson-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy only what the build reads. .dockerignore at the repo root drops the
# build*/ trees, work/ and SCRATCH/ - without it the context is ~1 GB of
# already-compiled objects that would be uploaded on every build.
COPY CMakeLists.txt /src/
COPY src/      /src/src/
COPY include/  /src/include/
COPY external/ /src/external/

# Release build, no tests, no DAP. Tests need the test/ tree (68 MB of corpora)
# which we deliberately do not ship into the build context.
#
# ND500X_WITH_NDMON stays ON: NDIX itself only needs MON 600, but the native
# frontend (src/frontend/nd500x, i.e. the actual `nd500x` binary) is only built
# in the WITH_NDMON configuration - see CMakeLists.txt:233-243. Turning it off
# gives you libraries and no executable.
#
# The test/ tree is NOT in the build context - it is 68 MB of conformance
# corpora that no runtime image needs. CMakeLists.txt:262 does an unconditional
# add_subdirectory(test) though, so configure fails outright without it. An
# empty stub satisfies cmake and builds nothing, which is exactly what we want.
# (Editing CMakeLists.txt to make the test tree optional would be the tidier
# fix, but that is a change to a shared source file for the benefit of the
# container, and this stub costs one line.)
RUN mkdir -p /src/test && : > /src/test/CMakeLists.txt

RUN cmake -S /src -B /build \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIP_LIBDAP=ON \
    && cmake --build /build -j "$(nproc)" --target nd500x \
    && test -x /build/bin/nd500x

# ---------------------------------------------------------------- runtime ---
FROM debian:bookworm-slim AS runtime

# iproute2  - `ip tuntap add`, the bridge, addresses. busybox's ip has no
#             tuntap subcommand, verified: it prints its usage and exits 1.
# iptables  - the DNAT that lets a published container port reach the guest,
#             because the guest has its own IP that Docker knows nothing about.
# socat     - optional console-over-TCP relay (NDIX_CONSOLE_PORT).
# libreadline8 / libcjson1 / libtinfo6 - the shared libraries in `ldd nd500x`.
# iputils-ping / netcat - troubleshooting from inside the container.
RUN apt-get update && apt-get install -y --no-install-recommends \
        iproute2 \
        iptables \
        socat \
        libreadline8 \
        libcjson1 \
        libtinfo6 \
        iputils-ping \
        netcat-openbsd \
        procps \
        python3-minimal \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/bin/nd500x /opt/nd500x/bin/nd500x

# The baked-in disk image. NDIX_IMAGE_SRC must name a path INSIDE the build
# context; docker/build.sh copies the master image to docker/disk/ first,
# because the images live outside the repository and COPY cannot escape the
# context.
ARG NDIX_IMAGE_SRC=docker/disk/rootfs_net.img
COPY ${NDIX_IMAGE_SRC} /opt/nd500x/disk/rootfs.img

COPY docker/entrypoint.sh      /opt/nd500x/bin/entrypoint.sh
COPY docker/patch-rc.sh        /opt/nd500x/bin/patch-rc.sh
COPY docker/console-bridge.sh  /opt/nd500x/bin/console-bridge.sh
COPY docker/healthcheck.sh     /opt/nd500x/bin/healthcheck.sh
# The tool that writes a file back into an NDIX filesystem in place. It is what
# lets the container put the etconfig/ifconfig lines into the guest's /etc/rc,
# so the guest configures its own network at boot instead of being typed at.
COPY tools/ndix/ndixput.py     /opt/nd500x/bin/ndixput.py
RUN chmod +x /opt/nd500x/bin/entrypoint.sh \
             /opt/nd500x/bin/patch-rc.sh \
             /opt/nd500x/bin/console-bridge.sh \
             /opt/nd500x/bin/healthcheck.sh

# /data is where an external disk image is mounted. Empty by default, and the
# entrypoint falls back to the baked-in image when it finds nothing here.
RUN mkdir -p /data
VOLUME ["/data"]

# --- defaults -------------------------------------------------------------
# Every one of these is overridable with `docker run -e`. See docker/README.md
# for what each does and why.
ENV NDIX_ID=1 \
    NDIX_TAP_DEV=nd0 \
    NDIX_BRIDGE_DEV=ndbr0 \
    NDIX_AUTOCONF=1 \
    NDIX_FORWARD_PORTS=23 \
    NDIX_CONSOLE_PORT=2400 \
    NDIX_BOOT_TIMEOUT=300 \
    ND500X_DISK_RW=1 \
    ND500X_NOXMSG=0 \
    ND500X_ETH_UPLINK=tap

# 23 = the guest's telnetd (started by /etc/rc via inetd, already enabled in
#      /etc/inetd.conf - nothing to switch on inside the guest).
# 2400 = the emulator CONSOLE relayed over TCP, which works even when guest
#        networking does not. Not a real ND port number, just a free one.
EXPOSE 23 2400

# The guest takes ~45 s to reach a login prompt and the auto-config runs after
# that, so start-period has to be generous or Docker marks a healthy machine
# unhealthy while it is still booting. See trap 9 in the notes: everything on
# this machine is slow.
HEALTHCHECK --interval=15s --timeout=10s --start-period=240s --retries=3 \
    CMD /opt/nd500x/bin/healthcheck.sh

ENTRYPOINT ["/opt/nd500x/bin/entrypoint.sh"]

# --- OCI metadata ----------------------------------------------------------
# Filled in by docker/build.sh with the real values; the defaults here keep a
# plain `docker build` honest rather than claiming a revision it does not know.
ARG NDIX_VERSION=dev
ARG NDIX_REVISION=unknown
# LICENCE: the nd500x repository has NO LICENSE file at the time of writing, and
# the NDIX disk image is Norsk Data's 1988 operating system, whose redistribution
# terms are not settled here. "NOASSERTION" is the SPDX way of saying exactly
# that. Do NOT put a licence name in here until both questions have a real
# answer - a wrong licence label on a public image is worse than none.
ARG NDIX_LICENSE=NOASSERTION
LABEL org.opencontainers.image.title="NDIX on nd500x" \
      org.opencontainers.image.description="Norsk Data NDIX Release 3 (4.3BSD, 1988) running on the nd500x ND-500/5000 emulator, with networking." \
      org.opencontainers.image.source="https://github.com/HackerCorpLabs/nd500x" \
      org.opencontainers.image.licenses="${NDIX_LICENSE}" \
      org.opencontainers.image.version="${NDIX_VERSION}" \
      org.opencontainers.image.revision="${NDIX_REVISION}"
