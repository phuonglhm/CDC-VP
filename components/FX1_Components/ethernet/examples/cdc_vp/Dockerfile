# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.cdc_vp.Dockerfile
# Description: Linux toolchain for the RISC-V (CDC-VP) platform of the Ethernet VP.
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
#   docker build -t icsl-eth-vp -f virtual_platform/cdc_vp/Dockerfile virtual_platform/scripts
#   virtual_platform/cdc_vp/run_in_docker.sh      # build + full regression in the image
#
# Pinned versions: Ubuntu 24.04, SystemC 2.3.4 (CDC-VP's), the RISC-V GCC of
# the distribution; CDC-VP and Bremen riscv-vp commits are pinned by
# run_in_docker.sh.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates \
        libboost-dev \
        gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
    && rm -rf /var/lib/apt/lists/*

COPY install_systemc.sh /tmp/install_systemc.sh
RUN bash /tmp/install_systemc.sh /opt/systemc-2.3.4 && rm /tmp/install_systemc.sh
ENV SYSTEMC_HOME=/opt/systemc-2.3.4
ENV LD_LIBRARY_PATH=/opt/systemc-2.3.4/lib

WORKDIR /work
