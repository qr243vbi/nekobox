#!/usr/bin/env bash

git rm -rf --ignore-unmatch \
  .github \
  example \
  tests \
  *_test.go \
  **/*_test.go \
  *.md

go get -u
go mod tidy
