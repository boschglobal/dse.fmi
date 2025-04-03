// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi3

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

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/index"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/fmi"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

func stringPtr(v string) *string { return &v }

// FMI XML struct/tags
// ===================

type Annotations struct {
	Annotation []Annotation `xml:"Annotation"`
}

type AnnotationField struct {
	Text string `xml:",chardata"`
}

type Annotation struct {
	Type     string           `xml:"type,attr"`
	Text     string           `xml:",chardata"`
	MimeType *AnnotationField `xml:"Mimetype,omitempty"`
	Encoding *AnnotationField `xml:"Encoding,omitempty"`
	Bus_id   *AnnotationField `xml:"Bus_id,omitempty"`
}

type Outputs struct {
	Text    string `xml:",chardata"`
	Unknown []struct {
		Text  string `xml:",chardata"`
		Index string `xml:"index,attr"`
	} `xml:"Unknown"`
}

type InitialUnknowns struct {
	Text    string `xml:",chardata"`
	Unknown []struct {
		Text  string `xml:",chardata"`
		Index string `xml:"index,attr"`
	} `xml:"Unknown"`
}

type CoSimulation struct {
	Text                                string  `xml:",chardata"`
	ModelIdentifier                     string  `xml:"modelIdentifier,attr"`
	NeedsExecutionTool                  *string `xml:"needsExecutionTool,attr,omitempty"`
	CanBeInstantiatedOnlyOncePerProcess *string `xml:"canBeInstantiatedOnlyOncePerProcess,attr,omitempty"`
	CanGetAndSetFMUState                *string `xml:"canGetAndSetFMUState,attr,omitempty"`
	CanSerializeFMUState                *string `xml:"canSerializeFMUState,attr,omitempty"`
	ProvidesDirectionalDerivatives      *string `xml:"providesDirectionalDerivatives,attr,omitempty"`
	ProvidesAdjointDerivatives          *string `xml:"providesAdjointDerivatives,attr,omitempty"`
	ProvidesPerElementDependencies      *string `xml:"providesPerElementDependencies,attr,omitempty"`
	ProvidesEvaluateDiscreteStates      *string `xml:"providesEvaluateDiscreteStates,attr,omitempty"`
}

type LogCategories struct {
	Text     string `xml:",chardata"`
	Category []struct {
		Text        string `xml:",chardata"`
		Name        string `xml:"name,attr"`
		Description string `xml:"description,attr"`
	}
}

type DefaultExperiment struct {
	Text      string `xml:",chardata"`
	StartTime string `xml:"startTime,attr"`
	StopTime  string `xml:"stopTime,attr"`
	StepSize  string `xml:"stepSize,attr"`
	Tolerance string `xml:"tolerance,attr,omitempty"`
}

type Start struct {
	Text  string `xml:",chardata"`
	Value string `xml:"value,attr"`
}

type Alias struct {
	Text        string  `xml:",chardata"`
	Name        string  `xml:"name,attr"`
	Description string  `xml:"description,attr"`
	DisplayUnit *string `xml:"displayUnit,attr"`
}

type Binary struct {
	Text                               string       `xml:",chardata"`
	Name                               string       `xml:"name,attr"`
	ValueReference                     string       `xml:"valueReference,attr"`
	Description                        string       `xml:"description,attr,omitempty"`
	Causality                          string       `xml:"causality,attr"`
	Variability                        *string      `xml:"variability,attr,omitempty"`
	CanHandleMultipleSetPerTimeInstant *string      `xml:"canHandleMultipleSetPerTimeInstant,attr,omitempty"`
	Clocks                             *string      `xml:"clocks,attr,omitempty"`
	IntermediateUpdate                 *string      `xml:"intermediateUpdate,attr,omitempty"`
	Previous                           *string      `xml:"previous,attr,omitempty"`
	DeclaredType                       *string      `xml:"declaredType,attr,omitempty"`
	Initial                            *string      `xml:"initial,attr,omitempty"`
	MimeType                           *string      `xml:"mimeType,attr,omitempty"`
	MaxSize                            *string      `xml:"maxSize,attr,omitempty"`
	Annotations                        *Annotations `xml:"Annotations,omitempty"`
	Start                              *Start       `xml:"Start,omitempty"`
}

