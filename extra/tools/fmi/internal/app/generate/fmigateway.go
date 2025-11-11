// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"encoding/json"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"github.com/boschglobal/dse.clib/extra/go/file/handler"
	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	"github.com/boschglobal/dse.clib/extra/go/file/index"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi3"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/log"
	schemaKind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/google/uuid"
	"gopkg.in/yaml.v3"
)

// GenFmiGatewayCommand handles FMI gateway generation.
type GenFmiGatewayCommand struct {
	commandName string
	fs          *flag.FlagSet

	// CLI arguments
	logLevel        int
	signalGroups    string
	uuid            string
	version         string
	fmiVersion      string
	outdir          string
	stack           string
	name            string
	modelIdentifier string
	annotation      string

	// Model parameters
	startTime float64
	endTime   float64
	stepSize  float64

	// Number of Envar Signals
	signalIdx int

	libRootPath string
}

// NewFmiGatewayCommand creates a new FMI Gateway command instance.
func NewFmiGatewayCommand(name string) *GenFmiGatewayCommand {
	c := &GenFmiGatewayCommand{
		commandName: name,
		fs:          flag.NewFlagSet(name, flag.ExitOnError),
		// Default values
		startTime: 0.0,
		endTime:   9999.0,
		stepSize:  0.0005,
		signalIdx: 1,
	}

	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "A list of signalgroups separated by comma")
	c.fs.StringVar(&c.stack, "stack", "", "Path to the stack.yaml")
	c.fs.StringVar(&c.outdir, "outdir", "out", "Output directory for the FMU file")
	c.fs.StringVar(&c.uuid, "uuid", "", "UUID to assign to the FMU, set to '' to generate a new UUID")
	c.fs.StringVar(&c.version, "version", "0.0.1", "Version to assign to the FMU")
	c.fs.StringVar(&c.fmiVersion, "fmiVersion", "2", "Modelica FMI Version")
	c.fs.StringVar(&c.name, "name", "", "Name of the FMU")
	c.fs.StringVar(&c.modelIdentifier, "modelidentifier", "", "ModelIdentifier field in the FMU modelDescription.xml")
	c.fs.StringVar(&c.annotation, "annotations", "{}", "annotations in dictionary format")

	// Supports unit testing.
	c.fs.StringVar(&c.libRootPath, "libroot", "/usr/local", "System lib root path (where lib & lib32 directory are found)")

	return c
}

// Name returns the command name.
func (c GenFmiGatewayCommand) Name() string {
	return c.commandName
}

