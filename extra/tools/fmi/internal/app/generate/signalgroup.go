// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"log/slog"
	"slices"

	"github.com/boschglobal/dse.clib/extra/go/command/log"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type GenSignalGroupCommand struct {
	commandName string
	fs          *flag.FlagSet

	modelName  string
	inputFile  string
	outputFile string
	logLevel   int
}

func NewGenSignalGroupCommand(name string) *GenSignalGroupCommand {
	c := &GenSignalGroupCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.modelName, "name", "", "Model Name for this FMU (defaults to FMU Name)")
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
	if c.modelName == "" {
		c.modelName = fmiMD.ModelName
	}
	if err := c.generateSignalVector(*fmiMD); err != nil {
		return err
	}
	return nil
}

// Allowed causality/variability combinations, FMI 2.0 section 2.2.7 (a)-(e).
var allowedVariability = map[string][]string{
	"parameter":           {"fixed", "tunable"},
	"calculatedParameter": {"fixed", "tunable"},
	"input":               {"discrete", "continuous"},
	"output":              {"constant", "discrete", "continuous"},
	"local":               {"constant", "fixed", "tunable", "discrete", "continuous"},
	"independent":         {"continuous"},
}

func (c *GenSignalGroupCommand) generateSignalVector(fmiMD fmi2.ModelDescription) error {
	// Build the SignalGroup.

	scalarSignals := []kind.Signal{}
	binarySignals := []kind.Signal{}

	for _, s := range fmiMD.ModelVariables.ScalarVariable {
		if s.Causality == "" {
			s.Causality = "local"
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
		// Parse variability; apply FMI 2 defaults when not specified.
		variability := "continuous"
		if s.Causality == "parameter" || s.Causality == "calculatedParameter" {
			variability = "fixed"
		}
		if s.Variability != nil && *s.Variability != "" {
			variability = *s.Variability
		}
		if !slices.Contains(allowedVariability[s.Causality], variability) {
			return fmt.Errorf("variable %s: variability %q is not allowed for causality %q",
				s.Name, variability, s.Causality)
		}
		annotations["fmi_variable_variability"] = variability
		if s.Causality == "local" || s.Causality == "parameter" {
			annotations["internal"] = true
		}

		if s.Causality == "parameter" || s.Causality == "input" {
			var startValue string
			switch {
			case s.Real != nil:
				startValue = s.Real.Start
			case s.Integer != nil:
				startValue = s.Integer.Start
			case s.Boolean != nil:
				startValue = s.Boolean.Start
			case s.String != nil:
				startValue = s.String.Start
			}
			// An absent start attribute must not become an empty start value.
			if startValue != "" {
				annotations["fmi_variable_start_value"] = startValue
			}
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
			Name: stringPtr(c.modelName),
			Labels: &kind.Labels{
				"channel": "signal_vector",
				"model":   c.modelName,
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
				Name: stringPtr(c.modelName),
				Labels: &kind.Labels{
					"channel": "network_vector",
					"model":   c.modelName,
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
