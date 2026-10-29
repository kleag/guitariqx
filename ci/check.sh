#!/bin/sh
# Build, test and package guitariqx on Debian, Ubuntu or Raspberry Pi OS.
# Used by the GitHub workflows; runs the same way locally in a container:
#
#   podman run --rm -v "$PWD:/src:Z" -w /src debian:bookworm ci/check.sh --deps
#
# --deps      install the build dependencies first (apt, as root)
# --no-tests  skip the tests
# --package   build the Debian package into dist/
#
# BUILD_DIR (default _ci/<codename>-<arch>) is where it builds.

set -eu

deps=no
tests=yes
package=no
for arg in "$@"; do
    case "$arg" in
    --deps) deps=yes ;;
    --no-tests) tests=no ;;
    --package) package=yes ;;
    *) echo "unknown option $arg" >&2; exit 2 ;;
    esac
done

if [ "$deps" = yes ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    # qml6-module-* and qt6-qpa-plugins: tst_ui runs the application
    # (offscreen); dpkg-dev and file: cpack's dpkg-shlibdeps
    apt-get install -y --no-install-recommends \
        cmake g++ make ninja-build dpkg-dev file ca-certificates \
        qt6-base-dev qt6-declarative-dev qt6-qpa-plugins \
        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
        qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
        fonts-dejavu-core
fi

. /etc/os-release
arch=$(dpkg --print-architecture)
build=${BUILD_DIR:-_ci/${VERSION_CODENAME:-unknown}-$arch}

cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=$([ "$tests" = yes ] && echo ON || echo OFF)
cmake --build "$build"

if [ "$tests" = yes ]; then
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
        ctest --test-dir "$build" --output-on-failure --parallel 4
fi

if [ "$package" = yes ]; then
    (cd "$build" && cpack -G DEB)
    mkdir -p dist
    cp "$build"/guitariqx_*.deb dist/
    ls -l dist
fi
