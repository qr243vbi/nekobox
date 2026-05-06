#!/usr/bin/env bash

set -e -o pipefail

function remove_unused() {
  git rm -rf --ignore-unmatch \
    .github \
    **/*_test.go \
    tstest/ \
    release/ \
    cmd/ \
    util/winutil/s4u/ \
    k8s-operator/ \
    ssh/ \
    wf/ \
    internal/tooldeps \
    gokrazy/ \
    ipn/lapitest \
    feature/taildrop \
    feature/condregister/maybe_taildrop.go
}

remove_unused
remove_unused

go mod tidy
git commit -a -m "Remove unused"
