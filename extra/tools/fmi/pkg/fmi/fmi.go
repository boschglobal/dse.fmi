// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi

import (
	"fmt"
	"path/filepath"
	"strings"
)

type FmiConfig struct {
	Name            string
	UUID            string
	Version         string
	FmiVersion      string
	Description     string
	StartTime       float64
	StopTime        float64
	StepSize        float64
	ModelIdentifier string
	GenerationTool  string
	Annotations     map[string]string
}

func GetFmuBinaryDirName(platform string, fmiVersion string) (dir string) {
	os, arch, found := strings.Cut(platform, "-")
	if !found {
		return
	}

	switch os {
	case "linux":
		switch arch {
		case "amd64":
			switch fmiVersion {
			case "2":
				dir = "linux64"
			case "3":
				dir = "x86_64-linux"
			}
		case "x86", "i386":
			switch fmiVersion {
			case "2":
				dir = "linux32"
			case "3":
				dir = "x86-linux"
			}
		}
	case "windows":
		switch arch {
		case "x64":
			switch fmiVersion {
			case "2":
				dir = "win64"
			case "3":
				dir = "x86_64-windows"
			}
		case "x86":
			switch fmiVersion {
			case "2":
				dir = "win32"
			case "3":
				dir = "x86-windows"
			}
		}
	}
	return
}

func GetFmuLibPath(packagePath string, modelIdentifier string, platform string) string {
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

func GetFmuLicensesPath() string {
	return filepath.Join("/licenses")
}

func GetFmuBinPath(fmuBinPath string, modelIdentifier string, platform string) string {
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
