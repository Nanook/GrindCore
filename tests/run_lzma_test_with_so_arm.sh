#!/bin/bash
# Usage: ./run_lzma_test_with_so_arm.sh

# !! BUILD ARM LIB BEFORE RUNNING

HOST_ARTIFACTS="/mnt/d/Src/NKitCode/GrindCore.build/.build/artifacts/bin/linux-arm/native"
DOCKER_ARTIFACTS="/workspaces/artifacts/bin/linux-arm/native"
TESTS_HOST="/mnt/d/Src/NKitCode/GrindCore/tests"
TESTS_DOCKER="/workspaces/src/native/tests"

# Ensure qemu-arm-static is available for ARM emulation
QEMU_ARM_STATIC="/usr/bin/qemu-arm-static"

docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

docker run --rm --privileged \
  -v "$HOST_ARTIFACTS":"$DOCKER_ARTIFACTS" \
  -v "$TESTS_HOST":"$TESTS_DOCKER" \
  -v $QEMU_ARM_STATIC:$QEMU_ARM_STATIC \
  --platform linux/arm/v7 \
  grindcore.build.linux.arm:latest \
  /bin/sh -c "\
    gcc -o $TESTS_DOCKER/lzma_test $TESTS_DOCKER/lzma_test.c \
      -I/workspaces/src/native/libs/GrindCore \
      -I/workspaces/src/native/external/sevenzip/lzma \
      -L$DOCKER_ARTIFACTS -l:libGrindCore.so -ldl -lpthread -lm && \
    LD_LIBRARY_PATH=$DOCKER_ARTIFACTS $TESTS_DOCKER/lzma_test"
