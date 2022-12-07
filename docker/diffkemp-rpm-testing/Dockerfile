# Container image for testing DiffKemp RPMs.
# The image contains dependencies and prepared kernel sources.
# Running a container installs DiffKemp from RPM and performs build-kernel + compare
#
# Usage (for Fedora 37):
#   docker build --build-arg REL=37 -t diffkemp-rpm-testing:f37 .
#
#   Testing RPM from Copr:
#     docker run diffkemp-rpm-testing:f37
#
#   Testing local RPM:
#     docker run -v <dir-with-rpm>:/rpm --env DIFFKEMP_RPM=/rpm/<rpm-file> diffkemp-rpm-testing:f37
#
ARG REL
FROM fedora:$REL
MAINTAINER Viktor Malik <vmalik@redhat.com>

RUN dnf install -y \
    autoconf \
    bc \
    bison \
    bzip2 \
    cpio \
    curl \
    diffutils \
    dnf-plugins-core \
    elfutils-libelf-devel \
    flex \
    gcc \
    gmp-devel \
    git \
    make \
    module-init-tools \
    openssl-devel \
    python \
    python-pip \
    xz
RUN dnf copr enable -y viktormalik/diffkemp
RUN git clone https://github.com/viktormalik/rhel-kernel-get.git && \
    pip3 install -r rhel-kernel-get/requirements.txt
RUN mkdir kernel
RUN /rhel-kernel-get/rhel-kernel-get.py 4.18.0-80.el8 --output-dir kernel --kabi
RUN /rhel-kernel-get/rhel-kernel-get.py 4.18.0-147.el8 --output-dir kernel --kabi
RUN mkdir snap
RUN echo __alloc_pages_nodemask > list

ENV DIFFKEMP_RPM=diffkemp
ENTRYPOINT dnf install -y $DIFFKEMP_RPM && \
    diffkemp build-kernel kernel/linux-4.18.0-80.el8 snap/80 list && \
    diffkemp build-kernel kernel/linux-4.18.0-147.el8 snap/147 list && \
    diffkemp compare snap/80 snap/147 --stdout --show-diff && \
    /bin/bash

