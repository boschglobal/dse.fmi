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
DOC_LINKTITLE_fmimcl := MCL
DOC_TITLE_fmimcl := "FMI MCL API Reference"

# Module "fmimodelc"
DOC_INPUT_fmimodelc := dse/fmimodelc/fmimodelc.h
DOC_CDIR_fmimodelc := dse/fmimodelc/ascii85.c,dse/fmimodelc/fmi2fmu.c,dse/fmimodelc/runtime.c
DOC_OUTPUT_fmimodelc := doc/content/apis/fmi/fmimodelc/index.md
DOC_LINKTITLE_fmimodelc := "ModelC FMU"
DOC_TITLE_fmimodelc := "FMI ModelC FMU API Reference"

# Targets
DOC_C_MODULES := fmimcl fmimodelc


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
generate: index $(DOC_C_MODULES)
