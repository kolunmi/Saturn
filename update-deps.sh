#!/usr/bin/env bash

# This script is needed to upgrade ripgrep
# generate rg-cargo-sources.json

set -euox pipefail

LOCK_FILE_DIR=$(mktemp -d)
cleanup() {
  rm -r "$LOCK_FILE_DIR"
}
trap cleanup EXIT

podman run --rm -it \
  -v .:/tmp/build:Z \
  -v "$LOCK_FILE_DIR:$LOCK_FILE_DIR:Z" \
  --pull newer \
  docker.io/library/python:alpine \
  sh -c "apk add jq curl git --no-cache && mkdir -p /tmp/build/flatpak-builder-tools && \
  curl -o /tmp/build/flatpak-builder-tools/flatpak-cargo-generator.py https://raw.githubusercontent.com/flatpak/flatpak-builder-tools/refs/heads/master/cargo/flatpak-cargo-generator.py && \
  git clone https://github.com/BurntSushi/ripgrep && cd ripgrep && git checkout \$(jq -r '.modules[] | select(.name == \"ripgrep\") | .sources[] | select(type==\"object\") | .commit' /tmp/build/net.kolunmi.Saturn.json) && \
  pip install --root-user-action=ignore aiohttp toml tomlkit && \
  python3 /tmp/build/flatpak-builder-tools/flatpak-cargo-generator.py Cargo.lock -o /tmp/build/rg-cargo-sources.json"
