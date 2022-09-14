FROM    ubuntu:20.04 AS base

## Install libraries by package
ENV     DEBIAN_FRONTEND=noninteractive
RUN     apt-get update && apt-get install -y tzdata sudo curl

FROM    base AS build

WORKDIR /tmp

ARG     OME_VERSION=master
ARG 	STRIP=TRUE
ARG     GPU=FALSE

ENV     PREFIX=/opt/ovenmediaengine
ENV     TEMP_DIR=/tmp/ome

## Download OvenMediaEngine
RUN \
        mkdir -p ${TEMP_DIR} && \
        cd ${TEMP_DIR} && \
        curl -sLf https://github.com/AirenSoft/OvenMediaEngine/archive/${OME_VERSION}.tar.gz | tar -xz --strip-components=1

## Install dependencies
RUN \
        if [ "$GPU" = "TRUE" ] ; then \
                ${TEMP_DIR}/misc/install_nvidia_docker_image.sh ; \
                ${TEMP_DIR}/misc/prerequisites.sh  --enable-nvc ; \
        else \
                ${TEMP_DIR}/misc/prerequisites.sh ; \
        fi

## Build OvenMediaEngine
RUN \
        cd ${TEMP_DIR}/src && \
        make release -j$(nproc)

RUN \
        if [ "$STRIP" = "TRUE" ] ; then strip ${TEMP_DIR}/src/bin/RELEASE/OvenMediaEngine ; fi

## Make running environment
RUN \
        cd ${TEMP_DIR}/src && \
        mkdir -p ${PREFIX}/bin/origin_conf && \
        mkdir -p ${PREFIX}/bin/edge_conf && \
        cp ./bin/RELEASE/OvenMediaEngine ${PREFIX}/bin/ && \
        cp ../misc/conf_examples/Origin.xml ${PREFIX}/bin/origin_conf/Server.xml && \
        cp ../misc/conf_examples/Logger.xml ${PREFIX}/bin/origin_conf/Logger.xml && \
        cp ../misc/conf_examples/Edge.xml ${PREFIX}/bin/edge_conf/Server.xml && \
        cp ../misc/conf_examples/Logger.xml ${PREFIX}/bin/edge_conf/Logger.xml && \
        rm -rf ${DIR}

FROM	base AS release

WORKDIR         /opt/ovenmediaengine/bin
EXPOSE          80/tcp 8080/tcp 8090/tcp 1935/tcp 3333/tcp 3334/tcp 4000-4005/udp 10000-10010/udp 9000/tcp
COPY            --from=build /opt/ovenmediaengine /opt/ovenmediaengine
# Default run as Origin mode
CMD             ["/opt/ovenmediaengine/bin/OvenMediaEngine", "-c", "origin_conf"]
