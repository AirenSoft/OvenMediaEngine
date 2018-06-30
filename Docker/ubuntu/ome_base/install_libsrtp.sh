#!/bin/bash

VERSION=2.2.0

echo "Installing libsrtp($VERSION)...";

curl -OL https://github.com/cisco/libsrtp/archive/v$VERSION.tar.gz &&
  tar zxf v$VERSION.tar.gz && cd libsrtp-$VERSION &&
  ./configure &&
  make && make install && cd -

if [ -f /usr/local/lib/libsrtp2.a ]; then
echo "Installing libsrtp($VERSION) is completed.";
  # clean up the directories
  rm -rf ./libsrtp-$VERSION ./v$VERSION.tar.gz
else
  echo "libsrtp($VERSION) installation failed";
  # leave it to check it out later
  exit 1;
fi
