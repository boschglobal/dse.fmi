# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


################
## DSE Projects.
export PACKAGE_ARCH ?= linux-amd64

DSE_CLIB_REPO ?= https://github.com/boschglobal/dse.clib
DSE_CLIB_VERSION ?= 1.0.44
export DSE_CLIB_URL ?= $(DSE_CLIB_REPO)/archive/refs/tags/v$(DSE_CLIB_VERSION).zip

DSE_MODELC_REPO ?= https://github.com/boschglobal/dse.modelc
DSE_MODELC_VERSION ?= 2.2.20
export DSE_MODELC_URL ?= $(DSE_MODELC_REPO)/archive/refs/tags/v$(DSE_MODELC_VERSION).zip
export DSE_MODELC_LIB_URL ?= $(DSE_MODELC_REPO)/releases/download/v$(DSE_MODELC_VERSION)/ModelC-$(DSE_MODELC_VERSION)-$(PACKAGE_ARCH).zip

DSE_NCODEC_REPO ?= https://github.com/boschglobal/dse.ncodec
DSE_NCODEC_VERSION ?= 1.2.0
export DSE_NCODEC_URL ?= $(DSE_NCODEC_REPO)/archive/refs/tags/v$(DSE_NCODEC_VERSION).zip


###############
## Docker Images.
GCC_BUILDER_IMAGE ?= ghcr.io/boschglobal/dse-gcc-builder:latest
TESTSCRIPT_IMAGE ?= ghcr.io/boschglobal/dse-testscript:latest
SIMER_IMAGE ?= ghcr.io/boschglobal/dse-simer:$(DSE_MODELC_VERSION)
DSE_CLANG_FORMAT_IMAGE ?= ghcr.io/boschglobal/dse-clang-format:latest


###############
## Tools (Container Images).
TOOL_DIRS = fmi


###############
## Build parameters.
export NAMESPACE = dse
export MODULE = fmi
export EXTERNAL_BUILD_DIR ?= /tmp/$(NAMESPACE).$(MODULE)
export PACKAGE_ARCH_LIST ?= $(PACKAGE_ARCH)
export CMAKE_TOOLCHAIN_FILE ?= $(shell pwd -P)/extra/cmake/$(PACKAGE_ARCH).cmake
export MODELC_SANDBOX_DIR ?= $(shell pwd -P)/dse/modelc/build/_out
export MAKE_NPROC ?= $(shell nproc)
SUBDIRS = $(NAMESPACE)


###############
## Package parameters.
export PACKAGE_VERSION ?= 0.0.1
DIST_DIR := $(shell pwd -P)/$(NAMESPACE)/build/_dist
OSS_DIR = $(NAMESPACE)/__oss__
PACKAGE_DOC_NAME = DSE FMI Library
PACKAGE_NAME = Fmi
PACKAGE_NAME_LC = fmi
PACKAGE_PATH = $(NAMESPACE)/dist


