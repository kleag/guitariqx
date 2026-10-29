# Installation

guitariqx needs a guitarix engine to talk to: install guitarix on the
computer attached to your audio interface (`sudo apt install guitarix` on
Debian, Ubuntu and Raspberry Pi OS). guitariqx itself can run on that
computer or on another one.

## Debian packages

Each [release](https://github.com/kleag/guitariqx/releases) has packages
for Debian 12 "bookworm" and Debian 13 "trixie", on x86_64 (`amd64`) and on
64 bit ARM (`arm64`):

| system | package |
|---|---|
| Raspberry Pi OS Bookworm, 64 bit (Pi 4, Pi 5) | `guitariqx_<version>-1.bookworm_arm64.deb` |
| Raspberry Pi OS Trixie, 64 bit | `guitariqx_<version>-1.trixie_arm64.deb` |
| Debian 12 | `guitariqx_<version>-1.bookworm_amd64.deb` |
| Debian 13 | `guitariqx_<version>-1.trixie_amd64.deb` |

On other distributions, such as Ubuntu, [build it from source](#from-source):
the packages are only tested on Debian.

Install the package with apt, which also installs the Qt modules it needs:

```sh
sudo apt install ./guitariqx_0.3.4-1.bookworm_arm64.deb
```

`SHA256SUMS` in the release lists the checksums of the files.

## From source

guitariqx needs Qt 6.4 or newer and CMake 3.21 or newer.

On Debian, Ubuntu or Raspberry Pi OS (Bookworm or newer):

```sh
sudo apt install cmake g++ qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
    qt6-wayland
git clone https://github.com/kleag/guitariqx.git
cd guitariqx
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build    # optional, installs /usr/local/bin/guitariqx
```

`qt6-wayland` is only needed at run time, on Wayland desktops (e.g. Raspberry
Pi OS Bookworm on a Pi 5); without it Qt falls back to X11 or shows no window.

The build includes the tests, which need the Qt Test module (part of
`qt6-base-dev`); `-DBUILD_TESTING=OFF` leaves them out. See
[Development](development.md) to run them.

To make your own Debian package of the build:

```sh
cd build && cpack -G DEB
```
