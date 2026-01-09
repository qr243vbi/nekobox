#!/usr/bin/env bash

git rm -rf --ignore-unmatch \
  .github \
  CHANGELOG \
  cmd \
  scripts \
  tests \
  internal/btesting \
  internal/guts_cli \
  internal/surgeon \
  *_test.go \
  **/*_test.go \
  *.md

go get -u
go mod tidy
