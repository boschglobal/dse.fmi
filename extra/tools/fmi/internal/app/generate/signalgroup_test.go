// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"os"
	"testing"

	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/stretchr/testify/assert"
	"gopkg.in/yaml.v3"
)

func TestGenSignalGrpCommand_Run(t *testing.T) {
	var xml_file = "../../../test/testdata/fmimcl/fmimcl.xml"
	var signalgroup_file = "fmimcl_signalgroup_test.yaml"

	cmd := NewGenSignalGroupCommand("test")
	args := []string{"-input", xml_file, "-output", signalgroup_file}

	// Parse arguments
	err := cmd.Parse(args)
	if err != nil {
		t.Errorf("Error parsing arguments: %v", err)
	}

	// Run the command
	err = cmd.Run()
	if err != nil {
		t.Errorf("Error running command: %v", err)
	}
	defer os.Remove(signalgroup_file)

	// Check if generated SignalGroup YAML exists
	_, err = os.Stat(signalgroup_file)
	if os.IsNotExist(err) {
		t.Errorf("Output Yaml file does not exist: %v", err)
	}

}

func TestSignalGrpAnnotations(t *testing.T) {
	var xml_file = "../../../test/testdata/fmimcl/fmimcl.xml"
	var signalgroup_file = "fmimcl_signalgroup_test.yaml"

	cmd := NewGenSignalGroupCommand("test")
	args := []string{"-input", xml_file, "-output", signalgroup_file}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed Run: %v", err)
	}
	defer os.Remove(signalgroup_file)

	// Read the generated YAML file
	data, err := os.ReadFile(signalgroup_file)
	if err != nil {
		t.Fatalf("Failed to read generated YAML file: %v", err)
	}

	// Parse the generated YAML
	var generatedYAML kind.SignalGroup
	err = yaml.Unmarshal(data, &generatedYAML)
	if err != nil {
		t.Fatalf("Failed to parse generated YAML: %v", err)
	}

	// Extract annotations
	assert.Equal(t, string(generatedYAML.Kind), "SignalGroup", "kind should match")

	assert.Equal(t, *generatedYAML.Metadata.Name, "Test", "metadata/name should match")
	assert.Equal(t, (*generatedYAML.Metadata.Labels)["channel"], "signal_vector", "metadata/labels/channel should match")
	assert.Equal(t, (*generatedYAML.Metadata.Labels)["model"], "Test", "metadata/labels/model should match")
}

func TestSignalGrpSignals(t *testing.T) {
	var xml_file = "../../../test/testdata/fmimcl/fmimcl.xml"
	var signalgroup_file = "fmimcl_signalgroup_test.yaml"

	cmd := NewGenSignalGroupCommand("test")
	args := []string{"-input", xml_file, "-output", signalgroup_file}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed run: %v", err)
	}
	defer os.Remove(signalgroup_file)
	data, _ := os.ReadFile(signalgroup_file)

	var generatedYAML kind.SignalGroup
	if err := yaml.Unmarshal(data, &generatedYAML); err != nil {
		t.Fatalf("Failed yaml parsing: %v", err)
	}

	var test_data = []map[string]interface{}{
		{
			"Signal":                 "scalar_1",
			"fmi_variable_causality": "input",
			"fmi_variable_vref":      "1",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "scalar_1",
		},
		{
			"Signal":                 "scalar_2",
			"fmi_variable_causality": "output",
			"fmi_variable_vref":      "2",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "scalar_2",
		},
		{
			"Signal":                 "scalar_3",
			"fmi_variable_causality": "local",
			"fmi_variable_vref":      "3",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "scalar_3",
		},
		{
			"Signal":                 "boolean_1",
			"fmi_variable_causality": "input",
			"fmi_variable_vref":      "4",
			"fmi_variable_type":      "Boolean",
			"fmi_variable_name":      "boolean_1",
		},
		{
			"Signal":                 "boolean_2",
			"fmi_variable_causality": "output",
			"fmi_variable_vref":      "5",
			"fmi_variable_type":      "Boolean",
			"fmi_variable_name":      "boolean_2",
		},
		{
			"Signal":                 "scalar_4",
			"fmi_variable_causality": "local",
			"fmi_variable_vref":      "6",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "scalar_4",
			"internal":               true,
		},
	}

	for i, s := range generatedYAML.Spec.Signals {
		assert.Equal(t, s.Signal, test_data[i]["Signal"], "signal should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_causality"], test_data[i]["fmi_variable_causality"], "annotation/fmi_variable_causality should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_vref"], test_data[i]["fmi_variable_vref"], "annotation/fmi_variable_vref should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_type"], test_data[i]["fmi_variable_type"], "annotation/fmi_variable_type should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_name"], test_data[i]["fmi_variable_name"], "annotation/fmi_variable_name should match")
	}
}
