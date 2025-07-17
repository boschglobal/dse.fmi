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
	"strconv"
	"strings"

	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	"github.com/boschglobal/dse.clib/extra/go/file/index"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/log"
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

type GenModelCFmuAnnotationCommand struct {
	commandName string
	fs          *flag.FlagSet

	simpath      string
	signalGroups string
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

func NewGenModelCFmuAnnotationCommand(name string) *GenModelCFmuAnnotationCommand {
	c := &GenModelCFmuAnnotationCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.simpath, "sim", "/sim", "Path to simulation (Simer layout)")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "Signal Groups to annotate, specify with comma-separated-list")
	c.fs.StringVar(&c.mappingFile, "mapping", "", "mapping file")
	c.fs.StringVar(&c.ruleFile, "rule", "", "rules in a csv format")
	c.fs.StringVar(&c.ruleset, "ruleset", "", "use a predefined set of rules ('signal-direction')")
	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")
	return c
}

func (c GenModelCFmuAnnotationCommand) Name() string {
	return c.commandName
}

func (c GenModelCFmuAnnotationCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *GenModelCFmuAnnotationCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *GenModelCFmuAnnotationCommand) Run() error {
	c.index = index.NewYamlFileIndex()
	c.index.Scan(c.simpath)
	if err := c.runAnnotateSignalGroups(); err != nil {
		return err
	}
	if err := c.runAnnotateStack(); err != nil {
		return err
	}
	//return nil
	return c.index.Save()
}

func (c *GenModelCFmuAnnotationCommand) runAnnotateStack() error {
	if len(c.index.DocMap["Stack"]) != 1 {
		return fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(c.index.DocMap["Stack"]))
	}
	stackDoc := &c.index.DocMap["Stack"][0]
	stackSpec := stackDoc.Spec.(*schema_kind.StackSpec)
	if stackDoc.Metadata.Annotations == nil {
		a := schema_kind.Annotations{}
		stackDoc.Metadata.Annotations = a
	}

	// Model Names, annotation: model_runtime__model_inst (',' string).
	modelNames := []string{}
	if stackSpec.Models != nil {
		for _, model := range *stackSpec.Models {
			if model.Name == "simbus" {
				continue
			}
			modelNames = append(modelNames, model.Name)
		}
	}
	stackDoc.Metadata.Annotations["model_runtime__model_inst"] = strings.Join(modelNames, ",")

	// Yaml Files, annotation: model_runtime__yaml_files (list).
	yamlFiles := []string{}
	for file, _ := range c.index.FileMap {
		if f, err := filepath.Rel(c.simpath, file); err == nil {
			yamlFiles = append(yamlFiles, f)
		}
	}
	stackDoc.Metadata.Annotations["model_runtime__yaml_files"] = yamlFiles

	c.index.Updated(stackDoc.File)
	return nil
}

func (c *GenModelCFmuAnnotationCommand) runAnnotateSignalGroups() error {
	slog.SetDefault(log.NewLogger(c.logLevel))

	if len(c.signalGroups) > 0 && len(c.simpath) > 0 {
		if len(c.index.DocMap["Stack"]) != 1 {
			return fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(c.index.DocMap["Stack"]))
		}
		if signalGroupDocs, ok := c.index.DocMap["SignalGroup"]; ok {
			filter := strings.Split(c.signalGroups, ",")
			for i, doc := range signalGroupDocs {
				if !slices.Contains(filter, doc.Metadata.Name) {
					continue
				}
				fmt.Fprintf(flag.CommandLine.Output(), "Annotate SignalGroup: %s (%s)\n", doc.Metadata.Name, doc.File)
				signalGroupDoc := &c.index.DocMap["SignalGroup"][i]
				if err := c.annotateSignalgroup(signalGroupDoc); err != nil {
					return err
				}
			}
		}
	} else {
		return fmt.Errorf("Nothing to do, provide simpath/signalGroups")
	}
	return nil
}

func (c *GenModelCFmuAnnotationCommand) annotateSignalgroup(signalgroupDoc *kind.KindDoc) error {
	// Apply annotations.
	ruleset, err := c.getRuleset()
	if err != nil {
		return err
	}
	if len(ruleset.rules) > 0 {
		if err := c.applyRule(ruleset, signalgroupDoc); err != nil {
			return fmt.Errorf("Could not apply rules. (%w)", err)
		}

		// Annotate based on direction (scalar/real).
		signalgroupSpec := signalgroupDoc.Spec.(*schema_kind.SignalGroupSpec)
		directions := []string{"input", "output"}
		for _, signal := range signalgroupSpec.Signals {
			if direction, ok := (*signal.Annotations)["fmi_variable_causality"]; ok {
				if slices.Contains(directions, direction.(string)) {
					c.vref += 1
					(*signal.Annotations)["fmi_variable_vref"] = c.vref
					(*signal.Annotations)["fmi_variable_type"] = "Real"
					(*signal.Annotations)["fmi_variable_name"] = signal.Signal
				}
			}
		}
		c.index.Updated(signalgroupDoc.File)
	}

	return nil
}

func (c *GenModelCFmuAnnotationCommand) getRuleset() (ruleset Ruleset, err error) {
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

func (c *GenModelCFmuAnnotationCommand) applyRule(ruleset Ruleset, signalgroupDoc *kind.KindDoc) error {
	signalgroupSpec := signalgroupDoc.Spec.(*schema_kind.SignalGroupSpec)

	if sg_type := signalgroupDoc.Metadata.Annotations["vector_type"]; sg_type != nil {
		if sg_type == "binary" {
			return fmt.Errorf("Binary not supported")
		}
	}

	for _, signal := range signalgroupSpec.Signals {
		for _, rule := range ruleset.rules[1:] {
			if ann := (*signal.Annotations)[rule[0]]; ann != nil {
				if ann == rule[1] {
					(*signal.Annotations)[rule[2]] = rule[3]
				}
			}
		}
	}

	return nil
}