// FlagSet returns the flag set for this command.
func (c GenFmiGatewayCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

// Parse parses the command line arguments.
func (c *GenFmiGatewayCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

// Run executes the FMI gateway generation.
func (c *GenFmiGatewayCommand) Run() error {
	slog.SetDefault(log.NewLogger(c.logLevel))
	if len(c.uuid) == 0 {
		c.uuid = uuid.New().String()
	}

	if err := os.MkdirAll(c.outdir, os.ModePerm); err != nil {
		return err
	}
	envars, err := c.parseStack()
	if err != nil {
		return err
	}
	if err := c.mergeSignalGroups(c.signalGroups, c.outdir); err != nil {
		return err
	}

	fmuXml, channels, err := c.createFmuXml(envars)
	if err != nil {
		return err
	}

	fmt.Printf("channels: %v\n", channels)

	if err := c.generateModel(channels); err != nil {
		return err
	}

	fmuModelDescriptionFilename := filepath.Join(c.outdir, "modelDescription.xml")
	if err := writeXml(fmuXml, fmuModelDescriptionFilename); err != nil {
		return fmt.Errorf("could not generate the FMU Model Description (%v)", err)
	}
	return nil
}

func (c *GenFmiGatewayCommand) patchSignal(vr *int, sgType string, signals []schemaKind.Signal, usedSignals *[]string) ([]schemaKind.Signal, error) {
	var signalList []schemaKind.Signal
	if sgType == "string" {
		for _, s := range signals {
			(*s.Annotations)["fmi_variable_vref"] = *vr
			(*s.Annotations)["fmi_variable_vref_input"] = *vr
			(*s.Annotations)["fmi_variable_vref_output"] = *vr + 1
			(*s.Annotations)["dse.standards.fmi-ls-binary-to-text.vref"] = []int{*vr, *vr + 1}
			(*s.Annotations)["dse.standards.fmi-ls-bus-topology.rx_vref"] = []int{*vr}
			(*s.Annotations)["dse.standards.fmi-ls-bus-topology.tx_vref"] = []int{*vr + 1}
			signalList = append(signalList, s)
		}
		return signalList, nil
	}
	for _, s := range signals {
		if slices.Contains(*usedSignals, s.Signal) {
			continue
		}
		*usedSignals = append(*usedSignals, s.Signal)
		slog.Info(fmt.Sprintf("Patch Signals: %s", s.Signal))
		causality := ""
		if (*s.Annotations)["softecu_direction"] != nil {
			switch (*s.Annotations)["softecu_direction"] {
			case "M2E":
				causality = "input"
			case "E2M":
				causality = "output"
			default:
			}
		}
		if (*s.Annotations)["direction"] != nil {
			causality = ((*s.Annotations)["direction"]).(string)
		}
		if (*s.Annotations)["fmi_variable_causality"] != nil {
			causality = ((*s.Annotations)["fmi_variable_causality"]).(string)
		}
		if causality == "local" {
			continue
		}

		annotations := schemaKind.Annotations{
			"fmi_variable_causality": causality,
			"fmi_variable_vref":      *vr,
			"fmi_variable_type":      sgType,
			"fmi_variable_name":      s.Signal,
		}
		*s.Annotations = annotations
		signalList = append(signalList, s)
		*vr++
	}
	return signalList, nil
}

func (c *GenFmiGatewayCommand) mergeSignalGroups(signalGroups string, outDir string) error {
	signalGroupList := []string{}
	if len(signalGroups) > 0 {
		signalGroupList = strings.Split(signalGroups, ",")
	}
	var signalList []string
	for _, signalGroup := range signalGroupList {

		// Load the Signal Group.
		inputYaml, err := os.ReadFile(signalGroup)
		if err != nil {
			return fmt.Errorf("unable to read input file: %s (%w)", signalGroup, err)
		}
		sg := schemaKind.SignalGroup{}
		if err := yaml.Unmarshal(inputYaml, &sg); err != nil {
			return fmt.Errorf("unable to unmarshal signalgroup yaml: %s (%w)", signalGroup, err)
		}
		sgType := "real"
		if sg.Metadata.Annotations != nil {
			if (*sg.Metadata.Annotations)["vector_type"] != nil {
				if (*sg.Metadata.Annotations)["vector_type"] == "binary" {
					sgType = "string"
				}
			}
		}

		signals := sg.Spec.Signals

		sg.Spec.Signals, err = c.patchSignal(&c.signalIdx, sgType, signals, &signalList)
		if err != nil {
			return err
		}

		if err := writeYaml(&sg, filepath.Join(outDir, "fmu.yaml"), true); err != nil {
			return err
		}
	}
	return nil
}

func (c *GenFmiGatewayCommand) buildFmi2XmlSignals(
	fmuXml *fmi2.ModelDescription, index *index.YamlFileIndex,
	envars *[]schemaKind.SignalGroupSpec) ([]string, error) {

	if envars != nil {
		if err := fmi2.ScalarSignal(fmuXml, (*envars)[1]); err != nil {
			slog.Warn(fmt.Sprintf("could not set envars in xml (Error: %s)", err.Error()))
		}
		if err := fmi2.StringSignal(fmuXml, (*envars)[0]); err != nil {
			slog.Warn(fmt.Sprintf("could not set envars in xml (Error: %s)", err.Error()))
		}
	}

	channels, err := fmi2.VariablesFromSignalgroups(fmuXml, "", index)
	if err != nil {
		return nil, err
	}

	return channels, nil
}

func (c *GenFmiGatewayCommand) buildFmi3XmlSignals(
	fmuXml *fmi3.ModelDescription, index *index.YamlFileIndex,
	envars *[]schemaKind.SignalGroupSpec) ([]string, error) {

	if envars != nil {
		if err := fmi3.ScalarSignal((*envars)[1], fmuXml); err != nil {
			slog.Warn(fmt.Sprintf("could not set envars in xml (Error: %s)", err.Error()))
		}
		if err := fmi3.StringSignal((*envars)[0], fmuXml); err != nil {
			slog.Warn(fmt.Sprintf("could not set envars in xml (Error: %s)", err.Error()))
		}
	}

	channels, err := fmi3.VariablesFromSignalgroups(fmuXml, "", index)
	if err != nil {
		return nil, err
	}
	return channels, nil
}

func getModelIdentifier(custom, defaultValue string) string {
	if custom != "" {
		return custom
	}
	return defaultValue
}

func (c *GenFmiGatewayCommand) createFmuXml(envars *[]schemaKind.SignalGroupSpec) (interface{}, []string, error) {
	index := index.NewYamlFileIndex()
	_, docs, err := handler.ParseFile(filepath.Join(c.outdir, "fmu.yaml"))
	if err != nil {
		return nil, nil, fmt.Errorf("parse failed (%s) on file: %s", err.Error(), "fmu.yaml")
	}
	for _, doc := range docs.([]kind.KindDoc) {
		slog.Info(fmt.Sprintf("kind: %s; name=%s (%s)", doc.Kind, doc.Metadata.Name, doc.File))
		if _, ok := index.DocMap[doc.Kind]; !ok {
			index.DocMap[doc.Kind] = []kind.KindDoc{}
		}
		index.DocMap[doc.Kind] = append(index.DocMap[doc.Kind], doc)
	}
	annotations, err := parseAnnotations(c.annotation)
	if err != nil {
		return nil, nil, err
	}
	fmiConfig := fmi.FmiConfig{
		Name:           c.name,
		UUID:           c.uuid,
		Version:        c.version,
		Description:    "Gateway to connect to the DSE Simbus",
		StartTime:      c.startTime,
		StopTime:       c.endTime,
		StepSize:       c.stepSize,
		GenerationTool: "DSE FMI - ModelC FMU",
		Annotations:    annotations,
	}
	switch c.fmiVersion {
	case "2":
		fmuXml := fmi2.ModelDescription{}
		fmiConfig.FmiVersion = "2"
		fmiConfig.ModelIdentifier = getModelIdentifier(c.modelIdentifier, "libfmi2gateway")
		if err := fmi2.SetGeneralFmuXmlFields(fmiConfig, &fmuXml); err != nil {
			return nil, nil, err
		}

		channels, err := c.buildFmi2XmlSignals(&fmuXml, index, envars)
		if err != nil {
			return nil, nil, err
		}

		return fmuXml, channels, nil
	case "3":
		fmuXml := fmi3.ModelDescription{}
		fmiConfig.FmiVersion = "3"
		fmiConfig.ModelIdentifier = getModelIdentifier(c.modelIdentifier, "libfmi3gateway")
		if err := fmi3.SetGeneralFmuXmlFields(fmiConfig, &fmuXml); err != nil {
			return nil, nil, err
		}

		channels, err := c.buildFmi3XmlSignals(&fmuXml, index, envars)
		if err != nil {
			return nil, nil, err
		}

		return fmuXml, channels, nil
	}
	return nil, nil, nil
}

func (c *GenFmiGatewayCommand) getEnvar(envarList interface{}, signalSpec *[]schemaKind.SignalGroupSpec, expectedType string) {
	for _, envar := range envarList.([]interface{}) {
		_type := envar.(schemaKind.Annotations)["type"]
		if _type == nil {
			_type = "real"
		}
		if _type != expectedType {
			continue
		}
		_name := envar.(schemaKind.Annotations)["name"].(string)
		_start := envar.(schemaKind.Annotations)["default"]
		var start string
		if _start != nil {
			start = _start.(string)
		} else {
			start = ""
		}
		signal := schemaKind.Signal{
			Signal: _name,
			Annotations: &schemaKind.Annotations{
				"fmi_variable_causality": "parameter",
				"fmi_variable_vref":      c.signalIdx,
				"fmi_variable_start":     start,
			},
		}
		switch expectedType {
		case "string":
			(*signalSpec)[0].Signals = append((*signalSpec)[0].Signals, signal)
		case "real":
			(*signalSpec)[1].Signals = append((*signalSpec)[1].Signals, signal)
		}
		c.signalIdx++
	}
}

func (c *GenFmiGatewayCommand) parseStack() (*[]schemaKind.SignalGroupSpec, error) {
	var signalSpec = make([]schemaKind.SignalGroupSpec, 2)
	// Load the Stack.
	stackFile, err := os.ReadFile(c.stack)
	if err != nil {
		return nil, fmt.Errorf("unable to read stack.yaml")
	}
	stack := schemaKind.Stack{}
	if err := yaml.Unmarshal(stackFile, &stack); err != nil {
		return nil, fmt.Errorf("unable to unmarshal stack.yaml")
	}
	for _, model := range *stack.Spec.Models {
		if model.Model.Name == "Gateway" {
			if stepSize := (*model.Annotations)["step_size"]; stepSize != nil {
				c.stepSize = stepSize.(float64)
			}
			if endTime := (*model.Annotations)["end_time"]; endTime != nil {
				c.endTime = endTime.(float64)
			}
			if startTime := (*model.Annotations)["fmi_start_time"]; startTime != nil {
				c.startTime = startTime.(float64)
			}

			if envarList := (*model.Annotations)["cmd_envvars"]; envarList != nil {
				c.getEnvar(envarList, &signalSpec, "real")
				c.getEnvar(envarList, &signalSpec, "string")
			}
		}
	}

	return &signalSpec, nil
}

func (c *GenFmiGatewayCommand) generateChannels(channelMap []string) ([]schemaKind.Channel, error) {
	channels := []schemaKind.Channel{}
	for i := range channelMap {
		channel := schemaKind.Channel{
			Alias: stringPtr(channelMap[i]),
			Selectors: &schemaKind.Labels{
				"channel": channelMap[i],
			},
		}
		channels = append(channels, channel)
	}
	return channels, nil
}

func (c *GenFmiGatewayCommand) generateModel(channelMap []string) error {

	// Build the model.xml
	model := schemaKind.Model{
		Kind: "Model",
		Metadata: &schemaKind.ObjectMetadata{
			Name: stringPtr("Gateway"),
		},
	}

	channels, err := c.generateChannels(channelMap)
	if err != nil {
		return fmt.Errorf("could not generate channels: (%v)", err)
	}
	gwSpec := schemaKind.GatewaySpec{}
	spec := schemaKind.ModelSpec{
		Channels: &channels,
		Runtime: &struct {
			Dynlib     *[]schemaKind.LibrarySpec    `yaml:"dynlib,omitempty"`
			Executable *[]schemaKind.ExecutableSpec `yaml:"executable,omitempty"`
			Gateway    *schemaKind.GatewaySpec      `yaml:"gateway,omitempty"`
			Mcl        *[]schemaKind.LibrarySpec    `yaml:"mcl,omitempty"`
		}{
			Gateway: &gwSpec,
		},
	}
	model.Spec = spec

	// Write the Model
	if err := os.MkdirAll(c.outdir, 0755); err != nil {
		return fmt.Errorf("error Creating Directory for Yaml output: %v", err)
	}

	modelYamlPath := filepath.Join(c.outdir, "model.yaml")
	fmt.Fprintf(flag.CommandLine.Output(), "Creating Model YAML: %s (%s)\n", *model.Metadata.Name, modelYamlPath)
	if err := writeYaml(&model, modelYamlPath, false); err != nil {
		return fmt.Errorf("error writing YAML: %v", err)
	}

	return nil
}

func parseAnnotations(annotationStr string) (map[string]string, error) {
	if annotationStr == "" || annotationStr == "{}" {
		return map[string]string{}, nil
	}

	var annotations map[string]string
	if err := json.Unmarshal([]byte(annotationStr), &annotations); err != nil {
		return nil, fmt.Errorf("failed to parse annotations: %w", err)
	}

	return annotations, nil
}
