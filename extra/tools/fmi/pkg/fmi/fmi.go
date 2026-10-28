// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi

import (
	"fmt"
	"path/filepath"
	"strconv"
	"strings"
)

type FmiConfig struct {
	Name            string
	UUID            string
	Version         string
	FmiVersion      string
	Description     string
	StartTime       float64
	StopTime        *float64
	StepSize        float64
	ModelIdentifier string
	GenerationTool  string
	Annotations     map[string]string
}

// FormatReal formats a float as the shortest string that parses back to the
// same value (xs:double compatible, e.g. 2.38587666666667e-05).
func FormatReal(v float64) string {
	return strconv.FormatFloat(v, 'g', -1, 64)
}

// ToFloat64 converts a YAML numeric value (int or float) to float64.
func ToFloat64(v any) (float64, bool) {
	switch v := v.(type) {
	case float64:
		return v, true
	case float32:
		return float64(v), true
	case int:
		return float64(v), true
	case int64:
		return float64(v), true
	case uint64:
		return float64(v), true
	}
	return 0, false
}

// StartValue formats a fmi_variable_start_value annotation for a numeric start
// attribute. Returns false if the value is missing or empty.
func StartValue(v any) (string, bool) {
	switch v := v.(type) {
	case string:
		return v, v != ""
	case int:
		return strconv.Itoa(v), true
	case int64:
		return strconv.FormatInt(v, 10), true
	case uint64:
		return strconv.FormatUint(v, 10), true
	case float32:
		return strconv.FormatFloat(float64(v), 'g', -1, 32), true
	case float64:
		return FormatReal(v), true
	}
	return "", false
}

// StartString returns a fmi_variable_start_value annotation as a string start
// attribute; non-string YAML scalars (e.g. 5 or true) are formatted, not rejected.
func StartString(v any) string {
	if v == nil {
		return ""
	}
	if s, ok := StartValue(v); ok {
		return s
	}
	return fmt.Sprint(v)
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
