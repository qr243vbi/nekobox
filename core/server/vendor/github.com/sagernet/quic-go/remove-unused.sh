#!/usr/bin/env bash

function remove_unused() {
    git rm -rf --ignore-unmatch \
      .circleci .github docs *.yml \
      *_test.go **/*_test.go **/testdata testutils integrationtests \
      mock_*.go **/mock_*.go internal/mocks mockgen.go tools.go \
      example interop fuzzing metrics
}

remove_unused
remove_unused

go mod tidy
