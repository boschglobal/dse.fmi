// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"log/slog"
	"os"

	"github.com/boschglobal/dse.fmi/extra/tools/fmi/internal/app/generate"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/patch"
)

var (
	cmds = []CommandRunner{
		NewHelpCommand("help"),
		// ModelC FMU
		generate.NewModelCFmuCommand("gen-modelcfmu"),
		generate.NewGenModelCFmuAnnotationCommand("gen-modelcfmu-annotations"),
		// FMI MCL
		generate.NewFmiMclCommand("gen-fmimcl"),
		generate.NewGenSignalGroupCommand("gen-signalgroup"),
		// Gateway FMU
		generate.NewFmiGatewayCommand("gen-gatewayfmu"),
		// Common
		patch.NewPatchSignalGroupCommand("patch-signalgroup"),
		generate.NewGenFmuAnnotationCommand("gen-annotations"),
	}
)

func main() {
	os.Exit(main_())
}

func main_() int {
	flag.Usage = PrintUsage
	if len(os.Args) == 1 {
		PrintUsage()
		return 1
	}
	if err := DispatchCommand(os.Args[1]); err != nil {
		slog.Error(err.Error())
		return 2
	}

	return 0
}
