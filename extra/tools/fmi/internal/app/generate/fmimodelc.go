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
	"strings"

	"github.com/google/uuid"

	"github.com/boschglobal/dse.clib/extra/go/file/index"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi3"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/log"
)

type GenFmiModelcCommand struct {
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

func NewFmiModelcCommand(name string) *GenFmiModelcCommand {
	c := &GenFmiModelcCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
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

func (c GenFmiModelcCommand) Name() string {
	return c.commandName
}

func (c GenFmiModelcCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *GenFmiModelcCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *GenFmiModelcCommand) Run() error {
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

func (c *GenFmiModelcCommand) getFmuBinaryDirName(platform string) (dir string) {
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

func (c *GenFmiModelcCommand) getFmuLibPath(packagePath string, modelIdentifier string, platform string) string {
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

func (c *GenFmiModelcCommand) getFmuLicensesPath() string {
	return filepath.Join("/licenses")
}

func (c *GenFmiModelcCommand) getFmuBinPath(fmuBinPath string, modelIdentifier string, platform string) string {
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

func (c *GenFmiModelcCommand) createFmuXml(name string, uuid string, version string, signalGroups string, index *index.YamlFileIndex) interface{} {

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
