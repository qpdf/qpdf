FROM ubuntu:18.04 as start
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install screen git sudo \
    build-essential pkg-config \
    zlib1g-dev libjpeg-dev libgnutls28-dev \
    python3-pip texlive-latex-extra latexmk \
    inkscape imagemagick busybox-static wget fuse

# Until we move to ubuntu:20.04, we need a newer cmake. After 20.04,
# we can remove this and add cmake to the install above.
RUN apt-get -y install software-properties-common wget
RUN wget -O /etc/apt/trusted.gpg.d/kitware.asc \
    https://apt.kitware.com/keys/kitware-archive-latest.asc
RUN apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
RUN apt-get update
RUN apt-get -y install cmake
# End cmake

RUN apt-get clean && rm -rf /var/lib/apt/lists/*

RUN pip3 install sphinx sphinx_rtd_theme

FROM ubuntu:18.04 as run
COPY --from=start / /
COPY entrypoint /entrypoint
RUN chmod +x /entrypoint
ENTRYPOINT [ "/entrypoint" ]
