#!/bin/bash

BASE_IMAGE_VERSION=0.1
CORE_IMAGE_VERSION=0.1

# base image build
docker build -t ome_base:$BASE_IMAGE_VERSION ./ome_base
# update the latest version
docker tag ome_base:$BASE_IMAGE_VERSION ome_base:latest

# core build
docker build -t ome_core:$CORE_IMAGE_VERSION ./ome_core
# update the latest version
docker tag ome_core:$CORE_IMAGE_VERSION ome_core:latest