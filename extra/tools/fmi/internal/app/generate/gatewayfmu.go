// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"encoding/csv"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"slices"
	"strings"

	"github.com/boschglobal/dse.clib/extra/go/command/log"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi3"
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
	parameters      string
	runtime         string
	simpath         string
	platform        string
	fmiPackage      string

	// Model parameters
	startTime float64
	endTime   float64
	stepSize  float64

	// Number of Signals processed so far, used for assigning unique vrefs.
	signalIdx int

	// Files structures to be written.
	signalGroupFile []schemaKind.SignalGroup
	modelFile       schemaKind.Model

	// Signal groups for FMU XML generation
	stringParams schemaKind.SignalGroupSpec
	realParams   schemaKind.SignalGroupSpec

	// Annotations map: runtime entries + user --annotations, merged before XML generation.
	annotations map[string]string

	libRootPath string
}

// NewFmiGatewayCommand creates a new FMI Gateway command instance.
func NewFmiGatewayCommand(name string) *GenFmiGatewayCommand {
	c := &GenFmiGatewayCommand{
		commandName: name,
		fs:          flag.NewFlagSet(name, flag.ExitOnError),
		// Default values
		startTime: 0.0,
		endTime:   999999.0,
		stepSize:  0.0005,
		signalIdx: 0,
	}

	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")

	/* Required. */
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "A list of signalgroups separated by comma")

	/* Optional. */
	c.fs.StringVar(&c.version, "version", "0.0.1", "Version to assign to the FMU")
	c.fs.StringVar(&c.fmiVersion, "fmiVersion", "2", "Modelica FMI Version")
	c.fs.StringVar(&c.name, "name", "fmugw", "Name of the FMU")
	c.fs.StringVar(&c.runtime, "runtime", "legacy", "Runtime type for the gateway")
	c.fs.StringVar(&c.platform, "platform", "linux-amd64", "Platform of FMU to generate")
	c.fs.StringVar(&c.simpath, "sim", "/sim", "Path to simulation (Simer layout)")
	c.fs.StringVar(&c.outdir, "outdir", "/out", "Output directory for the FMU file")
	c.fs.StringVar(&c.annotation, "annotations", "{}", "Annotations in dictionary format")
	c.fs.StringVar(&c.parameters, "parameters", "", "Path to a CSV file with parameters (name,type,default)")
	c.fs.StringVar(&c.modelIdentifier, "modelidentifier", "", "ModelIdentifier field in the FMU modelDescription.xml")
	c.fs.StringVar(&c.uuid, "uuid", "", "UUID to assign to the FMU, set to '' to generate a new UUID")
	c.fs.StringVar(&c.stack, "stack", "", "[Legacy] Path to the stack.yaml")
	c.fs.StringVar(&c.fmiPackage, "fmiPackage", "", "Fmi Package path")

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
	if filepath.Clean(c.simpath) == filepath.Clean(c.outdir) {
		return fmt.Errorf("simpath and outdir must not be the same path (%s)", c.outdir)
	}
	if len(c.uuid) == 0 {
		c.uuid = uuid.New().String()
	}
	if len(c.fmiPackage) == 0 {
		c.fmiPackage = filepath.Join("/package", c.platform, "fmigateway")
	}
	if len(c.modelIdentifier) == 0 {
		c.modelIdentifier = fmt.Sprintf("fmi%sgateway", c.fmiVersion)
	}

	if err := c.createRuntime(); err != nil {
		return err
	}

	/* Legacy Support. */
	if err := c.parseStack(); err != nil {
		return err
	}

	if err := c.parseParametersCsv(); err != nil {
		return err
	}

	if err := c.mergeSignalGroups(); err != nil {
		return err
	}

	fmuXml, channels, err := c.createFmuXml()
	if err != nil {
		return err
	}

	if err := c.generateModel(channels); err != nil {
		return err
	}

	if err := os.MkdirAll(c.outdir, os.ModePerm); err != nil {
		return fmt.Errorf("could not create output directory (%v)", err)
	}

	if c.runtime == "simer" {
		return c.buildFmuPackage(fmuXml)
	}

	/* LEGACY SUPPORT: generate modelDescription.xml and model.yaml only, without packaging as FMU. */
	modelYamlPath := filepath.Join(c.outdir, "model.yaml")
	fmuModelDescriptionFilename := filepath.Join(c.outdir, "modelDescription.xml")
	fmuYaml := filepath.Join(c.outdir, "fmu.yaml")
	_ = os.Remove(fmuYaml)

	// Write the files.
	for _, sg := range c.signalGroupFile {
		slog.Info("Signal Group", "name", *sg.Metadata.Name, "signals", len(sg.Spec.Signals))
		if err := writeYaml(&sg, fmuYaml, true); err != nil {
			return err
		}
	}

	slog.Info("Creating Model YAML", "name", *c.modelFile.Metadata.Name, "path", modelYamlPath)
	if err := writeYaml(&c.modelFile, modelYamlPath, false); err != nil {
		return fmt.Errorf("error writing YAML: %v", err)
	}

	if err := writeXml(fmuXml, fmuModelDescriptionFilename); err != nil {
		return fmt.Errorf("could not generate the FMU Model Description (%v)", err)
	}

	return nil
}

