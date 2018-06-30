#!/bin/bash

VERSION=1.7.0

echo "Installing libvpx($VERSION)...";

curl -OL https://chromium.googlesource.com/webm/libvpx/+archive/v$VERSION.tar.gz &&
  mkdir libvpx-$VERSION && cd libvpx-$VERSION && tar zxf ../v$VERSION.tar.gz &&
  ./configure --enable-shared --disable-static --disable-examples &&
  make && make install && cd -

if [ -f /usr/local/lib/libvpx.so ]; then
  echo "Installing libvpx($VERSION) is completed.";
  # clean up the directories
  rm -rf ./libvpx-$VERSION ./v$VERSION.tar.gz
else
  echo "libvpx($VERSION) installation failed.";
  # leave it to check it out later
  exit 1;
fi
