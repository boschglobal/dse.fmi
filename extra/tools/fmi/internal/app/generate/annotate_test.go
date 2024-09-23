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

func TestGenFmuAnnotationCommand_Run(t *testing.T) {
	signalgroup := "../../../test/testdata/fmimcl/signalgroup.yaml"
	rule_file := "../../../test/testdata/fmimcl/rule.csv"
	sg_annotated := "fmimcl_annotated_test.yaml"

	cmd := NewGenFmuAnnotationCommand("test")
	args := []string{"-input", signalgroup, "-output", sg_annotated, "-rule", rule_file}

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
	defer os.Remove(sg_annotated)

	// Check if generated SignalGroup YAML exists
	_, err = os.Stat(sg_annotated)
	if os.IsNotExist(err) {
		t.Errorf("Output Yaml file does not exist: %v", err)
	}
}

func TestGenFmuAnnotationSignals(t *testing.T) {
	signalgroup := "../../../test/testdata/fmimcl/signalgroup.yaml"
	rule_file := "../../../test/testdata/fmimcl/rule.csv"
	sg_annotated := "fmimcl_annotated_test.yaml"

	cmd := NewGenFmuAnnotationCommand("test")
	args := []string{"-input", signalgroup, "-output", sg_annotated, "-rule", rule_file}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed run: %v", err)
	}
	defer os.Remove(sg_annotated)
	data, _ := os.ReadFile(sg_annotated)

	var generatedYAML kind.SignalGroup
	if err := yaml.Unmarshal(data, &generatedYAML); err != nil {
		t.Fatalf("Failed yaml parsing: %v", err)
	}

	var test_data = []map[string]interface{}{
		{
			"Signal":                 "input_1",
			"fmi_variable_causality": "input",
			"fmi_variable_id":        "1",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "input_1",
		},
		{
			"Signal":                 "input_2",
			"fmi_variable_causality": "input",
			"fmi_variable_id":        "2",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "input_2",
		},
		{
			"Signal":                 "output_1",
			"fmi_variable_causality": "output",
			"fmi_variable_id":        "3",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "output_1",
		},
		{
			"Signal":                 "output_2",
			"fmi_variable_causality": "output",
			"fmi_variable_id":        "4",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "output_2",
		},
	}

	for i, s := range generatedYAML.Spec.Signals {
		assert.Equal(t, s.Signal, test_data[i]["Signal"], "signal should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_causality"], test_data[i]["fmi_variable_causality"], "annotation/fmi_variable_causality should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_id"], test_data[i]["fmi_variable_id"], "annotation/fmi_variable_id should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_type"], test_data[i]["fmi_variable_type"], "annotation/fmi_variable_type should match")
		assert.Equal(t, (*s.Annotations)["fmi_variable_name"], test_data[i]["fmi_variable_name"], "annotation/fmi_variable_name should match")
	}
}
