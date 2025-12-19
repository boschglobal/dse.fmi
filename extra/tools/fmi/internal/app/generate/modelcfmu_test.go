// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"os"
	"path"
	"testing"

	"github.com/boschglobal/dse.clib/extra/go/file/index"

	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"

	"github.com/davecgh/go-spew/spew"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"gopkg.in/yaml.v3"
)

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
	var generatedYAML schema_kind.SignalGroup
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
	var stackYaml schema_kind.Stack
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

func TestGenModelCFmuAnnotationSignals_DirectIndex(t *testing.T) {
	defer func() {
		if t.Failed() {
			os.Exit(1)
		}
	}()
	tmpdir := t.TempDir()
	simPath := path.Join(tmpdir, "sim")
	spew.Dump(simPath)
	//simPath = "sim"
	//os.RemoveAll(simPath)
	os.CopyFS(simPath, os.DirFS("../../../test/testdata/directindex/sim"))

	cmd := NewGenModelCFmuAnnotationCommand("test")
	args := []string{"-sim", simPath, "-ruleset", "signal-direction", "-index", "-log", "2"}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed run: %v", err)
	}
	directIndexPath := path.Join(simPath, "data/direct_index.yaml")
	assert.FileExists(t, directIndexPath)

	// Pic a few signals.
	checks := []struct {
		signal     string
		group      string
		groupAlias string
		index      int
		offset     int
	}{
		{
			signal:     "in_a",
			group:      "in",
			groupAlias: "in_vector",
			index:      0,
			offset:     0,
		},
		{
			signal:     "in_b",
			group:      "in",
			groupAlias: "in_vector",
			index:      1,
			offset:     8,
		},
		{
			signal:     "in_c",
			group:      "in",
			groupAlias: "in_vector",
			index:      2,
			offset:     16,
		},
		{
			signal:     "out_a",
			group:      "out",
			groupAlias: "out_vector",
			index:      0,
			offset:     (24*3 + 0),
		},
		{
			signal:     "out_b",
			group:      "out",
			groupAlias: "out_vector",
			index:      1,
			offset:     (24*3 + 8),
		},
		{
			signal:     "out_c",
			group:      "out",
			groupAlias: "out_vector",
			index:      2,
			offset:     (24*3 + 16),
		},
	}
	// Check the direct index (only).
	idx := index.NewYamlFileIndex()
	idx.Scan(path.Join(simPath, "data"))
	assert.Equal(t, 2, len(idx.DocMap["SignalGroup"]))
	for i, check := range checks {
		t.Log("check", i, check)
		var sg *kind.KindDoc
		for _, doc := range idx.DocMap["SignalGroup"] {
			t.Log("name", doc.Metadata.Name)
			if doc.Metadata.Name == check.group {
				sg = &doc
				break
			}
		}
		require.NotNil(t, sg)
		sgSpec := sg.Spec.(*schema_kind.SignalGroupSpec)
		var s schema_kind.Signal
		for _, signal := range sgSpec.Signals {
			if signal.Signal == check.signal {
				s = signal
				break
			}
		}
		require.NotNil(t, s)
		assert.Equal(t, check.index, (*s.Annotations)["index"])
		assert.Equal(t, check.offset, (*s.Annotations)["offset"])
	}

	// Check the SignalGroup annotations.
	idx = index.NewYamlFileIndex()
	idx.Scan(path.Join(simPath, "model", "direct"))
	assert.Equal(t, 2, len(idx.DocMap["SignalGroup"]))
	for i, check := range checks {
		t.Log("check", i, check)
		var sg *kind.KindDoc
		for _, doc := range idx.DocMap["SignalGroup"] {
			t.Log("name", doc.Metadata.Name)
			if doc.Metadata.Name == check.groupAlias {
				sg = &doc
				break
			}
		}
		require.NotNil(t, sg)
		sgSpec := sg.Spec.(*schema_kind.SignalGroupSpec)
		var s schema_kind.Signal
		for _, signal := range sgSpec.Signals {
			if signal.Signal == check.signal {
				s = signal
				break
			}
		}
		require.NotNil(t, s)
		assert.Equal(t, check.signal, (*s.Annotations)["fmi_variable_name"])
		assert.Equal(t, check.offset, (*s.Annotations)["fmi_variable_vref"])
	}
}
