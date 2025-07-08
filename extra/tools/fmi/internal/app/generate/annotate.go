// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/index"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/log"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"gopkg.in/yaml.v3"
)

type GenFmuAnnotationCommand struct {
	commandName string
	fs          *flag.FlagSet

	simpath      string
	signalGroups string
	inputFile    string
	outputFile   string
	mappingFile  string
	ruleFile     string
	ruleset      string
	logLevel     int

	index *index.YamlFileIndex
	vref  int // Increment before use (i.e. 1, 2, 3...)
}

type Ruleset struct {
	rules [][]string
}

func NewGenFmuAnnotationCommand(name string) *GenFmuAnnotationCommand {
	c := &GenFmuAnnotationCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.simpath, "sim", "/sim", "Path to simulation (Simer layout)")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "Signal Groups to annotate, specify with comma-separated-list")
	c.fs.StringVar(&c.inputFile, "input", "", "Signal Group file (YAML) to be annotated")
	c.fs.StringVar(&c.outputFile, "output", "", "Signal Group file (YAML) with annotations added")
	c.fs.StringVar(&c.mappingFile, "mapping", "", "mapping file")
	c.fs.StringVar(&c.ruleFile, "rule", "", "rules in a csv format")
	c.fs.StringVar(&c.ruleset, "ruleset", "", "use a predefined set of rules ('signal-direction')")
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
	if len(c.inputFile) > 0 || len(c.outputFile) > 0 {
	} else {
		c.index = index.NewYamlFileIndex()
		c.index.Scan(c.simpath)
	}
	if err := c.runAnnotateSignalGroups(); err != nil {
		return err
	}
	if err := c.runAnnotateStack(); err != nil {
		return err
	}

	return nil
}

func (c *GenFmuAnnotationCommand) runAnnotateStack() error {
	if len(c.inputFile) > 0 || len(c.outputFile) > 0 {
		// Called with input/output ... so nothing to do.
		return nil
	}
	if len(c.index.DocMap["Stack"]) != 1 {
		return fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(c.index.DocMap["Stack"]))
	}
	stackDoc := c.index.DocMap["Stack"][0]
	stackYaml, err := os.ReadFile(stackDoc.File)
	if err != nil {
		return fmt.Errorf("Unable to read stack file: %s (%w)", stackDoc.File, err)
	}
	stack := kind.Stack{}
	if err := yaml.Unmarshal(stackYaml, &stack); err != nil {
		return fmt.Errorf("Unable to unmarshal stack yaml: %s (%w)", stackDoc.File, err)
	}
	if stack.Metadata.Annotations == nil {
		a := kind.Annotations{}
		stack.Metadata.Annotations = &a
	}

	// Model Names, annotation: model_runtime__model_inst (',' string).
	modelNames := []string{}
	if stack.Spec.Models != nil {
		for _, model := range *stack.Spec.Models {
			if model.Name == "simbus" {
				continue
			}
			modelNames = append(modelNames, model.Name)
		}
	}
	(*stack.Metadata.Annotations)["model_runtime__model_inst"] = strings.Join(modelNames, ",")

	// Yaml Files, annotation: model_runtime__yaml_files (list).
	yamlFiles := []string{}
	for _, file := range c.index.Files {
		if f, err := filepath.Rel(c.simpath, file); err == nil {
			yamlFiles = append(yamlFiles, f)
		}
	}
	(*stack.Metadata.Annotations)["model_runtime__yaml_files"] = yamlFiles

	return writeYaml(&stack, stackDoc.File, false)
}

