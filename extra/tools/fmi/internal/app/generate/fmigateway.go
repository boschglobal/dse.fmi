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
	"time"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/handler"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/handler/kind"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/index"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/log"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/google/uuid"
	"gopkg.in/yaml.v3"
)

type GenFmiGatewayCommand struct {
	commandName string
	fs          *flag.FlagSet

	logLevel     int
	signalGroups string
	uuid         string
	version      string
	fmiVersion   string
	outdir       string
	session      string

	libRootPath string
}

func NewFmiGatewayCommand(name string) *GenFmiGatewayCommand {
	c := &GenFmiGatewayCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}

	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "A list of signalgroups separated by comma")
	c.fs.StringVar(&c.uuid, "uuid", "", "UUID to assign to the FMU, set to '' to generate a new UUID")
	c.fs.StringVar(&c.version, "version", "0.0.1", "Version to assign to the FMU")
	c.fs.StringVar(&c.fmiVersion, "fmiVersion", "2", "Modelica FMI Version")
	c.fs.StringVar(&c.outdir, "outdir", "out", "Output directory for the FMU file")
	c.fs.StringVar(&c.session, "session", "", "Yaml file containing the session annotation")

	// Supports unit testing.
	c.fs.StringVar(&c.libRootPath, "libroot", "/usr/local", "System lib root path (where lib & lib32 directory are found)")

	return c
}

func (c GenFmiGatewayCommand) Name() string {
	return c.commandName
}

func (c GenFmiGatewayCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *GenFmiGatewayCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *GenFmiGatewayCommand) Run() error {
	slog.SetDefault(log.NewLogger(c.logLevel))
	if len(c.uuid) == 0 {
		c.uuid = uuid.New().String()
	}

	if err := os.MkdirAll(c.outdir, os.ModePerm); err != nil {
		return err
	}
	if err := c.mergeSignalGroups(c.signalGroups, c.outdir); err != nil {
		return err
	}
	fmuXml, channels, err := c.createFmuXml()
	if err != nil {
		return err
	}
	if err := c.generateModel(*fmuXml, channels); err != nil {
		return err
	}
	fmuModelDescriptionFilename := filepath.Join(c.outdir, "modelDescription.xml")
	if err := writeXml(fmuXml, fmuModelDescriptionFilename); err != nil {
		return fmt.Errorf("could not generate the FMU Model Description (%v)", err)
	}
	return nil
}

func (c *GenFmiGatewayCommand) patchSignal(vr *int, sg_type string, signals []schema_kind.Signal, used_signals *[]string) ([]schema_kind.Signal, error) {
	var signal_list []schema_kind.Signal
	if sg_type == "string" {
		return signals, nil
	}
	for _, s := range signals {
		if slices.Contains(*used_signals, s.Signal) {
			continue
		}
		*used_signals = append(*used_signals, s.Signal)
		slog.Info(fmt.Sprintf("Patch Signals: %s", s.Signal))
		causality := ""
		if (*s.Annotations)["softecu_direction"] != nil {
			switch (*s.Annotations)["softecu_direction"] {
			case "M2E":
				{
					causality = "input"
					break
				}
			case "E2M":
				{
					causality = "output"
					break
				}
			default:
				break
			}
		}
		if (*s.Annotations)["direction"] != nil {
			causality = ((*s.Annotations)["direction"]).(string)
		}
		if (*s.Annotations)["fmi_variable_causality"] != nil {
			causality = ((*s.Annotations)["fmi_variable_causality"]).(string)
		}

		annotations := schema_kind.Annotations{
			"fmi_variable_causality": causality,
			"fmi_variable_vref":      *vr,
			"fmi_variable_type":      sg_type,
			"fmi_variable_name":      s.Signal,
		}
		*s.Annotations = annotations
		signal_list = append(signal_list, s)
		*vr++
	}
	return signal_list, nil
}

func (c *GenFmiGatewayCommand) mergeSignalGroups(signalGroups string, outDir string) error {
	signalGroupList := []string{}
	if len(signalGroups) > 0 {
		signalGroupList = strings.Split(signalGroups, ",")
	}
	vr := 0
	var signal_list []string
	for _, signalGroup := range signalGroupList {

		// Load the Signal Group.
		inputYaml, err := os.ReadFile(signalGroup)
		if err != nil {
			return fmt.Errorf("unable to read input file: %s (%w)", signalGroup, err)
		}
		sg := schema_kind.SignalGroup{}
		if err := yaml.Unmarshal(inputYaml, &sg); err != nil {
			return fmt.Errorf("unable to unmarshal signalgroup yaml: %s (%w)", signalGroup, err)
		}
		sg_type := "real"
		if sg.Metadata.Annotations != nil {
			if (*sg.Metadata.Annotations)["vector_type"] != nil {
				if (*sg.Metadata.Annotations)["vector_type"] == "binary" {
					sg_type = "string"
				}
			}
		}

		signals := sg.Spec.Signals

		sg.Spec.Signals, err = c.patchSignal(&vr, sg_type, signals, &signal_list)
		if err != nil {
			return err
		}

		if err := writeYaml(&sg, filepath.Join(outDir, "fmu.yaml"), true); err != nil {
			return err
		}
	}
	return nil
}

