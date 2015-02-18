#!/usr/bin/env bash

set -ex

[ -n "$1" ] || {
  echo >&2 "$0: [ERROR] clang-format missing. Aborting."
  exit 1
}

CLANG_FORMAT=$1

find ../src \( -name "*.h" -or -name "*.c" \) -print0 \
  | xargs -0 "$CLANG_FORMAT" -i
