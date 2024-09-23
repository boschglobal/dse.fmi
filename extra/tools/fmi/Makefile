# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


SRC           	 := $(shell find . -type f -name '*.go' -print)
BIN_DIR       	 := $(CURDIR)/bin
export BUILD_DIR := $(CURDIR)/build/stage
CLI_TOOLS     	 ?= $(BIN_DIR)/fmi
GOFLAGS       	 :=
GOPRIVATE     	 ?= github.com/boschglobal
export REPO_DIR  ?= $(shell readlink -e $(shell pwd -P)/../../..)


.PHONY: all
all: build

.PHONY: build
build: cmd

cmd: $(CLI_TOOLS)

$(CLI_TOOLS): $(SRC)
	go build $(GOFLAGS) -o $@ ./cmd/$(subst $(BIN_DIR)/,,$@)


.PHONY: test
test:
	go test ./...
	go test ./cmd/... -tags test_e2e

.PHONY: format
format:
	go fmt -x ./...

.PHONY: lint
lint:
	go fmt -x ./...
	docker run -t --rm \
		-v $$(pwd):/app \
		-v ~/.cache/golangci-lint/1.59.1:/root/.cache \
		-w /app \
		golangci/golangci-lint:latest \
		golangci-lint run -v ./...


.PHONY: clean
clean:
	@rm -rf '$(BIN_DIR)'
