// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"log/slog"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/log"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type GenSignalGroupCommand struct {
	commandName string
	fs          *flag.FlagSet

	inputFile  string
	outputFile string
	logLevel   int
}

func NewGenSignalGroupCommand(name string) *GenSignalGroupCommand {
	c := &GenSignalGroupCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.inputFile, "input", "", "path to FMU Model Description file (XML)")
	c.fs.StringVar(&c.outputFile, "output", "", "path to write generated signal group file")
	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")
	return c
}

func (c GenSignalGroupCommand) Name() string {
	return c.commandName
}

func (c GenSignalGroupCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *GenSignalGroupCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *GenSignalGroupCommand) Run() error {
	slog.SetDefault(log.NewLogger(c.logLevel))

	fmt.Fprintf(flag.CommandLine.Output(), "Reading file: %s\n", c.inputFile)
	h := fmi2.XmlFmuHandler{}
	fmiMD := h.Detect(c.inputFile)
	if err := c.generateSignalVector(*fmiMD); err != nil {
		return err
	}
	return nil
}

func (c *GenSignalGroupCommand) generateSignalVector(fmiMD fmi2.FmiModelDescription) error {
	// Build the SignalGroup.
	signalGroup := kind.SignalGroup{
		Kind: "SignalGroup",
		Metadata: &kind.ObjectMetadata{
			Name: stringPtr(fmiMD.ModelName),
			Labels: &kind.Labels{
				"channel": "signal_vector",
				"model":   fmiMD.ModelName,
			},
		},
	}
	signals := []kind.Signal{}
	for _, s := range fmiMD.ModelVariables.ScalarVariable {
		switch s.Causality {
		case "parameter":
			continue
		case "":
			s.Causality = "local"
		default:
		}

		var variable_type string
		if s.Real != nil {
			variable_type = "Real"
		} else if s.Integer != nil {
			variable_type = "Integer"
		} else if s.String != nil {
			variable_type = "String"
		} else if s.Boolean != nil {
			variable_type = "Boolean"
		}

		//annotations := map[string]interface{}{
		annotations := kind.Annotations{
			"fmi_variable_causality": s.Causality,
			"fmi_variable_id":        s.ValueReference,
			"fmi_variable_type":      variable_type,
			"fmi_variable_name":      s.Name,
		}
		if s.Causality == "local" {
			annotations["internal"] = true
		}

		signal := kind.Signal{
			Signal:      s.Name,
			Annotations: &annotations,
			// Annotations: &kind.Annotations{
			// 	"fmi_variable_causality": s.Causality,
			// 	"fmi_variable_id":        s.ValueReference,
			// 	"fmi_variable_type":      variable_type,
			// 	"fmi_variable_name":      s.Name,
			// },
		}
		signals = append(signals, signal)
	}
	signalGroup.Spec.Signals = signals

	// Write the SignalGroup.
	fmt.Fprintf(flag.CommandLine.Output(), "Appending file: %s\n", c.outputFile)
	return writeYaml(&signalGroup, c.outputFile, true)
}
