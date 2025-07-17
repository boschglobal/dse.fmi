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
		generate.NewFmiModelcCommand("gen-fmu"),
		generate.NewFmiMclCommand("gen-mcl"),
		generate.NewGenSignalGroupCommand("gen-signalgroup"),
		generate.NewGenFmuAnnotationCommand("gen-annotations"),
		generate.NewGenModelCFmuAnnotationCommand("gen-modelcfmu-annotations"),
		generate.NewFmiGatewayCommand("gen-gateway"),
		patch.NewPatchSignalGroupCommand("patch-signalgroup"),
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
