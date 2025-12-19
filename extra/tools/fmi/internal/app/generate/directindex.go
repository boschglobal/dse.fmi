// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package generate

import (
	"fmt"
	"log/slog"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type DirectIndexSignal struct {
	name   string
	index  uint // SignalGroup relative index.
	offset uint // Calculated from base of directIndex symbol (i.e. 0).
}

type DirectIndexSignalGroup struct {
	name    string
	signals []*DirectIndexSignal
	index   map[string]*DirectIndexSignal
	offset  uint // Offset of signals in this SignalGroup relative to slice all groups (i.e. 0).
	length  uint // Length of signals in this SignalGroup.
}

type DirectIndex struct {
	groups []*DirectIndexSignalGroup
	index  map[string]*DirectIndexSignalGroup
}

func NewDirectIndex() *DirectIndex {
	di := &DirectIndex{}
	di.index = make(map[string]*DirectIndexSignalGroup)
	return di
}

func (di *DirectIndex) getSignalGroup(name string) *DirectIndexSignalGroup {
	if group, ok := di.index[name]; ok {
		return group
	}
	group := DirectIndexSignalGroup{name: name}
	group.index = make(map[string]*DirectIndexSignal)
	di.groups = append(di.groups, &group)
	di.index[name] = &group
	return &group
}

func (di *DirectIndex) addSignal(group string, name string) {
	signalGroup := di.getSignalGroup(group)
	if _, ok := signalGroup.index[name]; ok {
		// Signal already exists, ignore.
		return
	}
	signal := DirectIndexSignal{name: name}
	signalGroup.signals = append(signalGroup.signals, &signal)
	signalGroup.index[name] = &signal
}

func (di *DirectIndex) calculateMapOffsets() {
	groupOffset := uint(0) // metadata:annotations:direct_index:offset
	for _, group := range di.groups {
		// Each group has a memory block allocated in the map with dimension:
		// 		[8] [8] [4] [4] .. n items per array/vector.
		//		(scalar/binary/size/len)
		group.offset = groupOffset
		mapOffset := groupOffset * 24 // [8] [8] [4] [4] per group with n items.
		group.length = uint(len(group.signals))

		for i, s := range group.signals {
			s.index = uint(i)
			s.offset = mapOffset + (uint(i) * 8)
			// TODO: for binary signal
		}
		groupOffset += uint(len(group.signals)) // Set starting point for next group.
	}
}

func (di *DirectIndex) writeSignalGroup(path string) error {
	for _, group := range di.groups {
		signals := []schema_kind.Signal{}
		for _, s := range group.signals {
			annotations := schema_kind.Annotations{
				"index":  s.index,
				"offset": s.offset,
			}
			signal := schema_kind.Signal{
				Signal:      s.name,
				Annotations: &annotations,
			}
			signals = append(signals, signal)
		}
		annotations := schema_kind.Annotations{
			"direct_index": &map[string]any{
				"offset": group.offset,
				"length": group.length,
			},
		}
		signalGroup := schema_kind.SignalGroup{
			Kind: "SignalGroup",
			Metadata: &schema_kind.ObjectMetadata{
				Name: stringPtr(group.name),
				Labels: &schema_kind.Labels{
					"index": "direct",
				},
				Annotations: &annotations,
			},
		}
		signalGroup.Spec.Signals = signals

		slog.Info(fmt.Sprintf("Append direct index: %v (file=%v)", group.name, path))
		if err := writeYaml(&signalGroup, path, true); err != nil {
			return err
		}
	}

	return nil
}

func (di *DirectIndex) getSignalOffset(group string, signal string) (offset uint, found bool) {
	if sg, ok := di.index[group]; ok {
		if s, ok := sg.index[signal]; ok {
			return s.offset, true
		}
	}
	return 0, false
}
