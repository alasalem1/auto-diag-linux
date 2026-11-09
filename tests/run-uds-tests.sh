#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/meta-auto-diag/recipes-apps/autodiag-ecu/files"
TEST_DIR="$ROOT_DIR/tests/uds"

BINARY="$(mktemp)"
trap 'rm -f "$BINARY"' EXIT

echo "===== COMPILING UDS UNIT TESTS ====="

gcc \
    -Wall -Wextra -Werror -std=c11 \
    -I "$SOURCE_DIR" \
    "$TEST_DIR/test_uds_dispatch.c" \
    "$SOURCE_DIR/uds_dispatch.c" \
    "$SOURCE_DIR/service_10.c" \
    "$SOURCE_DIR/service_11.c" \
    "$SOURCE_DIR/service_14.c" \
    "$SOURCE_DIR/service_19.c" \
    "$SOURCE_DIR/dtc_manager.c" \
    "$SOURCE_DIR/service_22.c" \
    "$SOURCE_DIR/did_manager.c" \
    "$SOURCE_DIR/service_2e.c" \
    "$SOURCE_DIR/service_27.c" \
    "$SOURCE_DIR/service_3e.c" \
    "$SOURCE_DIR/service_28.c" \
    "$SOURCE_DIR/service_85.c" \
    "$SOURCE_DIR/uds_security.c" \
    "$SOURCE_DIR/uds_session.c" \
    -o "$BINARY"

echo "===== EXECUTING UDS UNIT TESTS ====="
"$BINARY"
