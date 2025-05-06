# This should be based on as old of an Ubuntu LTS as possible while
# supporting the minimum C++ or other requirements.
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install screen git sudo \
    build-essential pkg-config cmake \
    zlib1g-dev libjpeg-dev libgnutls28-dev \
    python3-sphinx python3-sphinx-rtd-theme \
    libjs-sphinxdoc texlive texlive-latex-extra \
    latexmk tex-gyre imagemagick busybox-static \
    wget fuse && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
COPY --chmod=0755 entrypoint /entrypoint
ENTRYPOINT [ "/entrypoint" ]
