// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"testing"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/index"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/stretchr/testify/assert"
)

func TestFmiModelc_ModelDescription(t *testing.T) {
	var index = index.NewYamlFileIndex()
	index.Scan("../../../test/testdata/modelcfmu/sim")
	if len(index.DocMap["Stack"]) != 1 {
		assert.Fail(t, "Simulation folder contains %d Stacks, expected 1")
	}
	fmiMD := createFmuXml("modelcfmu_FMU_test", "11111111-2222-3333-4444-555555555555", "1.2.3", "", index)

	assert.Equal(t, "2.0", fmiMD.FmiVersion, "fmiVersion should match")
	assert.Equal(t, "{11111111-2222-3333-4444-555555555555}", fmiMD.Guid, "Guid should match")
	assert.Equal(t, "fmi2modelcfmu", fmiMD.CoSimulation.ModelIdentifier, "ModelIdentifier should match")

	ann := &[]fmi2.Annotations{
		{
			Tool: []fmi2.Tool{
				{
					Name: "dse.standards.fmi-ls-binary-codec",
					Annotation: &[]fmi2.Annotation{
						{Name: "mimetype", Text: "application/x-automotive-bus; interface=stream; type=frame; bus=can; schema=fbs; bus_id=1; node_id=2; interface_id=0"},
					},
				},
				{
					Name: "dse.standards.fmi-ls-binary-to-text",
					Annotation: &[]fmi2.Annotation{
						{Name: "encoding", Text: "ascii85"},
					},
				},
				{
					Name: "dse.standards.fmi-ls-bus-topology",
					Annotation: &[]fmi2.Annotation{
						{Name: "bus_id", Text: "1"},
					},
				},
			},
		},
	}

	var test_data = []fmi2.ScalarVariable{
		{
			Name:           "counter",
			ValueReference: "1",
			Causality:      "output",
			Real: &fmi2.FmiReal{
				Start: "42",
			},
		},
		{
			Name:           "network_1_1_rx",
			ValueReference: "2",
			Causality:      "input",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_1_tx",
			ValueReference: "3",
			Causality:      "output",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_2_rx",
			ValueReference: "4",
			Causality:      "input",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_2_tx",
			ValueReference: "5",
			Causality:      "output",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_3_rx",
			ValueReference: "6",
			Causality:      "input",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_3_tx",
			ValueReference: "7",
			Causality:      "output",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_4_rx",
			ValueReference: "8",
			Causality:      "input",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
		{
			Name:           "network_1_4_tx",
			ValueReference: "9",
			Causality:      "output",
			String:         &fmi2.FmiString{},
			Annotations:    ann,
		},
	}

	for i, variable := range fmiMD.ModelVariables.ScalarVariable {
		assert.Equal(t, test_data[i].Name, variable.Name, "variable.Name should match")
		assert.Equal(t, test_data[i].ValueReference, variable.ValueReference, "variable.ValueReference should match")
		assert.Equal(t, test_data[i].Causality, variable.Causality, "variable.Causality should match")
		if test_data[i].Real != nil {
			assert.NotNil(t, variable.Real)
			if test_data[i].Real.Start != "" {
				assert.Equal(t, test_data[i].Real.Start, variable.Real.Start, "variable.Real.Start should match")
			}
		}
	}

}
