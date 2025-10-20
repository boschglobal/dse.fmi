# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

###############
## Docker Images.
DSE_CDOCGEN_IMAGE ?= dse-cdocgen:latest
DSE_PLANTUML_IMAGE ?= dse-plantuml:latest


###############
## Document Modules

# File "_index.md"
define _INDEX_MD_FILE =
---
title: "DSE FMI API Reference"
linkTitle: "FMI"
weight: 400
---

## DSE FMI API Reference
endef
export _INDEX_MD_FILE
DOC_OUTPUT_index := doc/content/apis/fmi/_index.md

# Module "fmimcl"
DOC_INPUT_fmimcl := dse/fmimcl/fmimcl.h
DOC_CDIR_fmimcl := dse/fmimcl/engine.c,dse/fmimcl/fmimcl.c,dse/fmimcl/model.c,dse/fmimcl/parser.c
DOC_OUTPUT_fmimcl := doc/content/apis/fmi/fmimcl/index.md
DOC_LINKTITLE_fmimcl := "MCL"
DOC_TITLE_fmimcl := "FMI MCL API Reference"

# Module "fmimodelc"
DOC_INPUT_fmimodelc := dse/fmimodelc/fmimodelc.h
DOC_CDIR_fmimodelc := dse/fmu/fmi2fmu.c,dse/fmimodelc/runtime.c,dse/fmimodelc/signal.c
DOC_OUTPUT_fmimodelc := doc/content/apis/fmi/fmimodelc/index.md
DOC_LINKTITLE_fmimodelc := "ModelC FMU"
DOC_TITLE_fmimodelc := "FMI ModelC FMU API Reference"

# Module "fmu"
DOC_INPUT_fmu := dse/fmu/fmu.h
DOC_CDIR_fmu := dse/fmu/fmu.c,dse/fmu/signal.c,dse/fmu/fmi2fmu.c,dse/fmu/fmi2variable.c
DOC_OUTPUT_fmu := doc/content/apis/fmi/fmu/index.md
DOC_LINKTITLE_fmu := "FMU"
DOC_TITLE_fmu := "FMU API Reference"

# Module "fmu"
DOC_INPUT_fmigateway := dse/fmigateway/fmigateway.h
DOC_CDIR_fmigateway := dse/fmigateway/fmigateway.c,dse/fmigateway/parser.c,dse/fmigateway/index.c,dse/fmigateway/session.c,dse/fmigateway/session_win32.c
DOC_OUTPUT_fmigateway := doc/content/apis/fmi/fmigateway/index.md
DOC_LINKTITLE_fmigateway := "Gateway FMU"
DOC_TITLE_fmigateway := "FMI Gateway FMU API Reference"

# Targets
DOC_C_MODULES := fmimcl fmimodelc fmu fmigateway


.PHONY: examples
examples:
	cd ..;mkdir -p doc/content/apis/fmi/examples/fmu
	cd ..;cp dse/examples/fmu/counter/counter.c doc/content/apis/fmi/examples/fmu/fmu.c

.PHONY: index
index:
	@cd ..; mkdir -p $$(dirname $(DOC_OUTPUT_index))
	@cd ..; echo "$$_INDEX_MD_FILE" > $(DOC_OUTPUT_index)

.PHONY: $(DOC_C_MODULES)
$(DOC_C_MODULES):
	@cd ..; mkdir -p $$(dirname $(DOC_OUTPUT_$@))
	@cd ..; docker run -it --rm \
		--volume $$(pwd):/tmp/repo \
		--workdir /tmp/repo \
		$(DSE_CDOCGEN_IMAGE) \
			--input $(DOC_INPUT_$@) \
			--output $(DOC_OUTPUT_$@) \
			--cdir $(DOC_CDIR_$@) \
			--title $(DOC_TITLE_$@) \
			--linktitle $(DOC_LINKTITLE_$@)
	@cd ../$$(dirname $(DOC_OUTPUT_$@)); docker run -it --rm \
		--volume $$(pwd):/tmp/repo \
		--workdir /tmp/repo \
		$(DSE_PLANTUML_IMAGE) \
			-tpng  $$(basename $(DOC_OUTPUT_$@))

.PHONY: generate
generate: index examples $(DOC_C_MODULES)
