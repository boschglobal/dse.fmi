// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi2"
	"github.com/boschglobal/dse.fmi/extra/tools/fmi/pkg/fmi/fmi3"
	schemaKind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"gopkg.in/yaml.v3"
)

// testdataPath returns the absolute path to the fmigateway testdata directory.
func gatewayTestdataPath(t *testing.T) string {
	t.Helper()
	abs, err := filepath.Abs("../../../test/testdata/fmigateway")
	require.NoError(t, err)
	return abs
}

// sgList joins the named SG yamls into a comma-separated path list suitable
// for --signalgroups.
func sgList(base string, names ...string) string {
	paths := make([]string, len(names))
	for i, n := range names {
		paths[i] = filepath.Join(base, n)
	}
	return strings.Join(paths, ",")
}

// simerCtx holds the output paths for a simer-mode test run.
type simerCtx struct {
	outdir  string
	simpath string
	name    string
}

// runSimer runs the command in simer mode and returns a simerCtx with the output paths.
func runSimer(t *testing.T, args ...string) simerCtx {
	t.Helper()
	outdir := t.TempDir()
	simpath := t.TempDir()
	pkgdir := t.TempDir()

	// Determine the FMU name from args (default matches the command default).
	name := "fmugw"
	for i, a := range args {
		if a == "-name" && i+1 < len(args) {
			name = args[i+1]
		}
	}

	// Create fake gateway binaries so the FMU package step can copy them.
	libDir := filepath.Join(pkgdir, "lib")
	require.NoError(t, os.MkdirAll(libDir, 0755))
	for _, lib := range []string{"libfmi2gateway.so", "libfmi3gateway.so"} {
		require.NoError(t, os.WriteFile(filepath.Join(libDir, lib), []byte("fake"), 0644))
	}

	base := []string{"-runtime", "simer", "-outdir", outdir, "-sim", simpath, "-fmiPackage", pkgdir}
	cmd := NewFmiGatewayCommand("test")
	require.NoError(t, cmd.Parse(append(base, args...)))
	require.NoError(t, cmd.Run())
	return simerCtx{outdir: outdir, simpath: simpath, name: name}
}

// ---- helpers to read generated output -----------------------------------------------

func readFmi2Xml(t *testing.T, ctx simerCtx) fmi2.ModelDescription {
	t.Helper()
	handler := fmi2.XmlFmuHandler{}
	md := handler.Detect(filepath.Join(ctx.outdir, ctx.name, "modelDescription.xml"))
	require.NotNil(t, md, "modelDescription.xml must exist and be valid FMI2")
	return *md
}

func readFmi3Xml(t *testing.T, ctx simerCtx) fmi3.ModelDescription {
	t.Helper()
	handler := fmi3.XmlFmuHandler{}
	md := handler.Detect(filepath.Join(ctx.outdir, ctx.name, "modelDescription.xml"))
	require.NotNil(t, md, "modelDescription.xml must exist and be valid FMI3")
	return *md
}

func readSignalGroups(t *testing.T, ctx simerCtx) []schemaKind.SignalGroup {
	t.Helper()
	dataDir := filepath.Join(ctx.simpath, "model", ctx.name, "data")
	data, err := os.ReadFile(filepath.Join(dataDir, "fmu.yaml"))
	require.NoError(t, err, "fmu.yaml must exist")

	// fmu.yaml contains multiple YAML documents separated by "---"
	var groups []schemaKind.SignalGroup
	dec := yaml.NewDecoder(strings.NewReader(string(data)))
	for {
		var sg schemaKind.SignalGroup
		err := dec.Decode(&sg)
		if err != nil {
			break
		}
		if sg.Kind != "" {
			groups = append(groups, sg)
		}
	}
	return groups
}

func readModelYaml(t *testing.T, ctx simerCtx) schemaKind.Model {
	t.Helper()
	dataDir := filepath.Join(ctx.simpath, "model", ctx.name, "data")
	data, err := os.ReadFile(filepath.Join(dataDir, "model.yaml"))
	require.NoError(t, err, "model.yaml must exist")
	var m schemaKind.Model
	require.NoError(t, yaml.Unmarshal(data, &m))
	return m
}

// ---- Tests --------------------------------------------------------------------------