type Float struct {
	Text                               string       `xml:",chardata"`
	Name                               string       `xml:"name,attr"`
	ValueReference                     string       `xml:"valueReference,attr"`
	Description                        string       `xml:"description,attr,omitempty"`
	Causality                          string       `xml:"causality,attr"`
	Variability                        *string      `xml:"variability,attr,omitempty"`
	CanHandleMultipleSetPerTimeInstant *string      `xml:"canHandleMultipleSetPerTimeInstant,attr,omitempty"`
	Clocks                             *string      `xml:"clocks,attr,omitempty"`
	IntermediateUpdate                 *string      `xml:"intermediateUpdate,attr,omitempty"`
	Previous                           *string      `xml:"previous,attr,omitempty"`
	DeclaredType                       *string      `xml:"declaredType,attr,omitempty"`
	Initial                            *string      `xml:"initial,attr,omitempty"`
	Quantity                           *string      `xml:"quantity,attr,omitempty"`
	Unit                               *string      `xml:"unit,attr,omitempty"`
	DisplayUnit                        *string      `xml:"displayUnit,attr,omitempty"`
	RelativeQuantity                   *string      `xml:"relativeQuantity,attr,omitempty"`
	Unbounded                          *string      `xml:"unbounded,attr,omitempty"`
	Min                                *string      `xml:"min,attr,omitempty"`
	Max                                *string      `xml:"max,attr,omitempty"`
	Nominal                            *string      `xml:"nominal,attr,omitempty"`
	Start                              string       `xml:"start,attr,omitempty"`
	Derivate                           *string      `xml:"derivate,attr,omitempty"`
	Reinit                             *string      `xml:"reinit,attr,omitempty"`
	Annotations                        *Annotations `xml:"Annotations,omitempty"`
}

type ModelVariables struct {
	Text    string    `xml:",chardata"`
	Float32 *[]Float  `xml:"Float32,omitempty"`
	Float64 *[]Float  `xml:"Float64,omitempty"`
	Binary  *[]Binary `xml:"Binary,omitempty"`
}

type ModelStructure struct {
	Text            string           `xml:",chardata"`
	Outputs         *Outputs         `xml:"Outputs,omitempty"`
	InitialUnknowns *InitialUnknowns `xml:"InitialUnknowns,omitempty"`
}

