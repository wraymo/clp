FROM ubuntu:focal AS BASE

WORKDIR /root

RUN mkdir -p ./tools/scripts/lib_install
ADD ./tools/scripts/lib_install ./tools/scripts/lib_install

# NOTE: We can't use the `install-all.sh` script since we need to configure the compiler in between
# `install-prebuilt-packages.sh` and `install-packages-from-source.sh`.
RUN ./tools/scripts/lib_install/ubuntu-focal/install-prebuilt-packages.sh

# Set cpp, cc, and c++ to v10
RUN update-alternatives --set cc /usr/bin/gcc-10
RUN update-alternatives --set c++ /usr/bin/g++-10
RUN update-alternatives --set cpp /usr/bin/cpp-10

RUN ./tools/scripts/lib_install/ubuntu-focal/install-packages-from-source.sh

# Remove cached files
RUN apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Flatten the image
FROM scratch
COPY --from=BASE / /
