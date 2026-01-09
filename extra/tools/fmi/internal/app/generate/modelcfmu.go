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

	"github.com/google/uuid"

	"github.com/boschglobal/dse.clib/extra/go/command/log"
	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	"github.com/boschglobal/dse.clib/extra/go/file/index"

	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi3"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type GenModelCFmuCommand struct {
	commandName string
	fs          *flag.FlagSet

	simpath      string
	platform     string
	signalGroups string
	name         string
	version      string
	fmiVersion   string
	uuid         string
	outdir       string
	fmiPackage   string
	logLevel     int

	// Model parameter
	startTime float64
	endTime   float64
	stepSize  float64

	libRootPath     string
	modelIdentifier string
}

const directIndexFile = "direct_index.yaml"

func NewModelCFmuCommand(name string) *GenModelCFmuCommand {
	c := &GenModelCFmuCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.simpath, "sim", "/sim", "Path to simulation (Simer layout)")
	c.fs.StringVar(&c.platform, "platform", "linux-amd64", "Platform of FMU to generate")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "Signal Groups to export as FMU variables, default is all, specify with comma-separated-list")
	c.fs.StringVar(&c.name, "name", "", "Name of the FMU")
	c.fs.StringVar(&c.version, "version", "0.0.1", "Version to assign to the FMU")
	c.fs.StringVar(&c.uuid, "uuid", "11111111-2222-3333-4444-555555555555", "UUID to assign to the FMU, set to '' to generate a new UUID")
	c.fs.StringVar(&c.outdir, "outdir", "/out", "Output directory for the FMU file")
	c.fs.StringVar(&c.fmiVersion, "fmiVersion", "2", "Modelica FMI Version")
	c.fs.StringVar(&c.fmiPackage, "fmiPackage", "", "Fmi Package path")
	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")

	// Supports unit testing.
	c.fs.StringVar(&c.libRootPath, "libroot", "/usr/local", "System lib root path (where lib & lib32 directory are found)")

	// Default values
	c.startTime = 0.0
	c.endTime = 9999.0
	c.stepSize = 0.0005

	return c
}

func (c GenModelCFmuCommand) Name() string {
	return c.commandName
}

func (c GenModelCFmuCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *GenModelCFmuCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *GenModelCFmuCommand) Run() error {
	slog.SetDefault(log.NewLogger(c.logLevel))
	if len(c.uuid) == 0 {
		c.uuid = uuid.New().String()
	}
	if len(c.fmiPackage) == 0 {
		c.fmiPackage = filepath.Join("/package", c.platform, "fmimodelc")
	}
	c.modelIdentifier = fmt.Sprintf("fmi%smodelcfmu", c.fmiVersion)

	// Index the simulation files, layout is partly fixed:
	//	<simpath>/data/simulation.yaml
	//	<simpath>/...
	fmt.Fprintf(flag.CommandLine.Output(), "Scanning simulation (%s) ...\n", c.simpath)
	var index = index.NewYamlFileIndex()
	index.Scan(c.simpath)
	if len(index.DocMap["Stack"]) != 1 {
		return fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(index.DocMap["Stack"]))
	}
	stackDoc := index.DocMap["Stack"][0]
	if stackDoc.File != filepath.Join(c.simpath, "data/simulation.yaml") {
		// This constraint is hard coded in fmi2fmu.c.
		return fmt.Errorf("simulation layout incorrect, Stack should be in file data/simulation.yaml (is in: %s)", stackDoc.File)
	}

	// Build the FMU file layout.
	fmuOutDir := filepath.Join(c.outdir, c.name)
	fmt.Fprintf(flag.CommandLine.Output(), "Build the FMU file layout (%s) ...\n", fmuOutDir)
	if err := os.RemoveAll(fmuOutDir); err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Join(fmuOutDir, "resources", "sim"), os.ModePerm); err != nil {
		return err
	}

	fmuBinDir := c.getFmuBinaryDirName(c.platform)
	if fmuBinDir == "" {
		return fmt.Errorf("platform not supported (%s)", c.platform)
	}
	fmuBinPath := filepath.Join(fmuOutDir, "binaries", fmuBinDir)
	if err := os.MkdirAll(fmuBinPath, os.ModePerm); err != nil {
		return fmt.Errorf("could not create FMU binaries directory (%v)", err)
	}

	// Copy the necessary binaries from the FMI package.
	// libfmi2modelcfmu.so => fmi2modelcfmu.so
	src := c.getFmuLibPath(c.fmiPackage, fmt.Sprintf("lib%s", c.modelIdentifier), c.platform)
	if _, err := os.Stat(src); err != nil {
		// Also try without "lib" prefix.
		src = c.getFmuLibPath(c.fmiPackage, fmt.Sprintf("%s", c.modelIdentifier), c.platform)
	}
	tgt := c.getFmuBinPath(fmuBinPath, c.modelIdentifier, c.platform)
	if err := operations.Copy(src, tgt); err != nil {
		return fmt.Errorf("could not copy FMU binary (%v)", err)
	}
	// libmodelc.so => libmodelc.so
	src = c.getFmuLibPath(c.fmiPackage, "libmodelc", c.platform)
	tgt = c.getFmuBinPath(fmuBinPath, "libmodelc", c.platform)
	if err := operations.Copy(src, tgt); err != nil {
		return fmt.Errorf("could not copy modelc binary (%v)", err)
	}
	// /sim => resources/sim
	if err := operations.CopyDirectory(c.simpath, filepath.Join(fmuOutDir, "resources", "sim")); err != nil {
		return fmt.Errorf("could not copy FMU resources (%v)", err)
	}
	// /licenses => resources/licenses
	if _, err := os.Stat(c.getFmuLicensesPath()); err == nil {
		if err := operations.CopyDirectory(c.getFmuLicensesPath(), filepath.Join(fmuOutDir, "resources/licenses")); err != nil {
			return fmt.Errorf("could not copy licenses (%v)", err)
		}
	}

	// Construct the FMU Model Description.
	fmuModelDescriptionFilename := filepath.Join(fmuOutDir, "modelDescription.xml")
	fmt.Fprintf(flag.CommandLine.Output(), "Create FMU Model Description (%s) ...\n", fmuModelDescriptionFilename)
	fmuXml := c.createFmuXml(c.name, c.uuid, c.version, c.signalGroups, index)
	if err := writeXml(fmuXml, fmuModelDescriptionFilename); err != nil {
		return fmt.Errorf("could not generate the FMU Model Description (%v)", err)
	}

	// Produce the FMU.
	fmuFilename := fmuOutDir + ".fmu"
	fmt.Fprintf(flag.CommandLine.Output(), "Create FMU Package (%s) ...\n", fmuFilename)
	if err := operations.Zip(fmuOutDir, fmuFilename); err != nil {
		return fmt.Errorf("could not create FMU (%v)", err)
	}

	return nil
}

