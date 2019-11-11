FROM ubuntu:16.04
RUN apt-get update && \
    apt-get -y install screen autoconf git sudo \
       build-essential zlib1g-dev libjpeg-dev libgnutls28-dev \
       docbook-xsl fop xsltproc \
       inkscape imagemagick busybox-static wget fuse && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
COPY entrypoint /entrypoint
RUN chmod +x /entrypoint
ENTRYPOINT [ "/entrypoint" ]
