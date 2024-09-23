// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type FmiMclCommand struct {
	commandName string
	fs          *flag.FlagSet

	inputFile  string
	outputFile string
	channels   string
	arch       string
	os         string
	fmu        string
	dynlib     string
	resource   string
}

type channel []struct {
	Alias    string            `json:"alias"`
	Name     string            `json:"name"`
	Selector map[string]string `json:"selector"`
}

func NewFmiMclCommand(name string) *FmiMclCommand {
	c := &FmiMclCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.inputFile, "input", "", "modelDescription to be converted")
	c.fs.StringVar(&c.outputFile, "output", "", "Model.yaml to be generated")
	c.fs.StringVar(&c.channels, "channels", "", "[optional] channels to be generated")
	c.fs.StringVar(&c.arch, "arch", "", "Architecture of the binaries")
	c.fs.StringVar(&c.os, "os", "", "Operating system of the binaries")
	c.fs.StringVar(&c.fmu, "fmu", "", "relative path to FMU binary")
	c.fs.StringVar(&c.dynlib, "dynlib", "", "relative path to the FMIMCL binary")
	c.fs.StringVar(&c.resource, "resource", "./", "path to the resource dir")
	return c
}

func (c FmiMclCommand) Name() string {
	return c.commandName
}

func (c FmiMclCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *FmiMclCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *FmiMclCommand) Run() error {
	fmt.Fprintf(flag.CommandLine.Output(), "Reading file: %s\n", c.inputFile)
	h := fmi2.XmlFmuHandler{}
	var fmiMD *fmi2.FmiModelDescription
	if fmiMD = h.Detect(c.inputFile); fmiMD == nil {
		return fmt.Errorf("Could not read input file!")
	}
	if err := c.generateModel(*fmiMD); err != nil {
		return err
	}
	return nil
}

func _generateChannels(channelJson string) ([]kind.Channel, error) {
	/* Default. */
	if channelJson == "" {
		channels := make([]kind.Channel, 1)
		channels[0].Alias = stringPtr("fmu_channel")
		channels[0].Selectors = &kind.Labels{
			"channel": "fmu_vector",
		}

		return channels, nil
	}

	/* Customized channels and selectors. */
	data := channel{}
	if err := json.Unmarshal([]byte(channelJson), &data); err != nil {
		return nil, err
	}

	channels := make([]kind.Channel, len(data))
	for i := range data {
		channels[i].Name = &data[i].Name
		channels[i].Alias = &data[i].Alias
		selectors := kind.Labels{}
		for k, v := range data[i].Selector {
			selectors[k] = v
		}
		channels[i].Selectors = &selectors
	}
	return channels, nil
}

func (c *FmiMclCommand) generateModel(fmiMD fmi2.FmiModelDescription) error {

	nodeExists := func(Node any) string {
		if Node != nil {
			return "true"
		}
		return "false"
	}

	model := kind.Model{
		Kind: "Model",
		Metadata: &kind.ObjectMetadata{
			Name: &fmiMD.ModelName,
			Annotations: &kind.Annotations{
				"mcl_adapter":       "fmi",
				"mcl_version":       fmiMD.FmiVersion,
				"fmi_model_cosim":   nodeExists(fmiMD.CoSimulation),
				"fmi_model_version": fmiMD.Version,
				"fmi_stepsize":      fmiMD.DefaultExperiment.StepSize,
				"fmi_guid":          fmiMD.Guid,
				"fmi_resource_dir":  c.resource,
			},
		},
	}

	mcl := []kind.LibrarySpec{
		{
			Arch: stringPtr(c.arch),
			Os:   stringPtr(c.os),
			Path: c.fmu,
		},
	}

	dynlib := []kind.LibrarySpec{
		{Arch: stringPtr(c.arch), Os: stringPtr(c.os), Path: c.dynlib},
	}

	channels, err := _generateChannels(c.channels)
	if err != nil {
		return fmt.Errorf("Could not generate channels: (%v)", err)
	}

	spec := kind.ModelSpec{
		Channels: &channels,
		Runtime: &struct {
			Dynlib     *[]kind.LibrarySpec    `yaml:"dynlib,omitempty"`
			Executable *[]kind.ExecutableSpec `yaml:"executable,omitempty"`
			Gateway    *kind.GatewaySpec      `yaml:"gateway,omitempty"`
			Mcl        *[]kind.LibrarySpec    `yaml:"mcl,omitempty"`
		}{
			Dynlib: &dynlib,
			Mcl:    &mcl,
		},
	}
	model.Spec = spec

	// Write the Model
	fmt.Fprintf(flag.CommandLine.Output(), "Creating file: %s\n", c.outputFile)
	if err := os.MkdirAll(filepath.Dir(c.outputFile), 0755); err != nil {
		return fmt.Errorf("Error Creating Directory for Yaml output: %v", err)
	}

	if err := writeYaml(&model, c.outputFile, false); err != nil {
		return fmt.Errorf("Error writing YAML: %v", err)
	}

	return nil
}
