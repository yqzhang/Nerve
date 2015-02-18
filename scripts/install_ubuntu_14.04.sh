#!/usr/bin/env bash

set -ex

if [ "$EUID" -ne 0 ]
then
  echo >&2 "$0: [ERROR] Sudo access required. Aborting."
  exit 1
fi

sudo apt-get install -y wget tar

# Get libpfm 4.6.0
wget http://sourceforge.net/projects/perfmon2/files/libpfm4/libpfm-4.6.0.tar.gz

tar -xvf libpfm-4.6.0.tar.gz
cd libpfm-4.6.0 && make && make install && cd ..
rm -rf libpfm-4.6.0 libpfm-4.6.0.tar.gz

cd src/ && make
