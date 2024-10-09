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
	"strings"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type FmiMclCommand struct {
	commandName string
	fs          *flag.FlagSet

	fmupath  string
	mcl      string
	platform string
	channels string
	outdir   string
	logLevel int
}

type channel []struct {
	Alias    string            `json:"alias"`
	Name     string            `json:"name"`
	Selector map[string]string `json:"selector"`
}

func NewFmiMclCommand(name string) *FmiMclCommand {
	c := &FmiMclCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.fmupath, "fmu", "", "Path to FMU")
	c.fs.StringVar(&c.mcl, "mcl", "", "Path to the FMI MCL library")
	c.fs.StringVar(&c.platform, "platform", "linux-amd64", "Platform of Model to generate")
	c.fs.StringVar(&c.channels, "channels", `[{"alias": "fmu_channel", "selector":{"channel":"fmu_channel"}}]`, "Channels to be generated")
	c.fs.StringVar(&c.outdir, "outdir", "./", "Output directory for the Model files")
	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")

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

	fmuModelDescriptionFilename := filepath.Join(c.fmupath, "modelDescription.xml")
	fmt.Fprintf(flag.CommandLine.Output(), "Reading FMU Desciption (%s)\n", fmuModelDescriptionFilename)
	h := fmi2.XmlFmuHandler{}
	var fmiMD *fmi2.FmiModelDescription
	if fmiMD = h.Detect(fmuModelDescriptionFilename); fmiMD == nil {
		return fmt.Errorf("Could not read FMU Model Description file!")
	}
	if err := c.generateModel(*fmiMD); err != nil {
		return err
	}
	return nil
}

func _generateChannels(channelJson string) ([]kind.Channel, error) {
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

func (c *FmiMclCommand) getFmuBinaryPath(modelIdentifier string) string {
	dir := "linux64"
	extension := "so"
	os, arch, found := strings.Cut(c.platform, "-")
	if found {
		switch os {
		case "linux":
			switch arch {
			case "amd64":
				dir = "linux64"
			case "x86", "i386":
				dir = "linux32"
			}
		case "windows":
			extension = "dll"
			switch arch {
			case "x64":
				dir = "win64"
			case "x86":
				dir = "win32"
			}
		}
	}
	return filepath.Join(c.fmupath, "binaries", dir, fmt.Sprintf("%s.%s", modelIdentifier, extension))
}

func (c *FmiMclCommand) generateModel(fmiMD fmi2.FmiModelDescription) error {
	// Construct various parameters.
	platformOs, platformArch, found := strings.Cut(c.platform, "-")
	if !found {
		return fmt.Errorf("Could not decode platform: (%s)", c.platform)
	}
	fmuResourceDir := filepath.Join(c.fmupath, "resources")

	// Build the model.xml
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
				"fmi_resource_dir":  fmuResourceDir,
			},
		},
	}
	mcl := []kind.LibrarySpec{
		{
			Arch: stringPtr(platformArch),
			Os:   stringPtr(platformOs),
			Path: c.getFmuBinaryPath(fmiMD.CoSimulation.ModelIdentifier),
		},
	}
	dynlib := []kind.LibrarySpec{
		{Arch: stringPtr(platformArch), Os: stringPtr(platformOs), Path: c.mcl},
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
	if err := os.MkdirAll(c.outdir, 0755); err != nil {
		return fmt.Errorf("Error Creating Directory for Yaml output: %v", err)
	}
	modelYamlPath := filepath.Join(c.outdir, "model.yaml")
	fmt.Fprintf(flag.CommandLine.Output(), "Creating Model YAML: %s (%s)\n", *model.Metadata.Name, modelYamlPath)
	if err := writeYaml(&model, modelYamlPath, false); err != nil {
		return fmt.Errorf("Error writing YAML: %v", err)
	}

	return nil
}
