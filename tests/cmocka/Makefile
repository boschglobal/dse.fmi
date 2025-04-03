# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

export MAKE_NPROC ?= $(shell nproc)
GDB_CMD = valgrind -q --leak-check=full --track-origins=yes --error-exitcode=808
# GDB_CMD = gdb -q -ex='set confirm on' -ex=run -ex=quit

default: build

setup:
	mkdir build;
	cd build; cmake -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) ..

build:
# Build from scratch if no build dir.
	if [ ! -d "build" ]; then make setup; fi
# Build.
	cd build; cmake --build . -j $(MAKE_NPROC)
	cd build; cmake --build . -j $(MAKE_NPROC) -t install
	@echo ""
	@echo "Sandbox files: - $$(pwd)/build/_out"
	@echo "--------------"
	@find build/_out/ -type f -name '*' -exec ls -sh --color=auto {} \;

run:
	cd build/_out; $(GDB_CMD) bin/test_fmimcl
	cd build/_out; $(GDB_CMD) bin/test_mstep
	cd build/_out; $(GDB_CMD) bin/test_fmimodelc
	cd build/_out; $(GDB_CMD) bin/test_fmu
	cd build/_out; $(GDB_CMD) bin/test_fmi2fmu
	cd build/_out; $(GDB_CMD) bin/test_fmi3fmu
	cd build/_out; $(GDB_CMD) bin/test_fmigateway

clean:
	rm -rf build

cleanall: clean

.PHONY: default build run all clean cleanall
