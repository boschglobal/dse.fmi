// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi2

import (
	"encoding/xml"
	"errors"
	"flag"
	"fmt"
	"io"
	"log/slog"
	"os"
	"slices"
	"strconv"
	"strings"
	"time"

	"github.com/boschglobal/dse.clib/extra/go/file/index"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

func stringPtr(v string) *string { return &v }

// FMI XML struct/tags
// ===================

type Annotations struct {
	Tool []Tool `xml:"Tool"`
}

type VendorAnnotations struct {
	Tool []VendorTool `xml:"Tool"`
}

type VendorTool struct {
	Name       string `xml:"name,attr"`
	Annotation struct {
		InnerXML string `xml:",innerxml"`
	} `xml:"annotation"`
}

type Tool struct {
	Name       string       `xml:"name,attr"`
	Annotation []Annotation `xml:"Annotation,omitempty"`
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
	Start string `xml:"start,attr"`
}

type FmiBoolean struct {
	Start string `xml:"start,attr,omitempty"`
}

type ScalarVariable struct {
	Text           string       `xml:",chardata"`
	Name           string       `xml:"name,attr"`
	ValueReference string       `xml:"valueReference,attr"`
	Causality      string       `xml:"causality,attr"`
	Initial        *string      `xml:"initial,attr,omitempty"`
	Variability    *string      `xml:"variability,attr,omitempty"`
	Real           *FmiReal     `xml:"Real,omitempty"`
	String         *FmiString   `xml:"String,omitempty"`
	Boolean        *FmiBoolean  `xml:"Boolean,omitempty"`
	Integer        *FmiInteger  `xml:"Integer,omitempty"`
	Annotations    *Annotations `xml:"Annotations,omitempty"`
}

type Unknown struct {
	Text  string `xml:",chardata"`
	Index string `xml:"index,attr"`
}

type Outputs struct {
	Text    string    `xml:",chardata"`
	Unknown []Unknown `xml:"Unknown,omitempty"`
}

type Derivates struct {
	Text    string    `xml:",chardata"`
	Unknown []Unknown `xml:"Unknown,omitempty"`
}

type InitialUnknowns struct {
	Text    string    `xml:",chardata"`
	Unknown []Unknown `xml:"Unknown,omitempty"`
}

// ModelDescription was generated 2023-05-23 07:27:35 by https://xml-to-go.github.io/ in Ukraine.
type ModelDescription struct {
	XMLName                  xml.Name `xml:"fmiModelDescription"`
	Text                     string   `xml:",chardata"`
	FmiVersion               string   `xml:"fmiVersion,attr"`
	ModelName                string   `xml:"modelName,attr"`
	Guid                     string   `xml:"guid,attr"`
	Description              string   `xml:"description,attr"`
	Author                   string   `xml:"author,attr"`
	Version                  string   `xml:"version,attr"`
	GenerationTool           *string  `xml:"generationTool,attr,omitempty"`
	GenerationDateAndTime    *string  `xml:"generationDateAndTime,attr,omitempty"`
	VariableNamingConvention *string  `xml:"variableNamingConvention,attr,omitempty"`
	NumberOfEventIndicators  *string  `xml:"numberOfEventIndicators,attr,omitempty"`
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
	VendorAnnotations *VendorAnnotations `xml:"VendorAnnotations,omitempty"`
	ModelVariables    struct {
		Text           string           `xml:",chardata"`
		ScalarVariable []ScalarVariable `xml:"ScalarVariable"`
	} `xml:"ModelVariables"`
	ModelStructure struct {
		Text            string           `xml:",chardata"`
		Outputs         *Outputs         `xml:"Outputs,omitempty"`
		Derivatives     *Derivates       `xml:"Derivatives,omitempty"`
		InitialUnknowns *InitialUnknowns `xml:"InitialUnknowns,omitempty"`
	} `xml:"ModelStructure"`
}

// XML Detect & Imports
// ====================

type XmlFmuHandler struct{}

func (h *XmlFmuHandler) Detect(file string) *ModelDescription {
	data, err := getXmlData(file)
	if err != nil {
		fmt.Println(err)
		return nil
	}

	fmiMD := ModelDescription{}
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

func expandModelStructure(fmiXml *ModelDescription, vref string) error {
	output := Unknown{
		Index: vref,
	}
	if fmiXml.ModelStructure.Outputs == nil {
		fmiXml.ModelStructure.Outputs = &Outputs{}
	}
	if fmiXml.ModelStructure.InitialUnknowns == nil {
		fmiXml.ModelStructure.InitialUnknowns = &InitialUnknowns{}
	}
	fmiXml.ModelStructure.Outputs.Unknown = append(fmiXml.ModelStructure.Outputs.Unknown, output)
	fmiXml.ModelStructure.InitialUnknowns.Unknown = append(fmiXml.ModelStructure.InitialUnknowns.Unknown, output)

	return nil
}

func ScalarSignal(FmiXml *ModelDescription, signalGroupSpec schema_kind.SignalGroupSpec) error {
	for _, signal := range signalGroupSpec.Signals {
		v := (*signal.Annotations)["fmi_variable_causality"]
		if v == nil {
			continue
		}
		causality := v.(string)
		start := ""
		var variability *string = nil
		if causality == "input" || causality == "parameter" {
			start = "0.0"
		}
		if causality == "parameter" {
			variability = stringPtr("tunable")
		}

		if v = (*signal.Annotations)["fmi_variable_start_value"]; v != nil {
			switch v := v.(type) {
			case int:
				start = strconv.Itoa(v)
			case float32:
				start = fmt.Sprintf("%f", v)
			case float64:
				start = fmt.Sprintf("%f", v)
			}
		}
		if v = (*signal.Annotations)["fmi_variable_vref"]; v != nil {
			vRef := ""
			switch v := v.(type) {
			case string:
				vRef = v
			case int:
				vRef = strconv.Itoa(v)
			}
			ScalarVariable := ScalarVariable{
				Name:           signal.Signal,
				ValueReference: vRef,
				Causality:      causality,
				Variability:    variability,
				Real: &FmiReal{
					Start: start,
				},
			}
			FmiXml.ModelVariables.ScalarVariable = append(FmiXml.ModelVariables.ScalarVariable, ScalarVariable)

			if causality == "output" {
				expandModelStructure(FmiXml, vRef)
			}
		} else {
			return fmt.Errorf("could not get value reference for signal %s", signal.Signal)
		}
	}
	return nil
}

func buildBinarySignal(FmiXml *ModelDescription, signal schema_kind.Signal, vref string, causality string, id int) ScalarVariable {

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
	var variability *string = stringPtr("discrete")

	tool := []Tool{
		{
			Name: "dse.standards.fmi-ls-binary-codec",
			Annotation: []Annotation{
				{Name: "mimetype", Text: mime_type},
			},
		},
		{
			Name: "dse.standards.fmi-ls-binary-to-text",
			Annotation: []Annotation{
				{Name: "encoding", Text: encoding},
			},
		},
		{
			Name: "dse.standards.fmi-ls-bus-topology",
			Annotation: []Annotation{
				{Name: "bus_id", Text: strconv.Itoa(bus_id)},
			},
		},
	}
	ann := Annotations{Tool: tool}
	ScalarVariable := ScalarVariable{
		Name:           fmt.Sprintf("network_%d_%d_%s", bus_id, id, causality),
		ValueReference: vref,
		Causality:      _causality,
		Variability:    variability,
		String: &FmiString{
			Start: start,
		},
		Annotations: &ann,
	}
	FmiXml.ModelVariables.ScalarVariable = append(FmiXml.ModelVariables.ScalarVariable, ScalarVariable)
	if _causality == "output" {
		expandModelStructure(FmiXml, vref)
	}

	return ScalarVariable
}

func BinarySignal(FmiXml *ModelDescription, signalGroupSpec schema_kind.SignalGroupSpec) error {
	for _, signal := range signalGroupSpec.Signals {
		var v any

		if v = (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.rx_vref"]; v == nil {
			return fmt.Errorf("could not get rx_vref for signal %s", signal.Signal)
		}
		rx_vref := v.([]interface{})
		if v = (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.tx_vref"]; v == nil {
			return fmt.Errorf("could not get tx_vref for signal %s", signal.Signal)
		}
		tx_vref := v.([]interface{})
		if len(rx_vref) != len(tx_vref) {
			return errors.New("rx-tx mismatch")
		}
		for x := 0; x < len(rx_vref); x++ {
			rx := strconv.Itoa(rx_vref[x].(int))
			tx := strconv.Itoa(tx_vref[x].(int))
			buildBinarySignal(FmiXml, signal, rx, "rx", x+1)
			buildBinarySignal(FmiXml, signal, tx, "tx", x+1)
		}
	}
	return nil
}

func StringSignal(FmiXml *ModelDescription, signalGroupSpec schema_kind.SignalGroupSpec) error {
	for _, signal := range signalGroupSpec.Signals {
		v := (*signal.Annotations)["fmi_variable_causality"]
		if v == nil {
			continue
		}
		causality := v.(string)

		start := ""
		if v = (*signal.Annotations)["fmi_variable_start_value"]; v != nil {
			if causality == "input" || causality == "parameter" {
				start = v.(string)
			}
		}

		var variability *string = nil
		if causality == "parameter" {
			variability = stringPtr("tunable")
		}

		if v = (*signal.Annotations)["fmi_variable_vref"]; v != nil {
			ScalarVariable := ScalarVariable{
				Name:           signal.Signal,
				ValueReference: strconv.Itoa((*signal.Annotations)["fmi_variable_vref"].(int)),
				Causality:      causality,
				Variability:    variability,
				String: &FmiString{
					Start: start,
				},
			}
			FmiXml.ModelVariables.ScalarVariable = append(FmiXml.ModelVariables.ScalarVariable, ScalarVariable)
		} else {
			return fmt.Errorf("could not get value reference for signal %s", signal.Signal)
		}

	}
	return nil
}

func SetGeneralFmuXmlFields(fmiConfig fmi.FmiConfig, fmuXml *ModelDescription) error {
	// Basic FMU Information.
	fmuXml.ModelName = fmiConfig.Name
	fmuXml.FmiVersion = fmt.Sprintf("%s.0", fmiConfig.FmiVersion)
	fmuXml.Guid = fmt.Sprintf("{%s}", fmiConfig.UUID)
	fmuXml.GenerationTool = stringPtr(fmiConfig.GenerationTool)
	fmuXml.GenerationDateAndTime = stringPtr(time.Now().Format("2006-01-02T15:04:05Z"))
	fmuXml.Author = "Robert Bosch GmbH"
	fmuXml.Version = fmiConfig.Version
	fmuXml.Description = fmiConfig.Description

	// Runtime Information.
	fmuXml.DefaultExperiment.StartTime = fmt.Sprintf("%f", fmiConfig.StartTime)
	fmuXml.DefaultExperiment.StopTime = fmt.Sprintf("%f", fmiConfig.StopTime)
	fmuXml.DefaultExperiment.StepSize = fmt.Sprintf("%f", fmiConfig.StepSize)

	// Model Information.
	fmuXml.CoSimulation.ModelIdentifier = fmiConfig.ModelIdentifier // This is the packaged SO/DLL file.
	fmuXml.CoSimulation.CanHandleVariableCommunicationStepSize = "true"
	fmuXml.CoSimulation.CanInterpolateInputs = "true"

	// Add VendorAnnotations if provided.
	fmuXml.VendorAnnotations = buildAnnotations(fmiConfig.Annotations)

	return nil
}

// Helper function to build vendor annotations from map
func buildAnnotations(annotationsMap map[string]string) *VendorAnnotations {
	if len(annotationsMap) == 0 {
		return nil
	}
	var xmlContent strings.Builder
	for key, value := range annotationsMap {
		// Escape XML special characters
		escapedKey := escapeXML(key)
		escapedValue := escapeXML(value)
		xmlContent.WriteString(fmt.Sprintf("\n\t\t\t\t<%s>%s</%s>", escapedKey, escapedValue, escapedKey))
	}
	xmlContent.WriteString("\n\t\t\t")

	tool := VendorTool{
		Name: "dse.fmi.config",
	}
	tool.Annotation.InnerXML = xmlContent.String()

	return &VendorAnnotations{
		Tool: []VendorTool{tool},
	}
}

// Helper function to escape XML special characters
func escapeXML(s string) string {
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	s = strings.ReplaceAll(s, "\"", "&quot;")
	s = strings.ReplaceAll(s, "'", "&apos;")
	return s
}

func VariablesFromSignalgroups(
	FmiXml *ModelDescription, signalGroups string, index *index.YamlFileIndex) ([]string, error) {
	var channels []string
	if signalGroupDocs, ok := index.DocMap["SignalGroup"]; ok {
		filter := []string{}
		if len(signalGroups) > 0 {
			filter = strings.Split(signalGroups, ",")
		}
		for _, doc := range signalGroupDocs {
			if len(filter) > 0 && !slices.Contains(filter, doc.Metadata.Name) {
				continue
			}
			fmt.Fprintf(flag.CommandLine.Output(), "Adding SignalGroup: %s (%s)\n", doc.Metadata.Name, doc.File)
			signalGroupSpec := doc.Spec.(*schema_kind.SignalGroupSpec)
			if doc.Metadata.Annotations["vector_type"] == "binary" {
				if err := BinarySignal(FmiXml, *signalGroupSpec); err != nil {
					slog.Warn(fmt.Sprintf("Skipped BinarySignal (Error: %s)", err.Error()))
				}
			} else {
				if err := ScalarSignal(FmiXml, *signalGroupSpec); err != nil {
					slog.Warn(fmt.Sprintf("Skipped Scalarsignal (Error: %s)", err.Error()))
				}
			}
			if !slices.Contains(channels, doc.Metadata.Labels["channel"]) {
				channels = append(channels, doc.Metadata.Labels["channel"])
				slog.Debug(fmt.Sprintf("added %s", doc.Metadata.Labels["channel"]))
			}
		}
	}
	return channels, nil
}