func (c *GenFmiGatewayCommand) buildFmuPackage(fmuXml interface{}) error {
	gwFilePath := filepath.Join(c.simpath, "model", c.name, "data")
	if err := os.MkdirAll(gwFilePath, os.ModePerm); err != nil {
		return err
	}
	// Build the FMU file layout in a temporary directory.
	fmuOutDir := filepath.Join(c.outdir, c.name)
	if err := os.MkdirAll(fmuOutDir, os.ModePerm); err != nil {
		return fmt.Errorf("could not create temporary directory: %v", err)
	}

	slog.Info("Build the FMU file layout", "dir", fmuOutDir)
	fmuBinDir := fmi.GetFmuBinaryDirName(c.platform, c.fmiVersion)
	if fmuBinDir == "" {
		return fmt.Errorf("platform not supported (%s)", c.platform)
	}

	fmuBinPath := filepath.Join(fmuOutDir, "binaries", fmuBinDir)
	if err := os.MkdirAll(fmuBinPath, os.ModePerm); err != nil {
		return fmt.Errorf("could not create FMU binaries directory (%v)", err)
	}
	// Copy the necessary binaries from the FMI package.
	// libfmigateway.so => fmigateway.so
	src := fmi.GetFmuLibPath(c.fmiPackage, fmt.Sprintf("lib%s", c.modelIdentifier), c.platform)
	if _, err := os.Stat(src); err != nil {
		// Also try without "lib" prefix.
		src = fmi.GetFmuLibPath(c.fmiPackage, c.modelIdentifier, c.platform)
	}
	tgt := fmi.GetFmuBinPath(fmuBinPath, c.modelIdentifier, c.platform)
	if err := operations.Copy(src, tgt); err != nil {
		return fmt.Errorf("could not copy FMU binary (%v)", err)
	}

	// For Windows, also copy the modelc binary.
	osName, _, found := strings.Cut(c.platform, "-")
	if found && osName == "windows" {
		src = fmi.GetFmuLibPath(c.fmiPackage, "../../bin/libmodelc", c.platform)
		tgt = fmi.GetFmuBinPath(fmuBinPath, "libmodelc", c.platform)
		if err := operations.Copy(src, tgt); err != nil {
			return fmt.Errorf("could not copy modelc binary (%v)", err)
		}
	}

	modelYamlPath := filepath.Join(gwFilePath, "model.yaml")
	fmuYaml := filepath.Join(gwFilePath, "fmu.yaml")
	_ = os.Remove(fmuYaml)

	// Write the files.
	for _, sg := range c.signalGroupFile {
		slog.Info("Signal Group", "name", *sg.Metadata.Name, "signals", len(sg.Spec.Signals))
		if err := writeYaml(&sg, fmuYaml, true); err != nil {
			return err
		}
	}

	slog.Info("Creating Model YAML", "name", *c.modelFile.Metadata.Name, "path", modelYamlPath)
	if err := writeYaml(&c.modelFile, modelYamlPath, false); err != nil {
		return fmt.Errorf("error writing YAML: %v", err)
	}

	// /sim => resources/sim
	if err := operations.CopyDirectory(c.simpath, filepath.Join(fmuOutDir, "resources", "sim")); err != nil {
		return fmt.Errorf("could not copy FMU resources (%v)", err)
	}
	// Construct the FMU Model Description.
	fmuModelDescriptionFilename := filepath.Join(fmuOutDir, "modelDescription.xml")
	slog.Info("Create FMU Model Description", "path", fmuModelDescriptionFilename)
	if err := writeXml(fmuXml, fmuModelDescriptionFilename); err != nil {
		return fmt.Errorf("could not generate the FMU Model Description (%v)", err)
	}

	// Produce the FMU.
	fmuFilename := filepath.Join(c.outdir, c.name) + ".fmu"
	slog.Info("Create FMU Package", "path", fmuFilename)
	if err := operations.Zip(fmuOutDir, fmuFilename); err != nil {
		return fmt.Errorf("could not create FMU (%v)", err)
	}

	return nil
}

