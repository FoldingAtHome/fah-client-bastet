# AnyLinux AppImage

This directory builds a portable AppImage of `fah-client` using the
pkgforge-dev "AnyLinux" method (sharun + uruntime + DwarFS). The AppImage
bundles the C library and dynamic linker, so it runs on any Linux system,
including musl-based ones such as Alpine and systems with a very old glibc.

The build runs in an Arch Linux container. It clones and builds `cbang`, builds
`fah-client`, then bundles the binary. The finished AppImage is written to
`out/` in the repository root.

## Build

Run from the repository root:

    docker compose -f install/appimage/docker-compose.yml run --rm --build build

The result appears as `out/fah-client-<version>-<arch>.AppImage`.

The container detects your user id from the mounted repository, so the output
files belong to you and not to root.

## Notes

- `fah-client` is a background daemon. It does not open a window. Run the
  AppImage from a terminal; control the client through the web interface at
  <https://app.foldingathome.org/>.
- The AppImage bundles a whole C library, so it is larger than a normal
  package. That size is the cost of running on any Linux system.
- `cbang` is cloned inside the container only and does not touch your working
  copy. For a tagged release build, pass the version so the matching `cbang`
  tag is used:

      VERSION=v8.5.7 docker compose -f install/appimage/docker-compose.yml run --rm --build build
