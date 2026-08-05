VERSION := $(shell git describe --tags --abbrev=0)
GITREV := $(shell git describe --tags --dirty)
BUILDTIME := $(shell date)
LDFLAGS := '-w -s -X "main.VERSION=$(VERSION)" -X "main.GITREV=$(GITREV)" -X "main.BUILDTIME=$(BUILDTIME)"'
.PHONY: all deb
.ONESHELL:
all:
	mkdir -p build
	go build -tags purego -ldflags=$(LDFLAGS) -o ./build/vspomogator
deb: all
	./buildscripts/builddeb.sh
