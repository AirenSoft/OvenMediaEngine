#!/bin/bash

VERSION=1.1.0g

echo "Installing openssl($VERSION)...";

curl -OL https://www.openssl.org/source/openssl-$VERSION.tar.gz &&
  tar zxf openssl-$VERSION.tar.gz && cd openssl-$VERSION &&
  ./config shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa &&
  make && make install && cd -

if [ -f /usr/local/lib/libssl.so.1.1 ]; then
  echo "Installing openssl($VERSION) is completed.";
  # clean up the directories
  rm -rf ./openssl-$VERSION ./openssl-$VERSION.tar.gz
else
  echo "openssl($VERSION) installation failed.";
  # leave it to check it out later
  exit 1;
fi