func (c *GenModelCFmuCommand) getFmuBinaryDirName(platform string) (dir string) {
	os, arch, found := strings.Cut(platform, "-")
	if !found {
		return
	}

	switch os {
	case "linux":
		switch arch {
		case "amd64":
			switch c.fmiVersion {
			case "2":
				dir = "linux64"
			case "3":
				dir = "x86_64-linux"
			}
		case "x86", "i386":
			switch c.fmiVersion {
			case "2":
				dir = "linux32"
			case "3":
				dir = "x86-linux"
			}
		}
	case "windows":
		switch arch {
		case "x64":
			switch c.fmiVersion {
			case "2":
				dir = "win64"
			case "3":
				dir = "x86_64-windows"
			}
		case "x86":
			switch c.fmiVersion {
			case "2":
				dir = "win32"
			case "3":
				dir = "x86-windows"
			}
		}
	}
	return
}

func (c *GenModelCFmuCommand) getFmuLibPath(packagePath string, modelIdentifier string, platform string) string {
	var libpath string
	extension := "so"

	os, _, found := strings.Cut(platform, "-")
	if found {
		switch os {
		case "linux":
			libpath = "lib"
		case "windows":
			libpath = "lib"
			extension = "dll"
		}
	}
	return filepath.Join(packagePath, libpath, fmt.Sprintf("%s.%s", modelIdentifier, extension))
}

func (c *GenModelCFmuCommand) getFmuLicensesPath() string {
	return filepath.Join("/licenses")
}

func (c *GenModelCFmuCommand) getFmuBinPath(fmuBinPath string, modelIdentifier string, platform string) string {
	extension := "so"
	os, _, found := strings.Cut(platform, "-")
	if found {
		switch os {
		case "windows":
			extension = "dll"
		}
	}
	return filepath.Join(fmuBinPath, fmt.Sprintf("%s.%s", modelIdentifier, extension))
}

