#!/bin/bash

VERSION=1.1.3

echo "Installing opus($VERSION)...";

curl -OL https://archive.mozilla.org/pub/opus/opus-$VERSION.tar.gz &&
  tar zxf opus-$VERSION.tar.gz && cd opus-$VERSION &&
  autoreconf -f -i &&
  ./configure --enable-shared --disable-static &&
  make && make install && cd -

if [ -f /usr/local/lib/libopus.so.0 ]; then
  echo "Installing opus($VERSION) is completed.";
  # clean up the directories
  rm -rf ./opus-$VERSION ./opus-$VERSION.tar.gz
else
  echo "opus($VERSION) installation failed";
  # leave it to check it out later
  exit 1;
fi