func (c *GenFmiGatewayCommand) setGeneralXmlFields(fmuXml *fmi2.FmiModelDescription) {
	// Basic FMU Information.
	fmuXml.ModelName = "gateway"
	fmuXml.FmiVersion = c.fmiVersion
	fmuXml.Guid = fmt.Sprintf("{%s}", c.uuid)
	fmuXml.GenerationTool = "DSE FMI - FMU Gateway"
	fmuXml.GenerationDateAndTime = time.Now().String()
	fmuXml.Author = "Robert Bosch GmbH"
	fmuXml.Version = c.version
	fmuXml.Description = "Gateway to connect to the DSE Simbus"

	// Runtime Information.
	fmuXml.DefaultExperiment.StartTime = "0.0"
	fmuXml.DefaultExperiment.StopTime = "9999"
	fmuXml.DefaultExperiment.StepSize = "0.0005"

	// Model Information.
	fmuXml.CoSimulation.ModelIdentifier = fmt.Sprintf("libfmi%sgateway", c.fmiVersion) // This is the packaged SO/DLL file.
	fmuXml.CoSimulation.CanHandleVariableCommunicationStepSize = "true"
	fmuXml.CoSimulation.CanInterpolateInputs = "true"
}

func (c *GenFmiGatewayCommand) buildXmlSignals(fmuXml *fmi2.FmiModelDescription, index *index.YamlFileIndex) ([]string, error) {
	var channels []string
	if signalGroupDocs, ok := index.DocMap["SignalGroup"]; ok {
		for _, doc := range signalGroupDocs {
			fmt.Fprintf(flag.CommandLine.Output(), "Adding SignalGroup %s to %s\n", doc.Metadata.Name, doc.File)
			signalGroupSpec := doc.Spec.(*schema_kind.SignalGroupSpec)
			if doc.Metadata.Annotations["vector_type"] == "binary" {
				if err := fmi2.BinarySignal(*signalGroupSpec, fmuXml); err != nil {
					slog.Warn(fmt.Sprintf("Skipped BinarySignal (Error: %s)", err.Error()))
				}
			} else {
				if err := fmi2.ScalarSignal(*signalGroupSpec, fmuXml); err != nil {
					slog.Debug(fmt.Sprintf("Skipped Scalarsignal (Error: %s)", err.Error()))
				}
			}
			if !slices.Contains(channels, doc.Metadata.Labels["channel"]) {
				channels = append(channels, doc.Metadata.Labels["channel"])
				slog.Debug(fmt.Sprintf("added %s", doc.Metadata.Labels["channel"]))
			}
		}
	}
	return channels, nil
}

func (c *GenFmiGatewayCommand) createFmuXml() (*fmi2.FmiModelDescription, []string, error) {
	fmuXml := fmi2.FmiModelDescription{}
	c.setGeneralXmlFields(&fmuXml)

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
	channels, err := c.buildXmlSignals(&fmuXml, index)
	if err != nil {
		return nil, nil, err
	}
	return &fmuXml, channels, nil
}

func (c *GenFmiGatewayCommand) generateChannels(channel_map []string) ([]schema_kind.Channel, error) {
	channels := []schema_kind.Channel{}
	for i := range channel_map {
		channel := schema_kind.Channel{
			Alias: stringPtr(channel_map[i]),
			Selectors: &schema_kind.Labels{
				"channel": channel_map[i],
			},
		}
		channels = append(channels, channel)
	}
	return channels, nil
}

func (c *GenFmiGatewayCommand) generateModel(fmiMD fmi2.FmiModelDescription, channel_map []string) error {

	// Build the model.xml
	nodeExists := func(Node any) string {
		if Node != nil {
			return "true"
		}
		return "false"
	}

	model := schema_kind.Model{
		Kind: "Model",
		Metadata: &schema_kind.ObjectMetadata{
			Name: &fmiMD.ModelName,
			Annotations: &schema_kind.Annotations{
				"fmi_version":      fmiMD.FmiVersion,
				"fmi_model_cosim":  nodeExists(fmiMD.CoSimulation),
				"fmi_stepsize":     fmiMD.DefaultExperiment.StepSize,
				"fmi_endtime":      fmiMD.DefaultExperiment.StopTime,
				"fmi_guid":         fmiMD.Guid,
				"fmi_resource_dir": "resources",
				"loglevel":         4,
			},
		},
	}

	if len(c.session) != 0 {
		inputYaml, err := os.ReadFile(c.session)
		if err != nil {
			return fmt.Errorf("unable to read input file: %s (%w)", "session.yaml", err)
		}
		ann := schema_kind.ObjectMetadata{}
		if err := yaml.Unmarshal(inputYaml, &ann); err != nil {
			return fmt.Errorf("unable to unmarshal signalgroup yaml: %s (%w)", "session.yaml", err)
		}
		(*model.Metadata.Annotations)["session"] = (*ann.Annotations)["session"]
	}

	channels, err := c.generateChannels(channel_map)
	if err != nil {
		return fmt.Errorf("could not generate channels: (%v)", err)
	}
	spec := schema_kind.ModelSpec{
		Channels: &channels,
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
