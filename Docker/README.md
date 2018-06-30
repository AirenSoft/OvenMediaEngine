# Docker Installation

## Ubuntu

    $ cd ${PROJECT_DIRECTORY}/Docker/ubuntu
    # ./build_images.sh

## CentOS

Work in progress

## Launch the application

Please note that the container is not supposed to run without the capability option(`NET_ADMIN`) since rtmp server is using `TCP_NODELAY` option.

    $ docker run -d --cap-add=NET_ADMIN ome_core:latest

## How to expose the ports and the volume

To expose the multie ports, you can run the container with the following option(`-p <host port>:<container port>`)

    $ docker run -d --cap-add=NET_ADMIN \
        -p 8080:8080 -p 8081:8081 \
        -p 1935:1935 -p 3333:3333 \
        -p 45050:45050 \
        ome_core:latest

You can also expose the volume for the configuration(`Server.xml`, etc...).

    $ docker run -d --cap-add=NET_ADMIN \
        -p 8080:8080 -p 8081:8081 \
        -p 1935:1935 -p 3333:3333 \
        -p 45050:45050 \
        -v <host directory>:/opt/ome/OvenMediaEngine/src/bin/DEBUG/conf
        ome_core:latest