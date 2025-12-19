// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"os"
	"path"
	"testing"

	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/stretchr/testify/assert"
	"gopkg.in/yaml.v3"
)

func TestGenAnnotationSignals_SignalGroup(t *testing.T) {
	signalgroup := "../../../test/testdata/fmimcl/signalgroup.yaml"
	rule_file := "../../../test/testdata/fmimcl/rule.csv"
	tmpdir := t.TempDir()
	sg_annotated := path.Join(tmpdir, "fmimcl_annotated_test.yaml")

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
			"fmi_variable_vref":      "1",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "input_1",
		},
		{
			"Signal":                 "input_2",
			"fmi_variable_causality": "input",
			"fmi_variable_vref":      "2",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "input_2",
		},
		{
			"Signal":                 "output_1",
			"fmi_variable_causality": "output",
			"fmi_variable_vref":      "3",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "output_1",
		},
		{
			"Signal":                 "output_2",
			"fmi_variable_causality": "output",
			"fmi_variable_vref":      "4",
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "output_2",
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