// TestGatewayFmu_FMI2_AllSignalGroups exercises all 6 SG yamls with FMI 2 (simer mode).
func TestGatewayFmu_FMI2_AllSignalGroups(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "testgw",
		"-signalgroups", sgList(base, "SG1.yaml", "SG2.yaml", "SG3.yaml", "SG4.yaml", "SG5.yaml", "SG6.yaml"),
	)

	md := readFmi2Xml(t, ctx)
	assert.Equal(t, "2.0", md.FmiVersion)
	assert.Equal(t, "testgw", md.ModelName)
	assert.Equal(t, "fmi2gateway", md.CoSimulation.ModelIdentifier)

	// Built-in simer real + string parameters are always first (2 entries).
	simerParamCount := 2
	assert.GreaterOrEqual(t, len(md.ModelVariables.ScalarVariable), simerParamCount,
		"at least the two built-in simer parameters must be present")

	// Verify unique value references across all scalar variables.
	vrefs := map[string]string{}
	for _, v := range md.ModelVariables.ScalarVariable {
		prev, dup := vrefs[v.ValueReference]
		assert.False(t, dup, "duplicate valueReference %s between %q and %q", v.ValueReference, prev, v.Name)
		vrefs[v.ValueReference] = v.Name
	}

	// fmu.yaml must contain one document per signal group loaded (6).
	sgs := readSignalGroups(t, ctx)
	assert.Len(t, sgs, 6)

	// model.yaml must name Gateway.
	m := readModelYaml(t, ctx)
	require.NotNil(t, m.Metadata)
	assert.Equal(t, "Gateway", *m.Metadata.Name)
	require.NotNil(t, m.Spec.Channels)
	assert.NotEmpty(t, *m.Spec.Channels)
}

// TestGatewayFmu_FMI3_AllSignalGroups exercises all 6 SGs with FMI 3 (simer mode).
func TestGatewayFmu_FMI3_AllSignalGroups(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "3",
		"-name", "testgw3",
		"-signalgroups", sgList(base, "SG1.yaml", "SG2.yaml", "SG3.yaml", "SG4.yaml", "SG5.yaml", "SG6.yaml"),
	)

	md := readFmi3Xml(t, ctx)
	assert.Equal(t, "3.0", md.FmiVersion)
	assert.Equal(t, "testgw3", md.ModelName)
	assert.Equal(t, "fmi3gateway", md.CoSimulation.ModelIdentifier)

	// Unique value references across float64 variables.
	vrefs := map[string]string{}
	if md.ModelVariables.Float64 != nil {
		for _, v := range *md.ModelVariables.Float64 {
			prev, dup := vrefs[v.ValueReference]
			assert.False(t, dup, "duplicate valueReference %s between %q and %q", v.ValueReference, prev, v.Name)
			vrefs[v.ValueReference] = v.Name
		}
	}
}

// TestGatewayFmu_FMI2_WithStack exercises FMI2 with the stack.yaml (simer mode).
// The stack contains a Gateway model with step_size, end_time and cmd_envvars.
func TestGatewayFmu_FMI2_WithStack(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "stackgw",
		"-stack", filepath.Join(base, "stack.yaml"),
		"-signalgroups", sgList(base, "SG4.yaml", "SG5.yaml"),
	)

	md := readFmi2Xml(t, ctx)
	assert.Equal(t, "stackgw", md.ModelName)

	// stack.yaml Gateway has step_size: 0.0005 and end_time: 0.0200
	assert.Equal(t, "0.020000", md.DefaultExperiment.StopTime)
	assert.Equal(t, "0.000500", md.DefaultExperiment.StepSize)

	// stack.yaml Gateway has cmd_envvars: envar0 (string), envar1 (real), envar2 (no type → real).
	// In simer mode they must appear as parameter variables in the XML alongside the
	// built-in Simer_Command_Selector and Simer_Command parameters.
	paramNames := map[string]bool{}
	for _, v := range md.ModelVariables.ScalarVariable {
		if v.Causality == "parameter" {
			paramNames[v.Name] = true
		}
	}
	assert.True(t, paramNames["envar0"], "envar0 (string) parameter must exist")
	assert.True(t, paramNames["envar1"], "envar1 (real) parameter must exist")
	// envar2 has no type → default real
	assert.True(t, paramNames["envar2"], "envar2 (default-real) parameter must exist")
	// Simer built-in parameters must also be present.
	assert.True(t, paramNames["Simer_Command_Selector"], "Simer_Command_Selector must exist in simer mode")
	assert.True(t, paramNames["Simer_Command"], "Simer_Command must exist in simer mode")
}

// TestGatewayFmu_FMI2_WithParameters exercises the --parameters CSV (simer mode).
func TestGatewayFmu_FMI2_WithParameters(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "paramgw",
		"-signalgroups", sgList(base, "SG4.yaml"),
		"-parameters", filepath.Join(base, "parameters.csv"),
	)

	md := readFmi2Xml(t, ctx)

	// parameters.csv contains: param1 (Real), param2 (String).
	paramNames := map[string]bool{}
	for _, v := range md.ModelVariables.ScalarVariable {
		if v.Causality == "parameter" {
			paramNames[v.Name] = true
		}
	}
	assert.True(t, paramNames["param1"], "param1 from CSV must appear as a parameter variable")
	assert.True(t, paramNames["param2"], "param2 from CSV must appear as a parameter variable")
}

