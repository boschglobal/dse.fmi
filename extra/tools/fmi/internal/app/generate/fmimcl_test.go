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

func TestFmiMcl_Command(t *testing.T) {
	var xml_file = "../../../test/testdata/fmimcl/fmimcl.xml"
	var model_file = "fmimcl_model_test.yaml"
	var channels = `[{"name": "E2MM2E", "alias": "E2MM2E_Alias", "selector":{"selector":"E2M2E_Selector"}}]`
	var arch = "amd64"
	var OS = "linux"
	var mcl = "./fmu.so"
	var dynlib = "./fmimcl.so"

	cmd := NewFmiMclCommand("test")
	args := []string{"-input", xml_file, "-output", model_file, "-channels", channels, "-arch", arch, "-os", OS, "-fmu", mcl, "-dynlib", dynlib}

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
	defer os.Remove(model_file)

	// Check if generated SignalGroup YAML exists
	_, err = os.Stat(model_file)
	if os.IsNotExist(err) {
		t.Errorf("Output Yaml file does not exist: %v", err)
	}
}

func TestFmiMcl_Annotations(t *testing.T) {
	var xml_file = "../../../test/testdata/fmimcl/fmimcl.xml"
	var model_file = "fmimcl_model_test.yaml"
	var channels = `[{"name": "E2MM2E", "alias": "E2MM2E_Alias", "selector":{"selector":"E2M2E_Selector"}}]`
	var arch = "amd64"
	var OS = "linux"
	var mcl = "./mcl.so"
	var dynlib = "./fmu.so"
	var resource = "./test/resources"

	cmd := NewFmiMclCommand("test")
	args := []string{"-input", xml_file, "-output", model_file, "-channels", channels, "-arch", arch, "-os", OS, "-fmu", mcl, "-dynlib", dynlib, "-resource", resource}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf("Failed command parsing: %v", err)
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf("Failed command run: %v", err)
	}
	defer os.Remove(model_file)

	// Read the generated YAML file
	data, err := os.ReadFile(model_file)
	if err != nil {
		t.Fatalf("Failed to read generated YAML file: %v", err)
	}

	// Parse the generated YAML
	var generatedYAML kind.Model
	err = yaml.Unmarshal(data, &generatedYAML)
	if err != nil {
		t.Fatalf("Failed to parse generated YAML: %v", err)
	}

	// Extract annotations
	assert.Equal(t, string(generatedYAML.Kind), "Model", "kind should match")

	assert.Equal(t, *generatedYAML.Metadata.Name, "Test", "metadata/name should match")
	annotations := (*generatedYAML.Metadata.Annotations)
	assert.Equal(t, annotations["fmi_guid"], "{0bc3244a-e274-4c4a-8205-b2a5a18af23a}", "metadata/annotations/fmi_guid should match")
	assert.Equal(t, annotations["fmi_model_cosim"], "true", "metadata/annotations/fmi_model_cosim should match")
	assert.Equal(t, annotations["fmi_model_version"], "1.1", "metadata/annotations/fmi_model_version should match")
	assert.Equal(t, annotations["fmi_resource_dir"], "./test/resources", "metadata/annotations/fmi_resource_dir should match")
	assert.Equal(t, annotations["fmi_stepsize"], "0.0005", "metadata/annotations/fmi_stepsize should match")
	assert.Equal(t, annotations["mcl_adapter"], "fmi", "metadata/annotations/mcl_adapter should match")
	assert.Equal(t, annotations["mcl_version"], "2.0", "metadata/annotations/mcl_version should match")
}

func TestFmiMcl_Spec(t *testing.T) {
	var xml_file = "../../../test/testdata/fmimcl/fmimcl.xml"
	var model_file = "fmimcl_model_test.yaml"
	var channels = `[{"name": "E2MM2E", "alias": "E2MM2E_Alias", "selector":{"selector":"E2M2E_Selector"}}]`
	var arch = "amd64"
	var OS = "linux"
	var mcl = "./mcl.so"
	var dynlib = "./fmu.so"

	cmd := NewFmiMclCommand("test")
	args := []string{"-input", xml_file, "-output", model_file, "-channels", channels, "-arch", arch, "-os", OS, "-fmu", mcl, "-dynlib", dynlib}
	if err := cmd.Parse(args); err != nil {
		t.Fatalf(err.Error())
	}
	if err := cmd.Run(); err != nil {
		t.Fatalf(err.Error())
	}
	defer os.Remove(model_file)

	data, _ := os.ReadFile(model_file)

	var generatedYAML kind.Model
	if err := yaml.Unmarshal(data, &generatedYAML); err != nil {
		t.Fatalf(err.Error())
	}

	// Extract annotations
	for _, channel := range *generatedYAML.Spec.Channels {
		assert.Equal(t, *channel.Alias, "E2MM2E_Alias", "spec/channels/alias should match")
		assert.Equal(t, *channel.Name, "E2MM2E", "spec/channels/name should match")
		for k, v := range *channel.Selectors {
			assert.Equal(t, k, "selector", "spec/channels/selectors should match")
			assert.Equal(t, v, "E2M2E_Selector", "spec/channels/selectors should match")
		}
	}

	for _, dynlib := range *generatedYAML.Spec.Runtime.Dynlib {
		assert.Equal(t, *dynlib.Arch, "amd64", "spec/runtime/dynlib should match")
		assert.Equal(t, *dynlib.Os, "linux", "spec/runtime/dynlib should match")
		assert.Equal(t, dynlib.Path, "./fmu.so", "spec/runtime/dynlib should match")
	}

	for _, mcl := range *generatedYAML.Spec.Runtime.Mcl {
		assert.Equal(t, *mcl.Arch, "amd64", "spec/runtime/dynlib should match")
		assert.Equal(t, *mcl.Os, "linux", "spec/runtime/dynlib should match")
		assert.Equal(t, mcl.Path, "./mcl.so", "spec/runtime/dynlib should match")
	}

	defer os.Remove(model_file)
}
