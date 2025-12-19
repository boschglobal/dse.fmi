// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"path"
	"testing"

	"github.com/boschglobal/dse.clib/extra/go/file/index"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"github.com/stretchr/testify/assert"
)

func TestDirectIndex_group_signal_add(t *testing.T) {
	di := NewDirectIndex()
	group := di.getSignalGroup("foo")
	for _, signal := range []string{"one", "two", "two", "three"} {
		di.addSignal(group.name, signal)
	}

	assert.Equal(t, 1, len(di.groups))
	assert.Equal(t, 1, len(di.index))

	assert.Equal(t, "foo", group.name)
	assert.Equal(t, 3, len(group.index))
	assert.Equal(t, 3, len(group.signals))
	assert.Contains(t, group.index, "one")
	assert.Contains(t, group.index, "two")
	assert.Contains(t, group.index, "three")
	assert.Equal(t, "one", group.signals[0].name)
	assert.Equal(t, "two", group.signals[1].name)
	assert.Equal(t, "three", group.signals[2].name)
}

func TestDirectIndex_calculate_map(t *testing.T) {
	di := NewDirectIndex()
	groupFoo := di.getSignalGroup("foo")
	for _, signal := range []string{"one", "two", "three"} {
		di.addSignal(groupFoo.name, signal)
	}
	groupBar := di.getSignalGroup("bar")
	for _, signal := range []string{"four", "five", "six"} {
		di.addSignal(groupBar.name, signal)
	}
	di.calculateMapOffsets()
	groupFubar := di.getSignalGroup("Fubar")
	for _, signal := range []string{"fu"} {
		di.addSignal(groupFubar.name, signal)
	}
	di.calculateMapOffsets()

	assert.Equal(t, 3, len(di.groups))
	assert.Equal(t, 3, len(di.index))

	assert.Equal(t, "foo", groupFoo.name)
	assert.Equal(t, uint(0), groupFoo.offset)
	assert.Equal(t, uint(3), groupFoo.length)
	assert.Equal(t, "one", groupFoo.signals[0].name)
	assert.Equal(t, uint(0), groupFoo.signals[0].index)
	assert.Equal(t, uint(0), groupFoo.signals[0].offset)
	assert.Equal(t, "two", groupFoo.signals[1].name)
	assert.Equal(t, uint(1), groupFoo.signals[1].index)
	assert.Equal(t, uint(8), groupFoo.signals[1].offset)
	assert.Equal(t, "three", groupFoo.signals[2].name)
	assert.Equal(t, uint(2), groupFoo.signals[2].index)
	assert.Equal(t, uint(16), groupFoo.signals[2].offset)

	assert.Equal(t, "bar", groupBar.name)
	assert.Equal(t, uint(3), groupBar.offset)
	assert.Equal(t, uint(3), groupBar.length)
	assert.Equal(t, "four", groupBar.signals[0].name)
	assert.Equal(t, uint(0), groupBar.signals[0].index)
	assert.Equal(t, uint(3*24+0), groupBar.signals[0].offset)
	assert.Equal(t, "five", groupBar.signals[1].name)
	assert.Equal(t, uint(1), groupBar.signals[1].index)
	assert.Equal(t, uint(3*24+8), groupBar.signals[1].offset)
	assert.Equal(t, "six", groupBar.signals[2].name)
	assert.Equal(t, uint(2), groupBar.signals[2].index)
	assert.Equal(t, uint(3*24+16), groupBar.signals[2].offset)

	assert.Equal(t, "Fubar", groupFubar.name)
	assert.Equal(t, uint(6), groupFubar.offset)
	assert.Equal(t, uint(1), groupFubar.length)
	assert.Equal(t, "fu", groupFubar.signals[0].name)
	assert.Equal(t, uint(0), groupFubar.signals[0].index)
	assert.Equal(t, uint(6*24+0), groupFubar.signals[0].offset)

	getOffset := func(group string, signal string) uint {
		offset, found := di.getSignalOffset(group, signal)
		assert.True(t, found)
		return offset
	}
	assert.Equal(t, uint(0), getOffset("foo", "one"))
	assert.Equal(t, uint(8), getOffset("foo", "two"))
	assert.Equal(t, uint(16), getOffset("foo", "three"))
	assert.Equal(t, uint(3*24+0), getOffset("bar", "four"))
	assert.Equal(t, uint(3*24+8), getOffset("bar", "five"))
	assert.Equal(t, uint(3*24+16), getOffset("bar", "six"))
}

func TestDirectIndex_write_sg(t *testing.T) {
	tmpdir := t.TempDir()
	directIndexPath := path.Join(tmpdir, "direct_index.yaml")

	di := NewDirectIndex()
	groupFoo := di.getSignalGroup("foo")
	for _, signal := range []string{"one", "two", "three"} {
		di.addSignal(groupFoo.name, signal)
	}
	groupBar := di.getSignalGroup("bar")
	for _, signal := range []string{"four", "five", "six"} {
		di.addSignal(groupBar.name, signal)
	}
	di.calculateMapOffsets()
	di.writeSignalGroup(directIndexPath)

	assert.FileExists(t, directIndexPath)
	index := index.NewYamlFileIndex()
	index.Scan(tmpdir)
	assert.Equal(t, 2, len(index.DocMap["SignalGroup"]))

	sgFoo := index.DocMap["SignalGroup"][0]
	assert.NotNil(t, sgFoo)
	assert.Equal(t, "foo", sgFoo.Metadata.Name)
	assert.Equal(t, 0, sgFoo.Metadata.Annotations["direct_index"].(map[string]any)["offset"])
	assert.Equal(t, 3, sgFoo.Metadata.Annotations["direct_index"].(map[string]any)["length"])
	specFoo := sgFoo.Spec.(*schema_kind.SignalGroupSpec)
	assert.Equal(t, "one", specFoo.Signals[0].Signal)
	assert.Equal(t, 0, (*specFoo.Signals[0].Annotations)["index"])
	assert.Equal(t, 0, (*specFoo.Signals[0].Annotations)["offset"])
	assert.Equal(t, "three", specFoo.Signals[2].Signal)
	assert.Equal(t, 2, (*specFoo.Signals[2].Annotations)["index"])
	assert.Equal(t, 16, (*specFoo.Signals[2].Annotations)["offset"])

	sgBar := index.DocMap["SignalGroup"][1]
	assert.NotNil(t, sgBar)
	assert.Equal(t, "bar", sgBar.Metadata.Name)
	assert.Equal(t, 3, sgBar.Metadata.Annotations["direct_index"].(map[string]any)["offset"])
	assert.Equal(t, 3, sgBar.Metadata.Annotations["direct_index"].(map[string]any)["length"])
	specBar := sgBar.Spec.(*schema_kind.SignalGroupSpec)
	assert.Equal(t, "four", specBar.Signals[0].Signal)
	assert.Equal(t, 0, (*specBar.Signals[0].Annotations)["index"])
	assert.Equal(t, (24*3 + 0), (*specBar.Signals[0].Annotations)["offset"])
	assert.Equal(t, "six", specBar.Signals[2].Signal)
	assert.Equal(t, 2, (*specBar.Signals[2].Annotations)["index"])
	assert.Equal(t, (24*3 + 16), (*specBar.Signals[2].Annotations)["offset"])
}