func (c *GenModelCFmuCommand) createFmuXml(name string, uuid string, version string, signalGroups string, index *index.YamlFileIndex) interface{} {

	fmiConfig := fmi.FmiConfig{
		Name:            name,
		UUID:            uuid,
		Version:         version,
		Description:     fmt.Sprintf("Model: %s, via FMI ModelC FMU (using DSE ModelC Runtime).", name),
		StartTime:       c.startTime,
		StopTime:        c.endTime,
		StepSize:        c.stepSize,
		GenerationTool:  "DSE FMI - ModelC FMU",
		FmiVersion:      c.fmiVersion,
		ModelIdentifier: c.modelIdentifier,
	}
	switch fmiConfig.FmiVersion {
	case "2":
		fmuXml := fmi2.ModelDescription{}
		if err := fmi2.SetGeneralFmuXmlFields(fmiConfig, &fmuXml); err != nil {
			return err
		}
		if _, err := fmi2.VariablesFromSignalgroups(&fmuXml, signalGroups, index); err != nil {
			return err
		}
		return fmuXml
	case "3":
		fmuXml := fmi3.ModelDescription{}
		if err := fmi3.SetGeneralFmuXmlFields(fmiConfig, &fmuXml); err != nil {
			return err
		}
		if _, err := fmi3.VariablesFromSignalgroups(&fmuXml, signalGroups, index); err != nil {
			return err
		}
		return fmuXml
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
	buildIndex   bool
	logLevel     int

	directIndex *DirectIndex

	index *index.YamlFileIndex
	vref  int // Increment before use (i.e. 1, 2, 3...)
}

type Ruleset struct {
	rules [][]string
}

func NewGenModelCFmuAnnotationCommand(name string) *GenModelCFmuAnnotationCommand {
	c := &GenModelCFmuAnnotationCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.simpath, "sim", "/sim", "Path to simulation (Simer layout)")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "Signal Groups to annotate, default is all, specify with comma-separated-list")
	c.fs.StringVar(&c.mappingFile, "mapping", "", "mapping file")
	c.fs.StringVar(&c.ruleFile, "rule", "", "rules in a csv format")
	c.fs.StringVar(&c.ruleset, "ruleset", "", "use a predefined set of rules ('signal-direction')")
	c.fs.BoolVar(&c.buildIndex, "index", false, "build a direct index for the FMU")
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
	slog.SetDefault(log.NewLogger(c.logLevel))

	// Check if necessary parameters are present and valid.
	if len(c.simpath) == 0 {
		return fmt.Errorf("Nothing to do, provide simpath")
	}
	c.index = index.NewYamlFileIndex()
	c.index.Scan(c.simpath)
	if len(c.index.DocMap["Stack"]) != 1 {
		return fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(c.index.DocMap["Stack"]))
	}

	// Generate an index file (for FMU Direct Indexing).
	if c.buildIndex {
		if err := c.runGenerateDirectIndex(); err != nil {
			return err
		}
	}
	// Generate the annotations.
	if err := c.runAnnotateSignalGroups(); err != nil {
		return err
	}
	if err := c.runAnnotateStack(); err != nil {
		return err
	}
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
			fmt.Fprintf(flag.CommandLine.Output(), "Model Runtime YAML File: %s -> %s\n", file, f)
		}
	}
	stackDoc.Metadata.Annotations["model_runtime__yaml_files"] = yamlFiles

	c.index.Updated(stackDoc.File)
	return nil
}

func (c *GenModelCFmuAnnotationCommand) runAnnotateSignalGroups() error {
	filter := []string{}
	if len(c.signalGroups) > 0 {
		filter = strings.Split(c.signalGroups, ",")
	}
	if signalGroupDocs, ok := c.index.DocMap["SignalGroup"]; ok {
		for i, doc := range signalGroupDocs {
			if len(filter) > 0 && !slices.Contains(filter, doc.Metadata.Name) {
				continue
			}
			if _, ok := (doc.Metadata.Labels)["index"]; ok {
				// Don't annotate 'index' SignalGroups.
				continue
			}
			fmt.Fprintf(flag.CommandLine.Output(), "Annotate SignalGroup: %s (%s)\n", doc.Metadata.Name, doc.File)
			signalGroupDoc := &c.index.DocMap["SignalGroup"][i]
			if err := c.annotateSignalgroup(signalGroupDoc); err != nil {
				return err
			}
		}
	}
	return nil
}

