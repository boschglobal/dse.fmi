// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi2

import (
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"os"
	"strconv"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

// FMI XML struct/tags
// ===================

type Annotations struct {
	Tool any `xml:",any"`
}

type Tool struct {
	Name       string        `xml:"name,attr"`
	Annotation *[]Annotation `xml:"Annotation,omitempty"`
}

type Annotation struct {
	Name string `xml:"name,attr"`
	Text string `xml:",chardata"`
}

type FmiReal struct {
	Start      string `xml:"start,attr,omitempty"`
	Reinit     string `xml:"reinit,attr,omitempty"`
	Derivative string `xml:"derivative,attr,omitempty"`
}

type FmiInteger struct {
	Start string `xml:"start,attr,omitempty"`
}

type FmiString struct {
	Start string `xml:"start,attr,omitempty"`
}

type FmiBoolean struct {
	Start string `xml:"start,attr,omitempty"`
}

type ScalarVariable struct {
	Text           string         `xml:",chardata"`
	Name           string         `xml:"name,attr"`
	ValueReference string         `xml:"valueReference,attr"`
	Causality      string         `xml:"causality,attr"`
	Initial        string         `xml:"initial,attr"`
	Variability    string         `xml:"variability,attr"`
	Real           *FmiReal       `xml:"Real,omitempty"`
	String         *FmiString     `xml:"String,omitempty"`
	Boolean        *FmiBoolean    `xml:"Boolean,omitempty"`
	Integer        *FmiInteger    `xml:"Integer,omitempty"`
	Annotations    *[]Annotations `xml:"Annotations,omitempty"`
}

// FmiModelDescription was generated 2023-05-23 07:27:35 by https://xml-to-go.github.io/ in Ukraine.
type FmiModelDescription struct {
	XMLName                  xml.Name `xml:"fmiModelDescription"`
	Text                     string   `xml:",chardata"`
	FmiVersion               string   `xml:"fmiVersion,attr"`
	ModelName                string   `xml:"modelName,attr"`
	Guid                     string   `xml:"guid,attr"`
	Description              string   `xml:"description,attr"`
	Author                   string   `xml:"author,attr"`
	Version                  string   `xml:"version,attr"`
	GenerationTool           string   `xml:"generationTool,attr"`
	GenerationDateAndTime    string   `xml:"generationDateAndTime,attr"`
	VariableNamingConvention string   `xml:"variableNamingConvention,attr"`
	NumberOfEventIndicators  string   `xml:"numberOfEventIndicators,attr"`
	CoSimulation             struct {
		Text                                   string `xml:",chardata"`
		ModelIdentifier                        string `xml:"modelIdentifier,attr"`
		CanHandleVariableCommunicationStepSize string `xml:"canHandleVariableCommunicationStepSize,attr"`
		CanInterpolateInputs                   string `xml:"canInterpolateInputs,attr"`
	} `xml:"CoSimulation"`
	DefaultExperiment struct {
		Text      string `xml:",chardata"`
		StartTime string `xml:"startTime,attr"`
		StopTime  string `xml:"stopTime,attr"`
		StepSize  string `xml:"stepSize,attr"`
	} `xml:"DefaultExperiment"`
	ModelVariables struct {
		Text           string           `xml:",chardata"`
		ScalarVariable []ScalarVariable `xml:"ScalarVariable"`
	} `xml:"ModelVariables"`
	ModelStructure struct {
		Text    string `xml:",chardata"`
		Outputs struct {
			Text    string `xml:",chardata"`
			Unknown []struct {
				Text  string `xml:",chardata"`
				Index string `xml:"index,attr"`
			} `xml:"Unknown"`
		} `xml:"Outputs"`
		Derivatives struct {
			Text    string `xml:",chardata"`
			Unknown []struct {
				Text  string `xml:",chardata"`
				Index string `xml:"index,attr"`
			} `xml:"Unknown"`
		} `xml:"Derivatives"`
		InitialUnknowns struct {
			Text    string `xml:",chardata"`
			Unknown []struct {
				Text  string `xml:",chardata"`
				Index string `xml:"index,attr"`
			} `xml:"Unknown"`
		} `xml:"InitialUnknowns"`
	} `xml:"ModelStructure"`
}

// XML Detect & Imports
// ====================

type XmlFmuHandler struct{}

func (h *XmlFmuHandler) Detect(file string) *FmiModelDescription {
	data, err := getXmlData(file)
	if err != nil {
		fmt.Println(err)
		return nil
	}

	fmiMD := FmiModelDescription{}
	if err := xml.Unmarshal(data, &fmiMD); err != nil {
		fmt.Println("Could not read FMU XML from file")
		return nil
	}
	return &fmiMD
}

func getXmlData(file string) ([]byte, error) {
	xmlFile, err := os.Open(file)
	if err != nil {
		return nil, err
	}
	defer xmlFile.Close()

	data, _ := io.ReadAll(xmlFile)
	return data, nil
}

func ScalarSignal(signalGroupSpec schema_kind.SignalGroupSpec, FmiXml *FmiModelDescription) error {
	for _, signal := range signalGroupSpec.Signals {

		start := ""
		if (*signal.Annotations)["fmi_variable_start_value"] != nil {
			start = strconv.Itoa((*signal.Annotations)["fmi_variable_start_value"].(int))
		}
		if (*signal.Annotations)["fmi_value_reference"] == nil {
			return fmt.Errorf("Could not get value reference for signal %s", signal.Signal)
		}

		ScalarVariable := ScalarVariable{
			Name:           signal.Signal,
			ValueReference: strconv.Itoa((*signal.Annotations)["fmi_value_reference"].(int)),
			Causality:      (*signal.Annotations)["fmi_variable_causality"].(string),
			Real: &FmiReal{
				Start: start,
			},
		}
		FmiXml.ModelVariables.ScalarVariable = append(FmiXml.ModelVariables.ScalarVariable, ScalarVariable)
	}
	return nil
}

func buildBinarySignal(signal schema_kind.Signal, vref any, causality string, id int) ScalarVariable {

	bus_id := (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.bus_id"].(int)
	mime_type := (*signal.Annotations)["mime_type"].(string)
	encoding := (*signal.Annotations)["dse.standards.fmi-ls-binary-to-text.encoding"].(string)

	start := ""
	if (*signal.Annotations)["fmi_variable_start_value"] != nil {
		start = (*signal.Annotations)["fmi_variable_start_value"].(string)
	}

	var _causality string
	if causality == "rx" {
		_causality = "input"
	} else {
		_causality = "output"
	}

	tool := []Tool{
		{
			Name: "dse.standards.fmi-ls-binary-codec",
			Annotation: &[]Annotation{
				{Name: "mimetype", Text: mime_type},
			},
		},
		{
			Name: "dse.standards.fmi-ls-binary-to-text",
			Annotation: &[]Annotation{
				{Name: "encoding", Text: encoding},
			},
		},
		{
			Name: "dse.standards.fmi-ls-bus-topology",
			Annotation: &[]Annotation{
				{Name: "bus_id", Text: strconv.Itoa(bus_id)},
			},
		},
	}
	ann := []Annotations{{Tool: &tool}}
	ScalarVariable := ScalarVariable{
		Name:           fmt.Sprintf("network_%d_%d_%s", bus_id, id, causality),
		ValueReference: strconv.Itoa(vref.(int)),
		Causality:      _causality,
		String: &FmiString{
			Start: start,
		},
		Annotations: &ann,
	}
	return ScalarVariable
}

func BinarySignal(signalGroupSpec schema_kind.SignalGroupSpec, FmiXml *FmiModelDescription) error {
	for _, signal := range signalGroupSpec.Signals {
		var rx_vref []interface{}
		var tx_vref []interface{}
		if rx_vref := (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.rx_vref"]; rx_vref == nil {
			return fmt.Errorf("Could not get rx_vref for signal %s", signal.Signal)
		}
		if tx_vref := (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.tx_vref"]; tx_vref == nil {
			return fmt.Errorf("Could not get tx_vref for signal %s", signal.Signal)
		}
		if len(rx_vref) != len(tx_vref) {
			return errors.New("rx-tx mismatch")
		}
		for x := 0; x < len(rx_vref); x++ {
			rx := rx_vref[x]
			tx := tx_vref[x]

			FmiXml.ModelVariables.ScalarVariable = append(
				FmiXml.ModelVariables.ScalarVariable,
				buildBinarySignal(signal, rx, "rx", x+1),
				buildBinarySignal(signal, tx, "tx", x+1),
			)
		}

	}
	return nil
}
