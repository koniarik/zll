FROM alpine:3.21

RUN apk add --no-cache \
    cmake \
    ninja \
    gcc \
    g++ \
    gdb \
    python3 \
    musl-dev \
    linux-headers

WORKDIR /src
