#!/usr/bin/env bash

set -e -o pipefail

GO_FILES=$(find . -name "*.go" | grep -v .git)

gofumpt -l -w $GO_FILES
gofmt -l -w $GO_FILES
gci write $GO_FILES
