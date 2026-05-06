
#!/usr/bin/make -f

VERSION := $(shell git describe)

all: build

build-stamp:
	go build -a -v
	touch $@
build: build-stamp

install-stamp: build
	go install -v \
            -ldflags="X 'main.version=$(VERSION)'" \
            ./cmd/escargs
	touch $@
install: install-stamp

escargs: build
	go build -v \
            -ldflags="-X 'main.version=$(VERSION)'" \
            ./cmd/escargs

clean:
	rm -f escargs

distclean: clean
	rm -f build-stamp install-stamp

uninstall:
	rm -fv $(shell go env GOPATH)/bin/escargs

.PHONY: clean distclean install uninstall 