type ModelDescription struct {
	XMLName                  xml.Name          `xml:"fmiModelDescription"`
	Text                     string            `xml:",chardata"`
	FmiVersion               string            `xml:"fmiVersion,attr"`
	ModelName                string            `xml:"modelName,attr"`
	InstantiationToken       string            `xml:"instantiationToken,attr"`
	Description              string            `xml:"description,attr"`
	Author                   string            `xml:"author,attr"`
	Version                  string            `xml:"version,attr"`
	Copyright                string            `xml:"copyright,attr,omitempty"`
	License                  string            `xml:"license,attr,omitempty"`
	GenerationTool           *string           `xml:"generationTool,attr,omitempty"`
	GenerationDateAndTime    *string           `xml:"generationDateAndTime,attr,omitempty"`
	VariableNamingConvention *string           `xml:"variableNamingConvention,attr,omitempty"`
	CoSimulation             CoSimulation      `xml:"CoSimulation"`
	LogCategories            *LogCategories    `xml:"LogCategories,omitempty"`
	DefaultExperiment        DefaultExperiment `xml:"DefaultExperiment"`
	ModelVariables           ModelVariables    `xml:"ModelVariables"`
	ModelStructure           ModelStructure    `xml:"ModelStructure"`
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

func ScalarSignal(signalGroupSpec schema_kind.SignalGroupSpec, FmiXml *ModelDescription) error {
	for _, signal := range signalGroupSpec.Signals {

		causality := (*signal.Annotations)["fmi_variable_causality"].(string)
		start := ""
		var variability *string = nil
		if causality == "input" || causality == "parameter" {
			start = "0.0"
		}
		if causality == "parameter" {
			variability = stringPtr("tunable")
		}

		if (*signal.Annotations)["fmi_variable_start_value"] != nil {
			start = strconv.Itoa((*signal.Annotations)["fmi_variable_start_value"].(int))
		}
		if (*signal.Annotations)["fmi_variable_vref"] == nil {
			return fmt.Errorf("could not get value reference for signal %s", signal.Signal)
		}

		ScalarVariable := Float{
			Name:           signal.Signal,
			ValueReference: strconv.Itoa((*signal.Annotations)["fmi_variable_vref"].(int)),
			Causality:      causality,
			Variability:    variability,
			Start:          start,
		}
		*(FmiXml.ModelVariables.Float64) = append(*(FmiXml.ModelVariables.Float64), ScalarVariable)
	}
	return nil
}

func buildBinarySignal(signal schema_kind.Signal, vref any, causality string, id int) Binary {

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

	annotation := []Annotation{
		{
			Type: "dse.standards.fmi-ls-binary-codec",
			MimeType: &AnnotationField{
				Text: mime_type,
			},
		},
		{
			Type: "dse.standards.fmi-ls-binary-to-text",
			Encoding: &AnnotationField{
				Text: encoding,
			},
		},
		{
			Type: "dse.standards.fmi-ls-bus-topology",
			Bus_id: &AnnotationField{
				Text: strconv.Itoa(bus_id),
			},
		},
	}
	ann := Annotations{Annotation: annotation}

	ScalarVariable := Binary{
		Name:           fmt.Sprintf("network_%d_%d_%s", bus_id, id, causality),
		ValueReference: strconv.Itoa(vref.(int)),
		Causality:      _causality,
		Start: &Start{
			Value: start,
		},
		Annotations: &ann,
	}
	return ScalarVariable
}

func BinarySignal(signalGroupSpec schema_kind.SignalGroupSpec, FmiXml *ModelDescription) error {
	binary := []Binary{}
	for _, signal := range signalGroupSpec.Signals {
		var rx_vref []interface{}
		var tx_vref []interface{}
		if rx_vref = (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.rx_vref"].([]interface{}); rx_vref == nil {
			return fmt.Errorf("could not get rx_vref for signal %s", signal.Signal)
		}
		if tx_vref = (*signal.Annotations)["dse.standards.fmi-ls-bus-topology.tx_vref"].([]interface{}); tx_vref == nil {
			return fmt.Errorf("could not get tx_vref for signal %s", signal.Signal)
		}
		if len(rx_vref) != len(tx_vref) {
			return errors.New("rx-tx mismatch")
		}
		for x := 0; x < len(rx_vref); x++ {
			rx := rx_vref[x]
			tx := tx_vref[x]

			binary = append(
				binary,
				buildBinarySignal(signal, rx, "rx", x+1),
				buildBinarySignal(signal, tx, "tx", x+1),
			)
		}

	}
	FmiXml.ModelVariables.Binary = &binary
	return nil
}

func SetGeneralFmuXmlFields(fmiConfig fmi.FmiConfig, fmuXml *ModelDescription) error {
	// Basic FMU Information.
	fmuXml.ModelName = fmiConfig.Name
	fmuXml.FmiVersion = fmt.Sprintf("%s.0", fmiConfig.FmiVersion)
	fmuXml.InstantiationToken = fmiConfig.UUID
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
	return nil
}

func VariablesFromSignalgroups(FmiXml *ModelDescription, signalGroups string, index *index.YamlFileIndex) error {

	float := []Float{}
	ScalarVariable := Float{
		Name:           "time",
		ValueReference: strconv.Itoa(4294967295),
		Causality:      "independent",
		Variability:    stringPtr("continuous"),
		Description:    "Simulation time",
	}
	float = append(float, ScalarVariable)
	FmiXml.ModelVariables.Float64 = &float

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
				if err := BinarySignal(*signalGroupSpec, FmiXml); err != nil {
					slog.Warn(fmt.Sprintf("Skipped BinarySignal (Error: %s)", err.Error()))
				}
			} else {
				if err := ScalarSignal(*signalGroupSpec, FmiXml); err != nil {
					slog.Warn(fmt.Sprintf("Skipped Scalarsignal (Error: %s)", err.Error()))
				}
			}
		}
	}
	return nil
}
