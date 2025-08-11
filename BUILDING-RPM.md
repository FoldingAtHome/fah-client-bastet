# Building the RPM package

## Install the Prerequisites

### Fedora/Red Hat Enterprise Linux (RHEL)
```
sudo dnf -y install git python3-scons gcc-c++ openssl-devel zlib-devel bzip2-devel lz4-devel systemd-devel rpm-build systemd-rpm-macros
```

Fedora 41+ also requires the ``openssl-devel-engine`` package.

On Red Hat Enterprise Linux (RHEL) and clones, you need to first enable the [EPEL repository](https://docs.fedoraproject.org/en-US/epel/getting-started/).

### openSUSE
```
sudo zypper -n in git scons gcc-c++ libopenssl-devel zlib-devel libbz2-devel liblz4-devel systemd-devel rpm-build systemd-rpm-macros
```

## Get the code
```
git clone https://github.com/cauldrondevelopmentllc/cbang
git clone https://github.com/foldingathome/fah-client-bastet
```

## Build a specific version (Optional)
To checkout the code for a specific version of the client run:

```
git -C cbang checkout bastet-v<version>
git -C fah-client-bastet checkout v<version>
```

Where ``<version>`` is a version number.

Run ``git -C fah-client-bastet tag`` to list available version numbers.

## Build the Folding@home Client
```
export CBANG_HOME=$PWD/cbang
scons -C cbang
scons -C fah-client-bastet
scons -C fah-client-bastet package
```

(on RHEL 8 and clones, replace `scons` with `scons-3`)

## Install the package
The last build step builds the RPM package. You can then install it like this:

### Fedora/Red Hat Enterprise Linux (RHEL)
```
sudo dnf install ./fah-client-bastet/fah_client-<version>-1.x86_64.rpm
```

### openSUSE
```
sudo zypper in ./fah-client-bastet/fah_client-<version>-1.x86_64.rpm
```

Where `<version>` is the software version number. On openSUSE, ignore ("i") when `zypper` complains the package is not signed.

Folding@home Client older than v8 will be automatically removed.
