# cmdescape

![Build](https://github.com/qr243vbi/cmdescape/workflows/Build/badge.svg)
[![GoDoc](https://img.shields.io/badge/go.dev-reference-007d9c?logo=go&logoColor=white&style=flat-square)](https://pkg.go.dev/github.com/qr243vbi/cmdescape?tab=overview)
[![sourcegraph](https://sourcegraph.com/github.com/alessio/shellescape/-/badge.svg)](https://sourcegraph.com/github.com/qr243vbi/cmdescape)
[![codecov](https://codecov.io/gh/alessio/shellescape/branch/master/graph/badge.svg)](https://codecov.io/gh/qr243vbi/cmdescape)
[![Go Report Card](https://goreportcard.com/badge/github.com/alessio/shellescape)](https://goreportcard.com/report/github.com/qr243vbi/cmdescape)

Escape arbitrary strings for safe use as command line arguments for Windows Console (cmd).

## Contents of the package

This package provides the `shellescape.Quote()` function that returns a
cmd-escaped copy of a string. This functionality could be helpful
in those cases where it is known that the output of a Go program will
be appended to/used in the context of command line programs' command line arguments.

This work was inspired by the Go original package
[shellescape](https://al.essio.dev/pkg/shellescape).

## Usage

The following snippet shows a typical unsafe idiom:

```go
package main

import (
	"fmt"
	"os"
)

func main() {
	fmt.Printf("ls -l %s\n", os.Args[1])
}

```

Especially when creating pipeline of commands which might end up being
executed by a shell interpreter, it is particularly unsafe to not
escape arguments.

`cmdescape.Quote()` comes in handy and to safely escape strings:

```go
package main

import (
        "fmt"
        "os"

        "github.com/qr243vbi/cmdescape"
)

func main() {
        fmt.Printf("ls -l %s\n", cmdescape.Quote(os.Args[1]))
}
```

## The escargs utility

__escargs__ reads lines from the standard input and prints shell-escaped versions. Unlike __xargs__, blank lines on the standard input are not discarded.