func lookupSimBusChannelName(index *index.YamlFileIndex, sgDoc *kind.KindDoc) (string, error) {
	/*
		For the specified Channel, search for a Model Instance with a Channel that
		as the same Selector/Label set.
		If a Model Instance has a Channel _without_ a Selector set, then fallback
		to the Selector set of the Model.
	*/
	if len(index.DocMap["Stack"]) != 1 {
		return "", fmt.Errorf("simulation folder contains %d Stacks, expected 1", len(index.DocMap["Stack"]))
	}
	stackDoc := &index.DocMap["Stack"][0]
	stackSpec := stackDoc.Spec.(*schema_kind.StackSpec)
	if stackSpec.Models == nil {
		return "", fmt.Errorf("Stack contains no models")
	}
	// For each model (instance) in this stack.
	for _, model := range *stackSpec.Models {
		if model.Name == "simbus" {
			continue
		}
		// For each channel of this model.
	model_channel:
		for _, channel := range *model.Channels {
			// Compare the sgDoc labels with the selectors of this channel.
			selectorChannel := channel
			if channel.Selectors == nil {
				slog.Info("  stack.model.channel missing selectors", "alias", *channel.Alias)
				// Search the channels on the Model Definition.
				for _, modelDfn := range index.DocMap["Model"] {
					if modelDfn.Metadata.Name != model.Model.Name {
						continue
					}
					modelDfnSpec := modelDfn.Spec.(*schema_kind.ModelSpec)
					for _, modelChn := range *modelDfnSpec.Channels {
						if *modelChn.Alias != *channel.Alias {
							continue
						}
						if channel.Selectors == nil {
							slog.Info("  model.channel missing selectors", "alias", *channel.Alias)
						}
						selectorChannel = modelChn
					}
				}
			}
			if selectorChannel.Selectors == nil {
				continue model_channel
			}
			// Each selector must match to a label.
			selectorMatchCount := 0
			for selector, selector_val := range *selectorChannel.Selectors {
				selectorFound := false
				for label, label_val := range sgDoc.Metadata.Labels {
					if selector != label {
						continue
					}
					selectorFound = true
					if selector_val != label_val {
						slog.Debug(fmt.Sprintf("  selector value mismatch: %v != %v (%s)", selector_val, label_val, label))
						continue model_channel
					}
					selectorMatchCount += 1
				}
				if !selectorFound {
					slog.Info("  selector not found", "label", selector)
				}
			}
			if selectorMatchCount == len(*selectorChannel.Selectors) {
				slog.Info("SimBus channel found", "name", *channel.Name, "alias", *selectorChannel.Alias)
				return *channel.Name, nil
			}
		}
	}

	return "", fmt.Errorf("Stack contains no Model:Channel with matching selectors")
}

func (c *GenModelCFmuAnnotationCommand) runGenerateDirectIndex() error {
	c.directIndex = NewDirectIndex()
	indexFile := filepath.Join(c.simpath, "data", directIndexFile)
	fmt.Fprintf(flag.CommandLine.Output(), "Generating Direct Index: %s\n", indexFile)

	filter := []string{}
	if len(c.signalGroups) > 0 {
		filter = strings.Split(c.signalGroups, ",")
	}
	if signalGroupDocs, ok := c.index.DocMap["SignalGroup"]; ok {
		for _, doc := range signalGroupDocs {
			if len(filter) > 0 && !slices.Contains(filter, doc.Metadata.Name) {
				slog.Info(fmt.Sprintf("SignalGroup filtered: %s", doc.Metadata.Name))
				continue
			}
			if _, ok := (doc.Metadata.Labels)["index"]; ok {
				// Don't index 'index' SignalGroups.
				continue
			}
			simbusChannelName, err := lookupSimBusChannelName(c.index, &doc)
			if err != nil {
				slog.Error(fmt.Sprintf("SignalGroup not associated with SimBus channel: %v", doc.Metadata.Name), "\nfile", doc.File, "\nerror", err)
				continue
			}
			group := c.directIndex.getSignalGroup(simbusChannelName)
			signalgroupSpec := doc.Spec.(*schema_kind.SignalGroupSpec)
			for _, signal := range signalgroupSpec.Signals {
				c.directIndex.addSignal(group.name, signal.Signal)
			}
		}
	}

	c.directIndex.calculateMapOffsets()
	if err := c.directIndex.writeSignalGroup(indexFile); err != nil {
		slog.Error("Error writing direct index", "err", err)
	}

	// Add to index.
	c.index.Add(indexFile)

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
					var vref int
					if c.directIndex != nil {
						simbusChannelName, err := lookupSimBusChannelName(c.index, signalgroupDoc)
						if err != nil {
							continue
						}
						if v, found := c.directIndex.getSignalOffset(simbusChannelName, signal.Signal); found {
							vref = int(v)
						} else {
							continue
						}
					} else {
						c.vref += 1
						vref = c.vref
					}
					(*signal.Annotations)["fmi_variable_vref"] = vref
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