###############
## Test Parameters.
export HOST_DOCKER_WORKSPACE ?= $(shell pwd -P)
export TESTSCRIPT_E2E_DIR ?= tests/testscript/e2e
TESTSCRIPT_E2E_FILES = $(wildcard $(TESTSCRIPT_E2E_DIR)/*.txtar)



ifneq ($(CI), true)
DOCKER_BUILDER_CMD := \
	mkdir -p $(EXTERNAL_BUILD_DIR); \
	docker run -it --rm \
		--user $$(id -u):$$(id -g) \
		--env CMAKE_TOOLCHAIN_FILE=/tmp/repo/extra/cmake/$(PACKAGE_ARCH).cmake \
		--env EXTERNAL_BUILD_DIR=$(EXTERNAL_BUILD_DIR) \
		--env GDB_CMD="$(GDB_CMD)" \
		--env PACKAGE_ARCH=$(PACKAGE_ARCH) \
		--env PACKAGE_VERSION=$(PACKAGE_VERSION) \
		--env MAKE_NPROC=$(MAKE_NPROC) \
		--volume $$(pwd):/tmp/repo \
		--volume $(EXTERNAL_BUILD_DIR):$(EXTERNAL_BUILD_DIR) \
		--workdir /tmp/repo \
		$(GCC_BUILDER_IMAGE)
endif

DSE_CLANG_FORMAT_CMD := docker run -it --rm \
	--user $$(id -u):$$(id -g) \
	--volume $$(pwd):/tmp/code \
	${DSE_CLANG_FORMAT_IMAGE}


default: build

.PHONY: build
build:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-build

.PHONY: package
package:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-package

.PHONY: test_tools
test_tools:
	for d in $(TOOL_DIRS) ;\
	do \
		cd extra/tools/$$d ;\
		make ;\
		make test ;\
	done;

.PHONY: fmi
fmi:
	mkdir -p extra/tools/fmi/build/stage/package/linux-amd64
	mkdir -p extra/tools/fmi/build/stage/package/linux-x86
	mkdir -p extra/tools/fmi/build/stage/package/linux-i386
	@if [ ${PACKAGE_ARCH} = "linux-amd64" ]; then \
		cp -r dse/build/_out/fmimodelc extra/tools/fmi/build/stage/package/linux-amd64 ;\
		cp -r dse/build/_out/examples extra/tools/fmi/build/stage ;\
		cp -r licenses -t extra/tools/fmi/build/stage ;\
	fi
	@if [ ${PACKAGE_ARCH} = "linux-x86" ]; then \
		cp -r dse/build/_out/fmimodelc extra/tools/fmi/build/stage/package/linux-x86 ;\
	fi
	@if [ ${PACKAGE_ARCH} = "linux-i386" ]; then \
		cp -r dse/build/_out/fmimodelc extra/tools/fmi/build/stage/package/linux-i386 ;\
	fi

.PHONY: tools
tools:
	for d in $(TOOL_DIRS) ;\
	do \
		mkdir -p extra/tools/$$d/build/stage ;\
		docker build -f extra/tools/$$d/build/package/Dockerfile \
				--tag $$d:test extra/tools/$$d ;\
	done;

do-test_cmocka-build:
	$(MAKE) -C tests/cmocka build

do-test_cmocka-run:
	$(MAKE) -C tests/cmocka run

test_cmocka:
ifeq ($(PACKAGE_ARCH), linux-amd64)
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-build
	@${DOCKER_BUILDER_CMD} $(MAKE) do-test_cmocka-run
endif

.PHONY: test_e2e
test_e2e: do-test_testscript-e2e

do-test_testscript-e2e:
# Test debug;
#   Additional logging: add '-v' to Testscript command (e.g. $(TESTSCRIPT_IMAGE) -v \).
#   Retain work folder: add '-work' to Testscript command (e.g. $(TESTSCRIPT_IMAGE) -work \).
ifeq ($(PACKAGE_ARCH), linux-amd64)
	@-docker kill simer 2>/dev/null ; true
	@set -eu; for t in $(TESTSCRIPT_E2E_FILES) ;\
	do \
		export ENTRYWORKDIR=$$(mktemp -d) ;\
		echo "Running E2E Test: $$t (in $${ENTRYWORKDIR})" ;\
		docker run -i --rm \
			-e ENTRYHOSTDIR=$(HOST_DOCKER_WORKSPACE) \
			-e ENTRYWORKDIR=$${ENTRYWORKDIR} \
			-v /var/run/docker.sock:/var/run/docker.sock \
			-v $(HOST_DOCKER_WORKSPACE):/repo \
			-v $${ENTRYWORKDIR}:/workdir \
			$(TESTSCRIPT_IMAGE) \
				-e ENTRYHOSTDIR=$(HOST_DOCKER_WORKSPACE) \
				-e ENTRYWORKDIR=$${ENTRYWORKDIR} \
				-e REPODIR=/repo \
				-e WORKDIR=/workdir \
				-e SIMER=$(SIMER_IMAGE) \
				-e FMI_IMAGE=$(FMI_IMAGE) \
				-e FMI_TAG=$(FMI_TAG) \
				$$t ;\
	done;
endif

.PHONY: test
test: test_cmocka test_e2e

.PHONY: clean
clean:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-clean
	for d in $(TOOL_DIRS) ;\
	do \
		cd extra/tools/$$d ;\
		rm -rf build/stage ;\
		make clean ;\
	done;
	docker images -qf dangling=true | xargs -r docker rmi

.PHONY: cleanall
cleanall:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-cleanall
	docker ps --filter status=dead --filter status=exited -aq | xargs -r docker rm -v
	docker images -qf dangling=true | xargs -r docker rmi
	docker volume ls -qf dangling=true | xargs -r docker volume rm
	docker images -q */*/$(NAMESPACE)-fmi | xargs -r docker rmi

.PHONY: oss
oss:
	@${DOCKER_BUILDER_CMD} $(MAKE) do-oss

.PHONY: do-build
do-build:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d build ); done

.PHONY: do-package
do-package:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d package ); done

.PHONY: do-clean
do-clean:
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d clean ); done
	$(MAKE) -C tests/cmocka clean
	rm -rf $(OSS_DIR)
	rm -rf doc_
	rm -rvf *.zip
	rm -rvf *.log

.PHONY: do-cleanall
do-cleanall: do-clean
	@for d in $(SUBDIRS); do ($(MAKE) -C $$d cleanall ); done
	$(MAKE) -C tests/cmocka cleanall

.PHONY: do-oss
do-oss:
	$(MAKE) -C extra/external oss

.PHONY: generate
generate:
	$(MAKE) -C doc generate

.PHONY: format
format:
	@${DSE_CLANG_FORMAT_CMD} dse
	@${DSE_CLANG_FORMAT_CMD} tests/cmocka/

.PHONY: super-linter
super-linter:
	docker run --rm --volume $$(pwd):/tmp/lint \
		--env RUN_LOCAL=true \
		--env DEFAULT_BRANCH=main \
		--env IGNORE_GITIGNORED_FILES=true \
		--env FILTER_REGEX_EXCLUDE="(doc/content/.*|extra/tools/fmi/vendor/.*)" \
		--env VALIDATE_CPP=true \
		--env VALIDATE_DOCKERFILE=true \
		--env VALIDATE_MARKDOWN=true \
		--env VALIDATE_YAML=true \
		ghcr.io/super-linter/super-linter:slim-v8
