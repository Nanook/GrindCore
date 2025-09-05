#!/bin/bash
# Usage: ./run_lzma_test_with_so_docker.sh

# !! BUILD LIB BEFORE RUNNING

HOST_ARTIFACTS="/mnt/d/Src/NKitCode/GrindCore.build/.build/artifacts/bin/linux-x64/native"
DOCKER_ARTIFACTS="/workspaces/artifacts/bin/linux-x64/native"
TESTS_HOST="/mnt/d/Src/NKitCode/GrindCore/tests"
TESTS_DOCKER="/workspaces/src/native/tests"

docker run --rm \
  -v "$HOST_ARTIFACTS":"$DOCKER_ARTIFACTS" \
  -v "$TESTS_HOST":"$TESTS_DOCKER" \
  grindcore.build.linux:latest \
  /bin/sh -c "\
    gcc -o $TESTS_DOCKER/lzma_test $TESTS_DOCKER/lzma_test.c \
      -I/workspaces/src/native/libs/GrindCore \
      -I/workspaces/src/native/external/sevenzip/lzma \
      -L$DOCKER_ARTIFACTS -l:libGrindCore.so -ldl -lpthread -lm && \
      echo Complete
#    LD_LIBRARY_PATH=$DOCKER_ARTIFACTS $TESTS_DOCKER/lzma_test"
