// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"log/slog"
	"slices"

	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/log"
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

func (c *GenSignalGroupCommand) generateSignalVector(fmiMD fmi2.ModelDescription) error {
	// Build the SignalGroup.

	scalarSignals := []kind.Signal{}
	binarySignals := []kind.Signal{}

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

		annotations := kind.Annotations{
			"fmi_variable_causality": s.Causality,
			"fmi_variable_vref":      s.ValueReference,
			"fmi_variable_type":      variable_type,
			"fmi_variable_name":      s.Name,
		}
		if s.Causality == "local" {
			annotations["internal"] = true
		}
		if s.Annotations != nil {
			toolAnnotations := kind.Annotations{}
			for _, tool := range s.Annotations.Tool {
				for _, anno := range tool.Annotation {
					name := fmt.Sprintf("%s.%s", tool.Name, anno.Name)
					toolAnnotations[name] = anno.Text
				}
			}
			annotations["fmi_annotations"] = toolAnnotations
		}

		signal := kind.Signal{
			Signal:      s.Name,
			Annotations: &annotations,
		}
		if slices.Contains([]string{"String"}, variable_type) {
			binarySignals = append(binarySignals, signal)
		} else {
			scalarSignals = append(scalarSignals, signal)
		}
	}

	// Write the SignalGroups.
	fmt.Fprintf(flag.CommandLine.Output(), "Appending file: %s\n", c.outputFile)

	signalVector := kind.SignalGroup{
		Kind: "SignalGroup",
		Metadata: &kind.ObjectMetadata{
			Name: stringPtr(fmiMD.ModelName),
			Labels: &kind.Labels{
				"channel": "signal_vector",
				"model":   fmiMD.ModelName,
			},
		},
	}
	signalVector.Spec.Signals = scalarSignals
	if err := writeYaml(&signalVector, c.outputFile, true); err != nil {
		return err
	}

	if len(binarySignals) > 0 {
		networkVector := kind.SignalGroup{
			Kind: "SignalGroup",
			Metadata: &kind.ObjectMetadata{
				Name: stringPtr(fmiMD.ModelName),
				Labels: &kind.Labels{
					"channel": "network_vector",
					"model":   fmiMD.ModelName,
				},
				Annotations: &kind.Annotations{
					"vector_type": "binary",
				},
			},
		}
		networkVector.Spec.Signals = binarySignals
		if err := writeYaml(&networkVector, c.outputFile, true); err != nil {
			return err
		}
	}

	return nil
}
