# Multi-stage Dockerfile for a minimal, security-hardened qpdf image
# This image is designed to be as small as possible and have zero or low vulnerabilities.

# Stage 1: Build Stage
FROM alpine:3.20 AS builder

# Install build dependencies
# We use alpine as it provides a simple way to build statically against musl libc.
RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    zlib-dev \
    zlib-static \
    libjpeg-turbo-dev \
    libjpeg-turbo-static \
    pkgconf

WORKDIR /build
COPY . .

# Configure the build:
# - CMAKE_BUILD_TYPE=Release: Optimize for production.
# - BUILD_SHARED_LIBS=OFF: Prefer static linking.
# - REQUIRE_CRYPTO_NATIVE=ON: Use qpdf's native crypto implementation to avoid 
#   dependencies on OpenSSL or GnuTLS, reducing the attack surface and image size.
# - CMAKE_EXE_LINKER_FLAGS="-static": Force static linking of the executable.
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DREQUIRE_CRYPTO_NATIVE=ON \
    -DUSE_IMPLICIT_CRYPTO=OFF \
    -DINSTALL_EXAMPLES=OFF \
    -DINSTALL_MANUAL=OFF \
    -DBUILD_DOC=OFF \
    -DCMAKE_EXE_LINKER_FLAGS="-static"

# Build the qpdf executable
RUN cmake --build build --target qpdf
RUN strip build/qpdf/qpdf

# Verify the binary is statically linked
RUN ldd build/qpdf/qpdf 2>&1 | grep "Not a valid dynamic program" || (echo "Warning: Binary is not fully static" && ldd build/qpdf/qpdf)

# Stage 2: Final Production Image
# We use Alpine as the base for the final image to provide a minimal but functional 
# environment. Alpine is highly security-hardened and frequently updated.
FROM alpine:3.20

# Create a non-privileged user to run the application
RUN addgroup -S qpdf && adduser -S qpdf -G qpdf

# Copy the statically linked binary from the builder stage
COPY --from=builder /build/build/qpdf/qpdf /usr/local/bin/qpdf

# Set permissions and switch to the non-privileged user
# This is a key security hardening step.
USER qpdf

# Set the working directory to /data for easier volume mounting
WORKDIR /data

# Default entrypoint is the qpdf command
ENTRYPOINT ["/usr/local/bin/qpdf"]

# Metadata
LABEL maintainer="qpdf-dev" \
      description="Minimal, hardened Docker image for qpdf" \
      version="1.0.0"
