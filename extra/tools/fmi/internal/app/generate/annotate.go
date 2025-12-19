// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"log/slog"
	"os"
	"strconv"

	"github.com/boschglobal/dse.clib/extra/go/command/log"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"gopkg.in/yaml.v3"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type GenFmuAnnotationCommand struct {
	commandName string
	fs          *flag.FlagSet

	inputFile   string
	outputFile  string
	mappingFile string
	ruleFile    string
	logLevel    int
}

func NewGenFmuAnnotationCommand(name string) *GenFmuAnnotationCommand {
	c := &GenFmuAnnotationCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.inputFile, "input", "", "Signal Group file (YAML) to be annotated")
	c.fs.StringVar(&c.outputFile, "output", "", "Signal Group file (YAML) with annotations added")
	c.fs.StringVar(&c.mappingFile, "mapping", "", "mapping file")
	c.fs.StringVar(&c.ruleFile, "rule", "", "rules in a csv format")
	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")
	return c
}

func (c GenFmuAnnotationCommand) Name() string {
	return c.commandName
}

func (c GenFmuAnnotationCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *GenFmuAnnotationCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *GenFmuAnnotationCommand) Run() error {
	slog.SetDefault(log.NewLogger(c.logLevel))

	// Load the Signal Group.
	inputYaml, err := os.ReadFile(c.inputFile)
	if err != nil {
		return fmt.Errorf("Unable to read input file: %s (%w)", c.inputFile, err)
	}
	signalgroup := schema_kind.SignalGroup{}
	if err := yaml.Unmarshal(inputYaml, &signalgroup); err != nil {
		return fmt.Errorf("Unable to unmarshal signalgroup yaml: %s (%w)", c.inputFile, err)
	}

	// Apply annotations.
	if c.ruleFile != "" {
		if err := c.applyRule(c.ruleFile, &signalgroup); err != nil {
			return fmt.Errorf("Could not apply rules. (%w)", err)
		}
	}
	if c.mappingFile != "" {
		if err := c.applyMapping(c.mappingFile, &signalgroup); err != nil {
			return fmt.Errorf("Could not apply mapping. (%w)", err)
		}
	}
	return writeYaml(&signalgroup, c.outputFile, false)
}

func (c *GenFmuAnnotationCommand) applyMapping(mappingFile string, signalgroup *schema_kind.SignalGroup) error {
	return nil
}

func (c *GenFmuAnnotationCommand) applyRule(ruleFile string, signalgroup *schema_kind.SignalGroup) error {

	rules, err := operations.LoadCsv(ruleFile)
	if err != nil {
		return fmt.Errorf("Unable to load CSV: %s (%w)", ruleFile, err)
	}
	if signalgroup.Metadata.Annotations != nil {
		if sg_type := (*signalgroup.Metadata.Annotations)["vector_type"]; sg_type != nil {
			if sg_type == "binary" {
				return fmt.Errorf("Binary not supported: %s (%w)", ruleFile, err)
			}
		}
	}

	var vr = 1
	for _, signal := range signalgroup.Spec.Signals {
		for _, rule := range rules[1:] {
			if ann := (*signal.Annotations)[rule[0]]; ann != nil {
				if ann == rule[1] {
					(*signal.Annotations)[rule[2]] = rule[3]
				}
			}
		}
		(*signal.Annotations)["fmi_variable_vref"] = strconv.Itoa(vr)
		(*signal.Annotations)["fmi_variable_type"] = "Real"
		(*signal.Annotations)["fmi_variable_name"] = signal.Signal
		vr++
	}

	return nil
}