func (c *GenFmiGatewayCommand) createRuntime() error {

	c.annotations = map[string]string{
		"dse.fmi.gateway.runtime": c.runtime,
	}

	switch c.runtime {
	case "simer":
		// Add built-in gateway parameters.
		c.realParams.Signals = append(c.realParams.Signals, schemaKind.Signal{
			Signal: "Simer_Command_Selector",
			Annotations: &schemaKind.Annotations{
				"fmi_variable_causality": "parameter",
				"fmi_variable_vref":      c.signalIdx,
				"fmi_annotations": map[string]interface{}{
					"dse.fmi.gateway.simer.parameter": nil,
				},
			},
		})
		c.signalIdx++
		c.stringParams.Signals = append(c.stringParams.Signals, schemaKind.Signal{
			Signal: "Simer_Command",
			Annotations: &schemaKind.Annotations{
				"fmi_variable_causality": "parameter",
				"fmi_variable_vref":      c.signalIdx,
				"fmi_annotations": map[string]interface{}{
					"dse.fmi.gateway.simer.parameter": nil,
				},
			},
		})
		c.signalIdx++

		c.annotations["dse.fmi.gateway.simer.cmd.0"] = "-logger 6 -endtime 9999999"

	default:
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
		slog.Info("Patch Signals", "signal", s.Signal)
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

func (c *GenFmiGatewayCommand) mergeSignalGroups() error {
	signalGroupList := []string{}
	if len(c.signalGroups) > 0 {
		signalGroupList = strings.Split(c.signalGroups, ",")
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
		if sg.Metadata.Labels == nil {
			labels := schemaKind.Labels{}
			sg.Metadata.Labels = &labels
		}
		(*sg.Metadata.Labels)["model"] = c.name

		signals := sg.Spec.Signals

		sg.Spec.Signals, err = c.patchSignal(&c.signalIdx, sgType, signals, &signalList)
		if err != nil {
			return err
		}

		c.signalGroupFile = append(c.signalGroupFile, sg)
	}
	return nil
}

func (c *GenFmiGatewayCommand) buildFmi2XmlSignals(
	fmuXml *fmi2.ModelDescription) ([]string, error) {

	if err := fmi2.ScalarSignal(fmuXml, c.realParams); err != nil {
		slog.Warn("could not set envars in xml", "error", err)
	}
	if err := fmi2.StringSignal(fmuXml, c.stringParams); err != nil {
		slog.Warn("could not set envars in xml", "error", err)
	}

	channels, err := fmi2.VariablesFromSignalGroupObject(fmuXml, c.signalGroupFile)
	if err != nil {
		return nil, err
	}

	return channels, nil
}

func (c *GenFmiGatewayCommand) buildFmi3XmlSignals(
	fmuXml *fmi3.ModelDescription) ([]string, error) {

	if err := fmi3.ScalarSignal(c.realParams, fmuXml); err != nil {
		slog.Warn("could not set envars in xml", "error", err)
	}
	if err := fmi3.StringSignal(c.stringParams, fmuXml); err != nil {
		slog.Warn("could not set envars in xml", "error", err)
	}

	channels, err := fmi3.VariablesFromSignalGroupObject(fmuXml, c.signalGroupFile)
	if err != nil {
		return nil, err
	}
	return channels, nil
}

func (c *GenFmiGatewayCommand) createFmuXml() (interface{}, []string, error) {

	userAnnotations, err := parseAnnotations(c.annotation)
	if err != nil {
		return nil, nil, err
	}

	for k, v := range userAnnotations {
		c.annotations[k] = v
	}

	fmiConfig := fmi.FmiConfig{
		Name:           c.name,
		UUID:           c.uuid,
		Version:        c.version,
		Description:    "Gateway to connect to the DSE Co-Simulation",
		StartTime:      c.startTime,
		StopTime:       c.endTime,
		StepSize:       c.stepSize,
		GenerationTool: "DSE FMI - GATEWAY FMU",
		Annotations:    c.annotations,
	}

	switch c.fmiVersion {
	case "2":
		fmuXml := fmi2.ModelDescription{}
		fmiConfig.FmiVersion = "2"
		fmiConfig.ModelIdentifier = c.modelIdentifier
		if err := fmi2.SetGeneralFmuXmlFields(fmiConfig, &fmuXml); err != nil {
			return nil, nil, err
		}

		channels, err := c.buildFmi2XmlSignals(&fmuXml)
		if err != nil {
			return nil, nil, err
		}

		return fmuXml, channels, nil
	case "3":
		fmuXml := fmi3.ModelDescription{}
		fmiConfig.FmiVersion = "3"
		fmiConfig.ModelIdentifier = c.modelIdentifier
		if err := fmi3.SetGeneralFmuXmlFields(fmiConfig, &fmuXml); err != nil {
			return nil, nil, err
		}

		channels, err := c.buildFmi3XmlSignals(&fmuXml)
		if err != nil {
			return nil, nil, err
		}

		return fmuXml, channels, nil
	default:
		return nil, nil, fmt.Errorf("unsupported FMI version: %q", c.fmiVersion)
	}
}

func (c *GenFmiGatewayCommand) getEnvar(envarList interface{}, expectedType string) {
	for _, envar := range envarList.([]interface{}) {
		anns, ok := envar.(schemaKind.Annotations)
		if !ok {
			continue
		}
		_type := anns["type"]
		if _type == nil {
			_type = "real"
		}
		if _type != expectedType {
			continue
		}
		_name, ok := anns["name"].(string)
		if !ok || _name == "" {
			continue
		}
		var start string
		if _start, ok := anns["default"].(string); ok {
			start = _start
		}
		signal := schemaKind.Signal{
			Signal: _name,
			Annotations: &schemaKind.Annotations{
				"fmi_variable_causality": "parameter",
				"fmi_variable_vref":      c.signalIdx,
				"fmi_variable_start":     start,
				"fmi_annotations": map[string]interface{}{
					"dse.fmi.gateway.script.parameter": nil,
				},
			},
		}
		switch expectedType {
		case "string":
			c.stringParams.Signals = append(c.stringParams.Signals, signal)
		case "real":
			c.realParams.Signals = append(c.realParams.Signals, signal)
		}
		c.signalIdx++
	}
}

// parseParametersCsv parses the --parameters CSV file (name,type,default) and
// appends each entry directly to c.realParams or c.stringParams.
// type must be "real" or "string" (case-insensitive); default may be empty.
func (c *GenFmiGatewayCommand) parseParametersCsv() error {
	if c.parameters == "" {
		return nil
	}

	data, err := os.ReadFile(c.parameters)
	if err != nil {
		return fmt.Errorf("parseParametersCsv: could not read file %q: %w", c.parameters, err)
	}

	r := csv.NewReader(strings.NewReader(string(data)))
	r.FieldsPerRecord = -1
	records, err := r.ReadAll()
	if err != nil {
		return fmt.Errorf("parseParametersCsv: could not parse CSV: %w", err)
	}

	for _, record := range records {
		if len(record) < 2 {
			return fmt.Errorf("parseParametersCsv: malformed record (expected name,type[,default]): %v", record)
		}
		paramType := strings.ToLower(strings.TrimSpace(record[1]))
		if paramType == "type" {
			continue // skip header row
		}
		if paramType != "real" && paramType != "string" {
			return fmt.Errorf("parseParametersCsv: unknown type %q for parameter %q", paramType, record[0])
		}
		defaultValue := ""
		if len(record) >= 3 {
			defaultValue = strings.TrimSpace(record[2])
		}
		signal := schemaKind.Signal{
			Signal: strings.TrimSpace(record[0]),
			Annotations: &schemaKind.Annotations{
				"fmi_variable_causality": "parameter",
				"fmi_variable_vref":      c.signalIdx,
				"fmi_variable_start":     defaultValue,
				"fmi_annotations": map[string]interface{}{
					"dse.fmi.gateway.script.parameter": nil,
				},
			},
		}
		switch paramType {
		case "string":
			c.stringParams.Signals = append(c.stringParams.Signals, signal)
		case "real":
			c.realParams.Signals = append(c.realParams.Signals, signal)
		}
		c.signalIdx++
	}
	return nil
}

func (c *GenFmiGatewayCommand) parseStack() error {
	stackFile, err := os.ReadFile(c.stack)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil
		}
		return fmt.Errorf("unable to read stack file: %w", err)
	}
	stack := schemaKind.Stack{}
	if err := yaml.Unmarshal(stackFile, &stack); err != nil {
		return fmt.Errorf("unable to unmarshal stack.yaml")
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
				c.getEnvar(envarList, "real")
				c.getEnvar(envarList, "string")
			}
		}
	}
	return nil
}

func (c *GenFmiGatewayCommand) generateChannels(channelMap []string) []schemaKind.Channel {
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
	return channels
}

func (c *GenFmiGatewayCommand) generateModel(channelMap []string) error {

	// Build the model.xml
	c.modelFile = schemaKind.Model{
		Kind: "Model",
		Metadata: &schemaKind.ObjectMetadata{
			Name: stringPtr("gateway"),
		},
	}

	slog.Info("channels", "list", channelMap)
	channels := c.generateChannels(channelMap)

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
	c.modelFile.Spec = spec

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
