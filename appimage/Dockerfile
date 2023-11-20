FROM ubuntu:18.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get -y install screen git sudo \
    build-essential pkg-config \
    zlib1g-dev libjpeg-dev libgnutls28-dev \
    python3-pip texlive-latex-extra latexmk \
    inkscape imagemagick busybox-static wget fuse && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
# Get cmake from pypi. We need to keep Ubuntu 18.04 for a while longer
# since the glibc in Ubuntu 20.04 is too new (as of late 2023) for
# Amazon Linux 2 in Lambda and for some supported CentOS versions.
# When we are ready to update to 20.04 or newer, remove the version
# constraint on sphinx, and install the OS package for cmake.
RUN pip3 install --upgrade pip
RUN pip3 install sphinx==4 sphinx_rtd_theme cmake
COPY entrypoint /entrypoint
RUN chmod +x /entrypoint
ENTRYPOINT [ "/entrypoint" ]
