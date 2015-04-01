#!/usr/bin/env bash

set -ex

sudo apt-get install -y wget tar

# Get libpfm 4.6.0
wget http://sourceforge.net/projects/perfmon2/files/libpfm4/libpfm-4.6.0.tar.gz

tar -xvf libpfm-4.6.0.tar.gz
cd libpfm-4.6.0 && make && sudo make install && cd ..
rm -rf libpfm-4.6.0 libpfm-4.6.0.tar.gz

# Get Jansson
wget http://www.digip.org/jansson/releases/jansson-2.7.tar.gz
tar -xvf jansson-2.7.tar.gz
cd jasson-2.7 && make && sudo make install && cd ..
rm -rf jansson-2.7 jansson-2.7.tar.gz

cd src/ && make
