package patch

import (
	"os"
	"testing"

	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/stretchr/testify/assert"
	"gopkg.in/yaml.v3"
)

func TestPatchSignalGrpCommand_Run(t *testing.T) {
	var csv_file = "../../test/testdata/fmimcl/patch.csv"
	var signalgroup_file = "../../test/testdata/fmimcl/signalgroup.yaml"

	// Copy the original file to a temporary location
	tempFile := signalgroup_file + ".temp"

	input, _ := os.ReadFile(signalgroup_file)
	if err := os.WriteFile(tempFile, input, 0644); err != nil {
		t.Fatalf("Failed writing: %v", err)
	}

	defer os.Remove(tempFile)

	cmd := NewPatchSignalGroupCommand("test")
	args := []string{"-input", tempFile, "-patch", csv_file}

	// Parse and run the command

	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Error parsing arguments: %v", err)
	}
	if err := cmd.Run(); err != nil {

		t.Fatalf("Error running command: %v", err)
	}
}

func TestPatchedSignalGrpAnnotations(t *testing.T) {
	var csv_file = "../../test/testdata/fmimcl/patch.csv"
	var signalgroup_file = "../../test/testdata/fmimcl/signalgroup.yaml"

	// Copy the original file to a temporary location
	tempFile := signalgroup_file + ".temp"
	input, _ := os.ReadFile(signalgroup_file)
	if err := os.WriteFile(tempFile, input, 0644); err != nil {
		t.Fatalf("Failed writing: %v", err)
	}
	defer os.Remove(tempFile)

	cmd := NewPatchSignalGroupCommand("test")
	args := []string{"-input", tempFile, "-patch", csv_file}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed Run: %v", err)
	}

	// Read and parse the generated YAML file
	data, err := os.ReadFile(tempFile)
	if err != nil {
		t.Fatalf("Failed to read generated YAML file: %v", err)
	}

	var generatedYAML kind.SignalGroup
	if err := yaml.Unmarshal(data, &generatedYAML); err != nil {
		t.Fatalf("Failed to parse generated YAML: %v", err)
	}

	// Extract annotations and verify
	assert.Equal(t, string(generatedYAML.Kind), "SignalGroup", "kind should match")
	assert.Equal(t, *generatedYAML.Metadata.Name, "Test", "metadata/name should match")
	assert.Equal(t, (*generatedYAML.Metadata.Labels)["channel"], "E2M_M2E", "metadata/labels/channel should match")
	assert.Equal(t, (*generatedYAML.Metadata.Labels)["model"], "Test", "metadata/labels/model should match")
}

func TestPatchedSignalGrpSignals(t *testing.T) {
	var csv_file = "../../test/testdata/fmimcl/patch.csv"
	var signalgroup_file = "../../test/testdata/fmimcl/signalgroup.yaml"

	// Copy the original file to a temporary location
	tempFile := signalgroup_file + ".temp"
	input, _ := os.ReadFile(signalgroup_file)
	if err := os.WriteFile(tempFile, input, 0644); err != nil {
		t.Fatalf("Failed writing: %v", err)
	}
	defer os.Remove(tempFile)

	cmd := NewPatchSignalGroupCommand("test")
	args := []string{"-input", tempFile, "-patch", csv_file, "-remove-unknown"}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed Run: %v", err)
	}

	// Read and parse the generated YAML file
	data, err := os.ReadFile(tempFile)
	if err != nil {
		t.Fatalf("Failed to read generated YAML file: %v", err)
	}

	var generatedYAML kind.SignalGroup
	if err := yaml.Unmarshal(data, &generatedYAML); err != nil {
		t.Fatalf("Failed yaml parsing: %v", err)
	}

	test_data := []map[string]interface{}{
		{
			"Signal":                  "input_1_patched",
			"softecu_direction":       "M2E",
			"softecu_access_function": "E2M+M2E",
		},
		{
			"Signal":                  "input_2_patched",
			"softecu_direction":       "M2E",
			"softecu_access_function": "E2M+M2E",
		},
		{
			"Signal":            "output_1_patched",
			"softecu_direction": "E2M",

			"softecu_access_function": "E2M+M2E",
		},
	}

	for i, s := range generatedYAML.Spec.Signals {
		assert.Equal(t, s.Signal, test_data[i]["Signal"], "signal should match")
		assert.Equal(t, (*s.Annotations)["softecu_direction"], test_data[i]["softecu_direction"], "annotation/softecu_direction should match")
		assert.Equal(t, (*s.Annotations)["softecu_access_function"], test_data[i]["softecu_access_function"], "annotation/softecu_access_function should match")
	}
}
