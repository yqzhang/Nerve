#!/usr/bin/env bash

set -ex

hash wget 2>/dev/null || {
  echo >&2 "$0: [ERROR] wget is not installed. Aborting."
  exit 1
}

hash tar 2>/dev/null || {
  echo >&2 "$0: [ERROR] tar is not installed. Aborting."
  exit 1
}

if [ "$EUID" -ne 0 ]
then
  echo >&2 "$0: [ERROR] Sudo access required. Aborting."
  exit 1
fi

# Get libpfm 4.6.0
wget http://sourceforge.net/projects/perfmon2/files/libpfm4/libpfm-4.6.0.tar.gz

tar -xvf libpfm-4.6.0.tar.gz
cd libpfm-4.6.0 && make && make install && cd ..
rm -rf libpfm-4.6.0 libpfm-4.6.0.tar.gz
