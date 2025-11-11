// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi

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
