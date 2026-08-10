#!/usr/bin/env bash
#
# build.sh - build the NDIX container image.
#
# Two things this does that a bare `docker build` cannot:
#
#  1. Stages the disk image into the build context. The NDIX images live
#     outside this repository (they are ~71 MB of somebody else's 1988
#     operating system and have no business in git), and COPY cannot reach
#     outside the context. So the master is copied to docker/disk/ first.
#
#     THE MASTER IMAGE IS NEVER MODIFIED. This only ever reads it.
#
#  2. Stamps the OCI labels with the real version and git revision, so a
#     published image can be traced back to the commit that built it.
#
# Usage:
#   ./docker/build.sh                       # ronnya/ndix-c:dev
#   NDIX_MASTER_IMAGE=/path/rootfs.img ./docker/build.sh
#   TAG=1.0.0 ./docker/build.sh             # ronnya/ndix-c:1.0.0
#
set -euo pipefail

cd "$(dirname "$0")/.."          # repository root - the build context

# The published repository is docker.io/ronnya/ndix-c. Building under the same
# name locally means the image you tested is byte-for-byte the one you push.
IMAGE="${IMAGE:-ronnya/ndix-c}"
TAG="${TAG:-dev}"

# The default master is the image with the `ndix` network account added. The
# plain rootfs_full.img has root only, and root CANNOT log in over the network,
# so a container built from it would boot fine and be impossible to telnet into.
NDIX_MASTER_IMAGE="${NDIX_MASTER_IMAGE:-/mnt/e/Dev/Ronny/NDIX-C/rootfs_net.img}"

if [ ! -f "$NDIX_MASTER_IMAGE" ]; then
    echo "build.sh: no disk image at $NDIX_MASTER_IMAGE" >&2
    echo "          set NDIX_MASTER_IMAGE=/path/to/rootfs_net.img" >&2
    exit 1
fi

mkdir -p docker/disk
STAGED=docker/disk/rootfs_net.img

# Copy only when it differs, because it is 71 MB and this script gets run a lot.
if [ ! -f "$STAGED" ] || ! cmp -s "$NDIX_MASTER_IMAGE" "$STAGED"; then
    echo "build.sh: staging $NDIX_MASTER_IMAGE -> $STAGED"
    cp "$NDIX_MASTER_IMAGE" "$STAGED"
else
    echo "build.sh: $STAGED is already up to date"
fi

REVISION="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
if ! git diff --quiet 2>/dev/null; then
    REVISION="${REVISION}-dirty"
fi

echo "build.sh: building $IMAGE:$TAG (revision $REVISION)"
docker build \
    -f docker/Dockerfile \
    -t "$IMAGE:$TAG" \
    --build-arg NDIX_VERSION="$TAG" \
    --build-arg NDIX_REVISION="$REVISION" \
    --build-arg NDIX_IMAGE_SRC="$STAGED" \
    "$@" \
    .

echo
docker images --format '  {{.Repository}}:{{.Tag}}  {{.Size}}' "$IMAGE" | head -5
