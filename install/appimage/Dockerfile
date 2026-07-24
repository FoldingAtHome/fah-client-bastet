# Arch base for building the AnyLinux AppImage of fah-client. See README.md.
FROM archlinux:latest

# DisableSandbox: pacman 7's download sandbox cannot run inside a container.
RUN sed -i '/^\[options\]/a DisableSandbox' /etc/pacman.conf && \
    pacman -Syu --noconfirm --needed \
      base-devel \
      bzip2 \
      git \
      lz4 \
      openssl \
      patchelf \
      scons \
      systemd-libs \
      tar \
      wget \
      zlib && \
    pacman -Scc --noconfirm

RUN useradd -m builder

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
