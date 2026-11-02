#!/usr/bin/env bash
# Pinned upstream release and GitHub release-asset SHA-256 digests, checked 2026-11-02.
XRAY_VERSION="26.10.31"
xray_verify_archive() {
  local archive="$1" asset="$2" digest
  case "$asset" in
    Xray-windows-64.zip) digest=2e04a9995a6f2ad1b28c0bb25c708ecb2695e43a1c9c7889c4b6ac2b48cf7a6a ;;
    Xray-windows-32.zip) digest=9ea3a5849463f155b2484f8f32a9e8fd2a5628886571579d0c75d7fd0d888881 ;;
    Xray-windows-arm64-v8a.zip) digest=db062e8a177facacc8ae53d4c9c0df4132b13fc3a89acda6f21d92d36633077e ;;
    Xray-linux-64.zip) digest=e4bd9119f50ae7c41658b1abd0679ae06f98bd864a2907ff33f2f2cf7081b214 ;;
    Xray-linux-32.zip) digest=4064a12c7712f28f0a85082be3564c088f7861f2220ddb5e4de409c91837511d ;;
    Xray-linux-arm64-v8a.zip) digest=1811c8f1da0c3b6dc707ebaf1c8390e987ba39859a8f0a7aadd8526157f2ec8d ;;
    Xray-linux-arm32-v7a.zip) digest=2387f55239ee7799fd67d5382e79f589c1da07a97d993e86b058f2290b8e456b ;;
    *) echo "No pinned Xray checksum for $asset" >&2; return 1 ;;
  esac
  printf '%s  %s\n' "$digest" "$archive" | sha256sum --check --status
}
