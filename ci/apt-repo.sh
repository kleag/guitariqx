#!/bin/sh
# Copyright (C) 2026 Gaël de Chalendar
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Build a signed apt repository from guitariqx .deb files.
# Usage: apt-repo.sh DEBDIR REPODIR URL
#
# Each package goes to the suite named after its version (0.3.4-1~bookworm
# goes to bookworm). The signing key must already be imported into gpg (it
# must be the only secret key); it is the key of the guitarix-kleag
# repository (https://kleag.github.io/guitarix/), so that both repositories
# can share one keyring file. Needs reprepro and dpkg-deb.

set -eu

debdir=$1
repo=$(realpath -m "$2")
url=$3

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# the release asset names have "." instead of "~": read the version instead
for deb in "$debdir"/*.deb; do
    version=$(dpkg-deb -f "$deb" Version)
    case "$version" in
    *~*) suite=${version##*~} ;;
    *) echo "$deb: no distribution in version $version" >&2; exit 1 ;;
    esac
    mkdir -p "$work/debs/$suite"
    cp "$deb" "$work/debs/$suite/"
done
test -d "$work/debs" || { echo "no .deb in $debdir" >&2; exit 1; }
suites=$(ls "$work/debs")

mkdir -p "$work/conf" "$repo"
for suite in $suites; do
    cat >> "$work/conf/distributions" <<CONF
Codename: $suite
Suite: $suite
Components: main
Architectures: amd64 arm64
Origin: kleag
Label: guitariqx
Description: guitariqx packages built from github.com/kleag/guitariqx
SignWith: default

CONF
done
for suite in $suites; do
    reprepro --confdir "$work/conf" --dbdir "$work/db" --outdir "$repo" \
        includedeb "$suite" "$work/debs/$suite"/*.deb
done

gpg --armor --export > "$repo/kleag-guitarix.asc"
cat > "$repo/index.html" <<HTML
<!doctype html>
<html><head><meta charset="utf-8"><title>guitariqx apt repository</title></head>
<body>
<h1>guitariqx apt repository</h1>
<p>Suites: $suites</p>
<pre>
sudo install -d /etc/apt/keyrings
sudo curl -fsSLo /etc/apt/keyrings/kleag-guitarix.asc $url/kleag-guitarix.asc
printf 'Types: deb\\nURIs: $url\\nSuites: %s\\nComponents: main\\nArchitectures: %s\\nSigned-By: /etc/apt/keyrings/kleag-guitarix.asc\\n' \\
  "\$(. /etc/os-release; echo \$VERSION_CODENAME)" "\$(dpkg --print-architecture)" | sudo tee /etc/apt/sources.list.d/guitariqx.sources
sudo apt update
sudo apt install guitariqx
</pre>
<p>See <a href="../install/">the installation page</a>.</p>
</body></html>
HTML
