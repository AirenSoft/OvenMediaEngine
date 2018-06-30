FROM ome_base:latest
#TODO: add ARGS(> v1.8)

WORKDIR /opt/ome/
RUN git clone https://github.com/AirenSoft/OvenMediaEngine.git
RUN cd OvenMediaEngine/src && make

# copy templeate file
WORKDIR /opt/ome/OvenMediaEngine
COPY ./app-launcher.sh /opt/ome/OvenMediaEngine

RUN cp -R ./docs/conf_examples ./src/bin/DEBUG/conf
RUN chmod +x /opt/ome/OvenMediaEngine/app-launcher.sh && \
    cp /opt/ome/OvenMediaEngine/app-launcher.sh ./src/bin/DEBUG/


EXPOSE 8080 8081 1935 3333 45050
VOLUME ["/opt/ome/OvenMediaEngine/src/bin/DEBUG/conf"]

WORKDIR /opt/ome/OvenMediaEngine/src/bin/DEBUG
CMD ["/opt/ome/OvenMediaEngine/src/bin/DEBUG/app-launcher.sh"]