func (c *GenFmuAnnotationCommand) runAnnotateSignalGroups() error {
	slog.SetDefault(log.NewLogger(c.logLevel))

	if len(c.inputFile) > 0 && len(c.outputFile) > 0 {
		return c.annotateSignalgroup(c.inputFile, c.outputFile)
	} else if len(c.signalGroups) > 0 && len(c.simpath) > 0 {
		if len(c.index.DocMap["Stack"]) != 1 {
			return fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(c.index.DocMap["Stack"]))
		}
		if signalGroupDocs, ok := c.index.DocMap["SignalGroup"]; ok {
			filter := strings.Split(c.signalGroups, ",")
			for _, doc := range signalGroupDocs {
				if !slices.Contains(filter, doc.Metadata.Name) {
					continue
				}
				return nil
				fmt.Fprintf(flag.CommandLine.Output(), "Annotate SignalGroup: %s (%s)\n", doc.Metadata.Name, doc.File)
				if err := c.annotateSignalgroup(doc.File, doc.File); err != nil {
					return err
				}
			}
		}
	} else {
		return fmt.Errorf("Nothing to do, provide either input/output or simpath/signalGroups")
	}
	return nil
}

func (c *GenFmuAnnotationCommand) annotateSignalgroup(input string, output string) error {
	// Load the Signal Group.
	inputYaml, err := os.ReadFile(input)
	if err != nil {
		return fmt.Errorf("Unable to read input file: %s (%w)", input, err)
	}
	signalgroup := kind.SignalGroup{}
	if err := yaml.Unmarshal(inputYaml, &signalgroup); err != nil {
		return fmt.Errorf("Unable to unmarshal signalgroup yaml: %s (%w)", input, err)
	}

	// Apply annotations.
	ruleset, err := c.getRuleset()
	if err != nil {
		return err
	}
	if len(ruleset.rules) > 0 {
		if err := c.applyRule(ruleset, &signalgroup); err != nil {
			return fmt.Errorf("Could not apply rules. (%w)", err)
		}
	}
	if c.mappingFile != "" {
		if err := c.applyMapping(c.mappingFile, &signalgroup); err != nil {
			return fmt.Errorf("Could not apply mapping. (%w)", err)
		}
	}
	return writeYaml(&signalgroup, output, false)
}

func (c *GenFmuAnnotationCommand) applyMapping(mappingFile string, signalgroup *kind.SignalGroup) error {
	return nil
}

func (c *GenFmuAnnotationCommand) getRuleset() (ruleset Ruleset, err error) {
	if len(c.ruleFile) > 0 {
		ruleset.rules, err = operations.LoadCsv(c.ruleFile)
		if err != nil {
			err = fmt.Errorf("Unable to load CSV: %s (%w)", c.ruleFile, err)
		}
	} else {
		switch c.ruleset {
		case "signal-direction":
			ruleset.rules = [][]string{
				{"Annotation", "Value", "FmiAnnotation", "FmiValue"},
				{"direction", "input", "fmi_variable_causality", "input"},
				{"direction", "output", "fmi_variable_causality", "output"},
			}
		case "":
			// Nothing to do ... no ruleset.
		default:
			err = fmt.Errorf("Ruleset not supported: %s ", c.ruleset)
		}
	}
	return
}

func (c *GenFmuAnnotationCommand) applyRule(ruleset Ruleset, signalgroup *kind.SignalGroup) error {
	if signalgroup.Metadata.Annotations != nil {
		if sg_type := (*signalgroup.Metadata.Annotations)["vector_type"]; sg_type != nil {
			if sg_type == "binary" {
				return fmt.Errorf("Binary not supported")
			}
		}
	}

	for _, signal := range signalgroup.Spec.Signals {
		for _, rule := range ruleset.rules[1:] {
			if ann := (*signal.Annotations)[rule[0]]; ann != nil {
				if ann == rule[1] {
					(*signal.Annotations)[rule[2]] = rule[3]
				}
			}
		}
		c.vref += 1
		(*signal.Annotations)["fmi_variable_vref"] = c.vref
		(*signal.Annotations)["fmi_variable_type"] = "Real"
		(*signal.Annotations)["fmi_variable_name"] = signal.Signal
	}

	return nil
}
