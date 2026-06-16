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
	go test ./... -v

.PHONY: test_e2e
test_e2e:
	go test ./cmd/... -tags test_e2e -count=1

.PHONY: lint
lint:
	@docker run -t --rm \
		-v $$(pwd):/app \
		-w /app \
		golangci/golangci-lint:v2.12.2 \
			golangci-lint fmt ./...; \
			golangci-lint run --fix --disable errcheck ./...


.PHONY: clean
clean:
	@rm -rf '$(BIN_DIR)'
