// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"flag"
	"fmt"
	"github.com/google/uuid"
	"log/slog"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"time"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/index"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/operations"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/log"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type GenFmiModelcCommand struct {
	commandName string
	fs          *flag.FlagSet

	simpath      string
	platform     string
	signalGroups string
	name         string
	version      string
	uuid         string
	outdir       string
	logLevel     int

	libRootPath string
}

func NewFmiModelcCommand(name string) *GenFmiModelcCommand {
	c := &GenFmiModelcCommand{commandName: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.simpath, "sim", "", "Path to simulation (Simer layout)")
	c.fs.StringVar(&c.platform, "platform", "linux-amd64", "Platform of FMU to generate (e.g. linux-amd64)")
	c.fs.StringVar(&c.signalGroups, "signalgroups", "", "Signal Groups to export as FMU variables, default is all, specify with comma-separated-list")
	c.fs.StringVar(&c.name, "name", "", "Name of the FMU")
	c.fs.StringVar(&c.version, "version", "0.0.1", "Version to assign to the FMU")
	c.fs.StringVar(&c.uuid, "uuid", "11111111-2222-3333-4444-555555555555", "UUID to assign to the FMU, set to '' to generate a new UUID")
	c.fs.StringVar(&c.outdir, "outdir", "out", "Output directory for the FMU file")
	c.fs.IntVar(&c.logLevel, "log", 4, "Loglevel")

	// Supports unit testing.
	c.fs.StringVar(&c.libRootPath, "libroot", "/usr/local", "System lib root path (where lib & lib32 directory are found)")

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

	// Index the simulation files, layout is partly fixed:
	//	<simpath>/data/simulation.yaml
	//	<simpath>/...
	fmt.Fprintf(flag.CommandLine.Output(), "Scanning simulation (%s) ...\n", c.simpath)
	var index = index.NewYamlFileIndex()
	index.Scan(c.simpath)
	if len(index.DocMap["Stack"]) != 1 {
		return fmt.Errorf("Simulation folder contains %d Stacks, expected 1", len(index.DocMap["Stack"]))
	}
	stackDoc := index.DocMap["Stack"][0]
	if stackDoc.File != filepath.Join(c.simpath, "data/simulation.yaml") {
		// This constraint is hard coded in fmi2fmu.c.
		return fmt.Errorf("Simulation layout incorrect, Stack should be in file data/simulation.yaml (is in: %s)", stackDoc.File)
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

	fmuBinDir := getFmuBinaryDirName(c.platform)
	if fmuBinDir == "" {
		return fmt.Errorf("Platform not supported (%s)", c.platform)
	}
	fmuBinPath := filepath.Join(fmuOutDir, "binaries", fmuBinDir)
	if err := os.MkdirAll(fmuBinPath, os.ModePerm); err != nil {
		return fmt.Errorf("Could not create FMU binaries directory (%v)", err)
	}
	modelIdentifier := "fmi2modelcfmu"
	if err := operations.Copy(getFmuLibPath(c.libRootPath, modelIdentifier, c.platform), getFmuBinPath(fmuBinPath, modelIdentifier, c.platform)); err != nil {
		return fmt.Errorf("Could not copy FMU binary (%v)", err)
	}

	if err := operations.CopyDirectory(c.simpath, filepath.Join(fmuOutDir, "resources", "sim")); err != nil {
		return fmt.Errorf("Could not copy FMU resources (%v)", err)
	}

	if err := operations.CopyDirectory(getFmuLicensesPath(c.libRootPath), filepath.Join(fmuOutDir, "resources/licenses")); err != nil {
		return fmt.Errorf("Could not copy licenses (%v)", err)
	}

	// Construct the FMU Model Description.
	fmuModelDescriptionFilename := filepath.Join(fmuOutDir, "modelDescription.xml")
	fmt.Fprintf(flag.CommandLine.Output(), "Create FMU Model Description (%s) ...\n", fmuModelDescriptionFilename)
	fmuXml := createFmuXml(c.name, c.uuid, c.version, c.signalGroups, index)
	if err := writeXml(fmuXml, fmuModelDescriptionFilename); err != nil {
		return fmt.Errorf("Could not generate the FMU Model Description (%v)", err)
	}

	// Produce the FMU.
	fmuFilename := fmuOutDir + ".fmu"
	fmt.Fprintf(flag.CommandLine.Output(), "Create FMU Package (%s) ...\n", fmuFilename)
	if err := operations.Zip(fmuOutDir, fmuFilename); err != nil {
		return fmt.Errorf("Could not create FMU (%v)", err)
	}

	return nil
}

func getFmuBinaryDirName(platform string) (dir string) {
	os, arch, found := strings.Cut(platform, "-")
	if !found {
		return
	}
	switch os {
	case "linux":
		switch arch {
		case "amd64":
			dir = "linux64"
		case "x86", "i386":
			dir = "linux32"
		}
	case "windows":
		switch arch {
		case "x64":
			dir = "win64"
		case "x86":
			dir = "win32"
		}
	}
	return
}

func getFmuLibPath(libRootPath string, modelIdentifier string, platform string) string {
	libpath := "lib"
	filePrefix := ""
	filePostfix := ""
	extension := "so"

	os, arch, found := strings.Cut(platform, "-")
	if found {
		switch os {
		case "linux":
			switch arch {
			case "x86":
				libpath = "lib32"
				filePostfix = "_i386"
			case "i386":
				libpath = "lib32"
				filePostfix = "_x86"
			}
		case "windows":
			extension = "dll"
			switch arch {
			case "x86":
				libpath = "lib32"
			}
		}
	}
	return fmt.Sprintf("%s/%s/%s%s%s.%s", libRootPath, libpath, filePrefix, modelIdentifier, filePostfix, extension)
}

func getFmuLicensesPath(libRootPath string) string {
	return filepath.Join(libRootPath, "licenses")
}

func getFmuBinPath(fmuBinPath string, modelIdentifier string, platform string) string {
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

func createFmuXml(name string, uuid string, version string, signalGroups string, index *index.YamlFileIndex) fmi2.FmiModelDescription {
	fmuXml := fmi2.FmiModelDescription{}
	setGeneralFmuXmlFields(name, uuid, version, &fmuXml)
	if signalGroupDocs, ok := index.DocMap["SignalGroup"]; ok {
		filter := []string{}
		if len(signalGroups) > 0 {
			filter = strings.Split(signalGroups, ",")
		}
		for _, doc := range signalGroupDocs {
			if len(filter) > 0 && !slices.Contains(filter, doc.Metadata.Name) {
				continue
			}
			fmt.Fprintf(flag.CommandLine.Output(), "Adding SignalGroup: %s (%s)\n", doc.Metadata.Name, doc.File)
			signalGroupSpec := doc.Spec.(*schema_kind.SignalGroupSpec)
			if doc.Metadata.Annotations["vector_type"] == "binary" {
				if err := fmi2.BinarySignal(*signalGroupSpec, &fmuXml); err != nil {
					slog.Warn(fmt.Sprintf("Skipped BinarySignal (Error: %s)", err.Error()))
				}
			} else {
				if err := fmi2.ScalarSignal(*signalGroupSpec, &fmuXml); err != nil {
					slog.Warn(fmt.Sprintf("Skipped Scalarsignal (Error: %s)", err.Error()))
				}
			}
		}
	}
	return fmuXml
}

func setGeneralFmuXmlFields(name string, uuid string, version string, fmuXml *fmi2.FmiModelDescription) {
	// Basic FMU Information.
	fmuXml.ModelName = name
	fmuXml.FmiVersion = "2.0"
	fmuXml.Guid = fmt.Sprintf("{%s}", uuid)
	fmuXml.GenerationTool = "DSE FMI - ModelC FMU"
	fmuXml.GenerationDateAndTime = time.Now().String()
	fmuXml.Author = "Robert Bosch GmbH"
	fmuXml.Version = version
	fmuXml.Description = fmt.Sprintf("Model '%s' via FMI ModelC FMU (using DSE ModelC Runtime).", name)

	// Runtime Information.
	fmuXml.DefaultExperiment.StartTime = "0.0"
	fmuXml.DefaultExperiment.StopTime = "1.0"
	fmuXml.DefaultExperiment.StepSize = "0.0005"

	// Model Information.
	fmuXml.CoSimulation.ModelIdentifier = "fmi2modelcfmu" // This is the packaged SO/DLL file.
	fmuXml.CoSimulation.CanHandleVariableCommunicationStepSize = "true"
	fmuXml.CoSimulation.CanInterpolateInputs = "true"
}
