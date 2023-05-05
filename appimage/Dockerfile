FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install screen git sudo \
    build-essential pkg-config cmake \
    zlib1g-dev libjpeg-dev libgnutls28-dev \
    python3-pip texlive-latex-extra latexmk \
    inkscape imagemagick busybox-static wget fuse && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
RUN pip3 install sphinx sphinx_rtd_theme
COPY entrypoint /entrypoint
RUN chmod +x /entrypoint
ENTRYPOINT [ "/entrypoint" ]
