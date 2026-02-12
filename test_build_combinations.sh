#!/bin/bash
# This script builds LHF based on common flag combinations of LHF.

set -o errexit
set -o nounset

SRC_ROOT_DIR=$(realpath "./")
BUILD_DIR=$(realpath "./build")

run_build_with_flags() {
	cmake_args=("${@}")

	if [ ${#cmake_args[@]} -eq 0 ]; then
		echo "@@@@ RUNNING BUILD: <no args>"
	else
		echo "@@@@ RUNNING BUILD: ${cmake_args[@]}"
	fi

	set -o xtrace

	mkdir -p "$BUILD_DIR"
	cd "$BUILD_DIR"
	rm -rf *
	cmake .. -DENABLE_TESTS=1 -DENABLE_EXAMPLES=0 "${cmake_args[@]}"
	make -j4
	ctest .
	cd "$SRC_ROOT_DIR"

	set +o xtrace
}

run_build_with_flags;

run_build_with_flags -DENABLE_PARALLEL=1;

run_build_with_flags -DENABLE_TBB=1;

run_build_with_flags -DENABLE_EVICTION=1;

run_build_with_flags -DENABLE_SERIALIZATION=1;

run_build_with_flags -DDISABLE_INTEGRITY_CHECKS=1;

run_build_with_flags -DENABLE_DEBUG=0;

run_build_with_flags -DENABLE_PERFORMANCE_METRICS=0 -DENABLE_DEBUG=0;

run_build_with_flags -DENABLE_PERFORMANCE_METRICS=1 -DENABLE_DEBUG=0;

run_build_with_flags \
	-DENABLE_PERFORMANCE_METRICS=0 \
	-DENABLE_DEBUG=0 \
	-DENABLE_PARALLEL=1 \
	-DENABLE_EVICTION=1 \
	-DDISABLE_INTEGRITY_CHECKS=1 \
	-DENABLE_SERIALIZATION=1;