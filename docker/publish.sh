#!/usr/bin/env bash
#
# publish.sh - build and push docker.io/ronnya/ndix-c.
#
#   ./docker/publish.sh 1.0.0              # build, tag 1.0.0 + latest, push
#   ./docker/publish.sh 1.0.0 --dry-run    # do everything except the push
#
# You must `docker login` first. This script never handles credentials.
#
# WHAT GETS TAGGED
#   ronnya/ndix-c:<version>   the release you name
#   ronnya/ndix-c:latest      moved to point at it
#   ronnya/ndix-c:git-<sha>   the exact nd500x commit it was built from, so a
#                             bug report can be tied to a source revision
#
# MULTI-ARCH - DOES NOT WORK TODAY
# --------------------------------
# MULTIARCH=1 builds linux/amd64 + linux/arm64 with buildx. The arm64 half
# FAILS at link time: under QEMU emulation, cmake's file(GLOB) in
# external/ndmonlib/CMakeLists.txt:13 returns 0 source files where amd64 finds
# 234, and a few hundred `undefined reference to mon_*` follow. The files are
# demonstrably present - a probe image running the identical COPY sequence globs
# the same directory and gets 234. Nobody has identified an actual arm64 source
# incompatibility, so this may well build on a NATIVE arm64 machine. Until
# somebody tries, amd64 is the only architecture that can honestly be published.
#
# Left in place for whoever has that machine. Note that buildx cannot load a
# multi-platform image into the local daemon, so this path PUSHES DIRECTLY -
# there is no "build it, test it, then push it" in one command.
#
set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE="${IMAGE:-ronnya/ndix-c}"
VERSION="${1:-}"
DRY_RUN=0
[ "${2:-}" = "--dry-run" ] && DRY_RUN=1

if [ -z "$VERSION" ]; then
    echo "usage: $0 <version> [--dry-run]      e.g. $0 1.0.0" >&2
    exit 1
fi

REVISION="$(git rev-parse HEAD)"
SHORT="$(git rev-parse --short HEAD)"

if ! git diff --quiet; then
    echo "publish.sh: the working tree is dirty. Publishing a build that cannot" >&2
    echo "            be reproduced from a commit is not worth the confusion." >&2
    echo "            Commit or stash first, or set ALLOW_DIRTY=1." >&2
    [ "${ALLOW_DIRTY:-0}" = "1" ] || exit 1
fi

# Same staging step as build.sh: the disk image lives outside the repository.
NDIX_MASTER_IMAGE="${NDIX_MASTER_IMAGE:-/mnt/e/Dev/Ronny/NDIX-C/rootfs_net.img}"
mkdir -p docker/disk
if [ ! -f docker/disk/rootfs_net.img ] || ! cmp -s "$NDIX_MASTER_IMAGE" docker/disk/rootfs_net.img; then
    cp "$NDIX_MASTER_IMAGE" docker/disk/rootfs_net.img
fi

COMMON_ARGS=(
    -f docker/Dockerfile
    --build-arg "NDIX_VERSION=$VERSION"
    --build-arg "NDIX_REVISION=$REVISION"
    --build-arg "NDIX_IMAGE_SRC=docker/disk/rootfs_net.img"
    -t "$IMAGE:$VERSION"
    -t "$IMAGE:latest"
    -t "$IMAGE:git-$SHORT"
)

if [ "${MULTIARCH:-0}" = "1" ]; then
    # One buildx builder, reused. `docker buildx create` fails if it exists,
    # which is not an error worth stopping for.
    docker buildx create --name ndixbuilder --use 2>/dev/null || docker buildx use ndixbuilder

    if [ "$DRY_RUN" = "1" ]; then
        echo "publish.sh: multi-arch dry run - building both platforms, not pushing"
        docker buildx build --platform linux/amd64,linux/arm64 "${COMMON_ARGS[@]}" .
    else
        docker buildx build --platform linux/amd64,linux/arm64 --push "${COMMON_ARGS[@]}" .
    fi
else
    docker build "${COMMON_ARGS[@]}" .

    if [ "$DRY_RUN" = "1" ]; then
        echo
        echo "publish.sh: dry run. To push:"
        echo "  docker push $IMAGE:$VERSION"
        echo "  docker push $IMAGE:latest"
        echo "  docker push $IMAGE:git-$SHORT"
        exit 0
    fi

    docker push "$IMAGE:$VERSION"
    docker push "$IMAGE:latest"
    docker push "$IMAGE:git-$SHORT"
fi

echo
echo "publish.sh: done. Remember the Docker Hub overview page is docker/DOCKERHUB.md -"
echo "            it is not uploaded by the push and has to be pasted in by hand."
