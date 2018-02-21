FROM ubuntu:14.04
RUN apt-get update && \
    apt-get -y install screen autoconf git \
       build-essential zlib1g-dev libjpeg-dev \
       docbook-xsl fop xsltproc \
       imagemagick wget fuse && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
COPY entrypoint /entrypoint
RUN chmod +x /entrypoint
ENTRYPOINT [ "/entrypoint" ]
