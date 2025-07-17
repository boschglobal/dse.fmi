// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type FmiMclCommand struct {
	commandName string
	fs          *flag.FlagSet

	fmupath  string
	mcl      string
	platform string
	outdir   string
	logLevel int
}

func NewFmiMclCommand(name string) *FmiMclCommand {
	c := &FmiMclCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.fmupath, "fmu", "", "Path to FMU")
	c.fs.StringVar(&c.mcl, "mcl", "", "Path to the FMI MCL library")
	c.fs.StringVar(&c.platform, "platform", "linux-amd64", "Platform of Model to generate")
	c.fs.StringVar(&c.outdir, "outdir", "out", "Output directory for the Model files")
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
	var fmiMD *fmi2.ModelDescription
	if fmiMD = h.Detect(fmuModelDescriptionFilename); fmiMD == nil {
		return fmt.Errorf("could not read FMU Model Description file")
	}
	if err := c.generateModel(*fmiMD); err != nil {
		return err
	}
	return nil
}

func _generateChannels(fmiMD fmi2.ModelDescription) ([]kind.Channel, error) {
	channels := []kind.Channel{
		{
			Alias: stringPtr("signal_channel"),
			Selectors: &kind.Labels{
				"model":   fmiMD.ModelName,
				"channel": "signal_vector",
			},
		},
	}
	binarySignalCount := func() int {
		count := 0
		for _, s := range fmiMD.ModelVariables.ScalarVariable {
			if s.String != nil {
				count++
			}
		}
		return count
	}()
	if binarySignalCount > 0 {
		channels = append(channels,
			kind.Channel{
				Alias: stringPtr("network_channel"),
				Selectors: &kind.Labels{
					"model":   fmiMD.ModelName,
					"channel": "network_vector",
				},
			},
		)
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

func (c *FmiMclCommand) generateModel(fmiMD fmi2.ModelDescription) error {
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
	channels, err := _generateChannels(fmiMD)
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
