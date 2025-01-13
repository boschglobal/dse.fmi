// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package patch

import (
	"encoding/csv"
	"flag"
	"fmt"
	"log/slog"
	"os"

	"gopkg.in/yaml.v3"

	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type PatchSignalGroupCommand struct {
	name string
	fs   *flag.FlagSet

	signalGroupFile string
	signalPatchFile string
	removeUnknown   bool
}

func NewPatchSignalGroupCommand(name string) *PatchSignalGroupCommand {
	c := &PatchSignalGroupCommand{name: name, fs: flag.NewFlagSet(name, flag.ExitOnError)}
	c.fs.StringVar(&c.signalGroupFile, "input", "", "signalgroup file")
	c.fs.StringVar(&c.signalPatchFile, "patch", "", "path to signal patch file, csv format")
	c.fs.BoolVar(&c.removeUnknown, "remove-unknown", false, "remove signals not included in signal patch file")
	return c
}

func (c PatchSignalGroupCommand) Name() string {
	return c.name
}

func (c PatchSignalGroupCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *PatchSignalGroupCommand) Parse(args []string) error {
	return c.fs.Parse(args)
}

func (c *PatchSignalGroupCommand) Run() error {
	inputYaml, err := os.ReadFile(c.signalGroupFile)
	if err != nil {
		return fmt.Errorf("Unable to open signalgroup file: %s (%w)", c.signalGroupFile, err)
	}
	sigGroup := kind.SignalGroup{}
	if err := yaml.Unmarshal(inputYaml, &sigGroup); err != nil {
		return fmt.Errorf("Unable to unmarshal signalgroup yaml: %s (%w)", c.signalGroupFile, err)
	}

	if c.signalPatchFile != "" {
		if err := patchSignals(&sigGroup, c.signalPatchFile, c.removeUnknown); err != nil {
			return fmt.Errorf("Error with signal patch: %v", err)
		}
	}

	// Write the patched SignalGroup.
	y, err := yaml.Marshal(&sigGroup)
	if err != nil {
		return fmt.Errorf("Error marshalling yaml: %v", err)
	}
	err = os.WriteFile(c.signalGroupFile, y, 0644)
	if err != nil {
		return fmt.Errorf("Error writing yaml: %v", err)
	}
	return nil
}

func patchSignals(sigGroup *kind.SignalGroup, patchFile string, removeUnknown bool) error {
	// Load the CSV patch file.
	f, err := os.Open(patchFile)
	if err != nil {
		return fmt.Errorf("Unable to open patch file: %s (%w)", patchFile, err)
	}
	r := csv.NewReader(f)
	r.Comma = ','
	r.TrimLeadingSpace = true
	record, err := r.Read()
	if err != nil {
		return fmt.Errorf("Unable to read csv (%w)", err)
	}
	if record[0] != "source" || record[1] != "target" {
		return fmt.Errorf("Unexpected csv columns (%v)", record)
	}

	// Load the patch map.
	records, err := r.ReadAll()
	if err != nil {
		return fmt.Errorf("Unable to read csv (%w)", err)
	}

	// Replace the source signal with target signal.
	patch := map[string]string{}
	for _, record := range records {
		if record[1] != "" {
			patch[record[0]] = record[1]
		} else {
			patch[record[0]] = record[0]
		}
	}

	// Apply the patch (remove if target=inactive or removeUnknown=true).
	signals := sigGroup.Spec.Signals
	i := 0
	for _, s := range signals {
		if target, ok := patch[s.Signal]; ok {
			if target != "inactive" {
				slog.Info(fmt.Sprintf("Patch Signals: patch: %s -> %s", s.Signal, target))
				s.Signal = target
				signals[i] = s
				i++
				continue
			}
		} else {
			if !removeUnknown {
				signals[i] = s
				i++
				continue
			}
		}
		slog.Info(fmt.Sprintf("Patch Signals: remove: %s", s.Signal))
	}

	// Trim the slice to only include retained signals.
	sigGroup.Spec.Signals = signals[:i]

	return nil
}
