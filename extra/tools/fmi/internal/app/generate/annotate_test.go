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

func TestGenModelCFmuAnnotationSignals_Sim(t *testing.T) {
	tmpdir := t.TempDir()
	simPath := path.Join(tmpdir, "sim")
	//simPath = "sim"
	//os.RemoveAll(simPath)
	os.CopyFS(simPath, os.DirFS("../../../test/testdata/modelcfmu/sim"))

	cmd := NewGenModelCFmuAnnotationCommand("test")
	args := []string{"-sim", simPath, "-signalgroups", "signal", "-ruleset", "signal-direction"}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed run: %v", err)
	}

	// SignalGroup annotations.
	var generatedYAML kind.SignalGroup
	data, _ := os.ReadFile(path.Join(simPath, "data/signalgroup-signal.yaml"))
	if err := yaml.Unmarshal(data, &generatedYAML); err != nil {
		t.Fatalf("Failed yaml parsing: %v", err)
	}
	var test_data = []map[string]interface{}{
		{
			"Signal":                 "reset_counters",
			"direction":              "input",
			"fmi_variable_causality": "input",
			"fmi_variable_vref":      1,
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "reset_counters",
		},
		{
			"Signal":                 "task_init_done",
			"direction":              "output",
			"fmi_variable_causality": "output",
			"fmi_variable_vref":      2,
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "task_init_done",
		},
		{
			"Signal":                 "task_5_active",
			"direction":              "output",
			"fmi_variable_causality": "output",
			"fmi_variable_vref":      3,
			"fmi_variable_type":      "Real",
			"fmi_variable_name":      "task_5_active",
		},
		// 3 and 4 (frame) not considered
	}
	assert.Equal(t, len(test_data), len(generatedYAML.Spec.Signals))
	for i, s := range generatedYAML.Spec.Signals {
		assert.Equal(t, s.Signal, test_data[i]["Signal"])
		assert.Equal(t, (*s.Annotations)["fmi_variable_causality"], test_data[i]["fmi_variable_causality"])
		assert.Equal(t, (*s.Annotations)["fmi_variable_vref"], test_data[i]["fmi_variable_vref"])
		assert.Equal(t, (*s.Annotations)["fmi_variable_type"], test_data[i]["fmi_variable_type"])
		assert.Equal(t, (*s.Annotations)["fmi_variable_name"], test_data[i]["fmi_variable_name"])
	}

	// Stack annotations.
	var stackYaml kind.Stack
	data, _ = os.ReadFile(path.Join(simPath, "data/simulation.yaml"))
	if err := yaml.Unmarshal(data, &stackYaml); err != nil {
		t.Fatalf("Failed yaml parsing: %v", err)
	}
	yamlFiles := []interface{}{
		"data/model.yaml",
		"data/runnable.yaml",
		"data/signalgroup-network.yaml",
		"data/signalgroup-signal.yaml",
		"data/simulation.yaml",
	}
	assert.ElementsMatch(t, (*stackYaml.Metadata.Annotations)["model_runtime__yaml_files"].([]interface{}), yamlFiles)
	assert.Equal(t, (*stackYaml.Metadata.Annotations)["model_runtime__model_inst"], "target_inst")
}
