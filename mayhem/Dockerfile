# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y cmake clang

## Add source code to the build stage.
ADD . /esp32_bluetooth_classic_sniffer
WORKDIR /esp32_bluetooth_classic_sniffer/host_stack/bluekitchen/src

## TODO: ADD YOUR BUILD INSTRUCTIONS HERE.
RUN mkdir build
WORKDIR build
RUN CC=clang CXX=clang++ cmake .. 
RUN make

# Package Stage
FROM --platform=linux/amd64 ubuntu:20.04

## TODO: Change <Path in Builder Stage>
COPY --from=builder /esp32_bluetooth_classic_sniffer/host_stack/bluekitchen/src/build/ad_parser /