// TestGatewayFmu_FMI2_DuplicateSignalsDeduped verifies that a signal name
// referenced in multiple SGs is only patched once (via the usedSignals guard).
func TestGatewayFmu_FMI2_DuplicateSignalsDeduped(t *testing.T) {
	base := gatewayTestdataPath(t)
	// SG1 and SG2 both contain softecu_name_1_changed and softecu_name_2_changed.
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "dedupgw",
		"-signalgroups", sgList(base, "SG1.yaml", "SG2.yaml"),
	)

	md := readFmi2Xml(t, ctx)

	// Count occurrences of the duplicate signal names.
	countByName := map[string]int{}
	for _, v := range md.ModelVariables.ScalarVariable {
		countByName[v.Name]++
	}
	assert.Equal(t, 1, countByName["softecu_name_1_changed"],
		"softecu_name_1_changed must appear exactly once")
	assert.Equal(t, 1, countByName["softecu_name_2_changed"],
		"softecu_name_2_changed must appear exactly once")
}

// TestGatewayFmu_FMI2_BinarySignalGroup verifies that SG6 (vector_type: binary)
// contributes String variables with the network_<bus_id>_<id>_<dir> naming pattern.
func TestGatewayFmu_FMI2_BinarySignalGroup(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "bingw",
		"-signalgroups", sgList(base, "SG6.yaml"),
	)

	md := readFmi2Xml(t, ctx)

	// BinarySignal produces variables named network_{bus_id}_{idx}_{rx|tx}.
	// SG6.yaml has bus_id=1, one signal → expect network_1_1_rx and network_1_1_tx.
	varNames := map[string]bool{}
	for _, v := range md.ModelVariables.ScalarVariable {
		varNames[v.Name] = true
	}
	assert.True(t, varNames["network_1_1_rx"], "binary SG6 must produce network_1_1_rx")
	assert.True(t, varNames["network_1_1_tx"], "binary SG6 must produce network_1_1_tx")
}

// TestGatewayFmu_FMI2_LocalSignalsSkipped verifies that signals with
// fmi_variable_causality: local in SG1 are excluded from the output.
func TestGatewayFmu_FMI2_LocalSignalsSkipped(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "localgw",
		"-signalgroups", sgList(base, "SG1.yaml"),
	)

	md := readFmi2Xml(t, ctx)

	for _, v := range md.ModelVariables.ScalarVariable {
		assert.NotEqual(t, "local", v.Causality,
			"local-causality signals (%s) must not appear in the XML", v.Name)
	}
	localNames := []string{"local_1", "local_2", "local_3"}
	varNames := map[string]bool{}
	for _, v := range md.ModelVariables.ScalarVariable {
		varNames[v.Name] = true
	}
	for _, ln := range localNames {
		assert.False(t, varNames[ln], "local signal %s must not appear in XML", ln)
	}
}

// TestGatewayFmu_UnsupportedFmiVersion verifies the default case in createFmuXml.
func TestGatewayFmu_UnsupportedFmiVersion(t *testing.T) {
	base := gatewayTestdataPath(t)
	outdir := t.TempDir()
	simpath := t.TempDir()
	cmd := NewFmiGatewayCommand("test")
	require.NoError(t, cmd.Parse([]string{
		"-runtime", "simer",
		"-outdir", outdir,
		"-sim", simpath,
		"-fmiVersion", "99",
		"-signalgroups", sgList(base, "SG4.yaml"),
	}))
	err := cmd.Run()
	require.Error(t, err)
	assert.Contains(t, err.Error(), "unsupported FMI version")
}

// TestGatewayFmu_FMI3_WithStack exercises FMI3 with the stack.yaml.
func TestGatewayFmu_FMI3_WithStack(t *testing.T) {
	base := gatewayTestdataPath(t)
	ctx := runSimer(t,
		"-fmiVersion", "3",
		"-name", "stackgw3",
		"-stack", filepath.Join(base, "stack.yaml"),
		"-signalgroups", sgList(base, "SG4.yaml", "SG5.yaml"),
	)

	md := readFmi3Xml(t, ctx)
	assert.Equal(t, "3.0", md.FmiVersion)
	assert.Equal(t, "stackgw3", md.ModelName)

	// Same step/stop time assertions as FMI2 case.
	m := readModelYaml(t, ctx)
	require.NotNil(t, m.Spec.Channels)
	assert.NotEmpty(t, *m.Spec.Channels)
}

// TestGatewayFmu_FMI2_ModelChannels verifies that the model.yaml channels
// match what the signal group labels declare.
func TestGatewayFmu_FMI2_ModelChannels(t *testing.T) {
	base := gatewayTestdataPath(t)
	// SG4 → channel "E2M_M2E", SG5 → channel "com_phys"
	ctx := runSimer(t,
		"-fmiVersion", "2",
		"-name", "channelgw",
		"-signalgroups", sgList(base, "SG4.yaml", "SG5.yaml"),
	)

	m := readModelYaml(t, ctx)
	require.NotNil(t, m.Spec.Channels)
	channels := *m.Spec.Channels

	aliases := make([]string, len(channels))
	for i, ch := range channels {
		require.NotNil(t, ch.Alias)
		aliases[i] = *ch.Alias
	}
	assert.ElementsMatch(t, []string{"E2M_M2E", "com_phys"}, aliases)
}
