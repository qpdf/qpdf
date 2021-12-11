FROM ubuntu:18.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get -y install screen autoconf git sudo \
       build-essential zlib1g-dev libjpeg-dev libgnutls28-dev \
       python3-pip texlive-latex-extra latexmk \
       inkscape imagemagick busybox-static wget fuse && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
RUN pip3 install sphinx
COPY entrypoint /entrypoint
RUN chmod +x /entrypoint
ENTRYPOINT [ "/entrypoint" ]
