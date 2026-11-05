// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package fmi

import (
	"strconv"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestStartValue_RoundTrip(t *testing.T) {
	for _, v := range []float64{2.38587666666667e-5, 5.3333e-6, 161.988181818182, 1e-12, 0, 42} {
		s, ok := StartValue(v)
		assert.True(t, ok)
		parsed, err := strconv.ParseFloat(s, 64)
		assert.NoError(t, err)
		assert.Equal(t, v, parsed, "start value must round-trip (%s)", s)
	}
}

func TestStartValue_Types(t *testing.T) {
	s, ok := StartValue("2.5E-5")
	assert.True(t, ok)
	assert.Equal(t, "2.5E-5", s, "strings are passed through unchanged")

	s, _ = StartValue(float32(0.1))
	assert.Equal(t, "0.1", s)
	s, _ = StartValue(int64(-7))
	assert.Equal(t, "-7", s)
	s, _ = StartValue(uint64(7))
	assert.Equal(t, "7", s)

	_, ok = StartValue("")
	assert.False(t, ok)
	_, ok = StartValue(nil)
	assert.False(t, ok)
}

func TestStartString(t *testing.T) {
	assert.Equal(t, "", StartString(nil))
	assert.Equal(t, "abc", StartString("abc"))
	assert.Equal(t, "5", StartString(5))
	assert.Equal(t, "true", StartString(true))
}

func TestFormatReal(t *testing.T) {
	assert.Equal(t, "0.0005", FormatReal(0.0005))
	assert.Equal(t, "10", FormatReal(10))
	assert.Equal(t, "1e-07", FormatReal(1e-7))
}

func TestToFloat64(t *testing.T) {
	f, ok := ToFloat64(1)
	assert.True(t, ok)
	assert.Equal(t, 1.0, f)
	f, ok = ToFloat64(0.5)
	assert.True(t, ok)
	assert.Equal(t, 0.5, f)
	_, ok = ToFloat64("1")
	assert.False(t, ok)
}
