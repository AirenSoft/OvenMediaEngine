#!/bin/bash

VERSION=3.4.2

echo "Installing ffmpeg($VERSION)...";

curl -OL https://www.ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz &&
  xz -d ffmpeg-$VERSION.tar.xz && tar xf ffmpeg-$VERSION.tar && cd ffmpeg-$VERSION &&
  ./configure \
    --enable-shared \
    --disable-static \
    --extra-libs=-ldl \
    --disable-ffplay \
    --enable-ffprobe \
    --disable-ffserver \
    --disable-avdevice \
    --disable-doc \
    --disable-symver \
    --disable-debug \
    --disable-indevs \
    --disable-outdevs \
    --disable-devices \
    --disable-hwaccels \
    --disable-encoders \
    --enable-zlib \
    --disable-filters \
    --disable-vaapi \
    --enable-libopus \
    --enable-libvpx \
    --enable-encoder=libvpx_vp8,libvpx_vp9,libopus \
    --disable-decoder=tiff \
    --enable-filter=aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb &&
    make && make install


if [ -f /usr/local/lib/libavcodec.so ] &&
  [ -f /usr/local/lib/libavfilter.so ] &&
  [ -f /usr/local/lib/libavformat.so ] &&
  [ -f /usr/local/lib/libavutil.so ] ; then
  echo "Installing ffmpeg($VERSION) is completed.";
  # clean up the directories
  rm -rf ./ffmpeg-$VERSION ffmpeg-$VERSION.tar
else
  echo "ffmpeg($VERSION) installation failed.";
  # leave it to check it out later
  exit 1;
fi
