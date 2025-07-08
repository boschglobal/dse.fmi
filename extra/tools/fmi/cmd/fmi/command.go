// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"fmt"
	"os"
	"reflect"
)

var usage = `
FMI Toolset

  fmi <command> [command options,]

Examples:
  fmi gen-model \
	-input data/sample.xml \
	-output ./model.yaml
	-channels [{"name": "channel", "alias": "channel_alias", "selector":{"selector":"channel_Selector"}}] \
	-os linux \
	-arch amd64 \
	-dynlib path/fmimcl.so \
	-fmu path/fmu.so \
	-resource path/resources

  fmi gen-signalgroup \
  	-input data/sample.xml \
	-output ./signalgroup.yaml \
	-log 4

  fmi gen-annotations \
  	-input data/signalgroup.yaml \
	-output ./signalgroup_modified.yaml \
	-rule data/rule.csv \
	-log 4

  fmi gen-modelcfmu-annotations \
  	-sim sim \
	-signalgroups signal,network \
	-ruleset signal-direction \
	-log 4

  fmi gen-fmu \
  	-input sim \
	-model Target \
	-name Test \
	-dir ./ \
	-version 1.1.0 \
	-fmiVersion 2 \
	-log 4

  fmi patch-signalgroup \
  	-signalgroup signalgroup.yaml \
	-signal_patch patch.csv

Commands:
`

func PrintUsage() {
	fmt.Fprint(flag.CommandLine.Output(), usage)

	for _, cmd := range cmds {
		if cmd.Name() == "help" {
			continue
		}

		fmt.Fprintf(flag.CommandLine.Output(), "  %s\n", cmd.Name())
		fmt.Fprintf(flag.CommandLine.Output(), "    Options:\n")
		cmd.FlagSet().VisitAll(func(f *flag.Flag) {
			fmt.Fprintf(flag.CommandLine.Output(), "      -%s %s\n", f.Name, reflect.TypeOf(f.Value))
			fmt.Fprintf(flag.CommandLine.Output(), "          %s", f.Usage)
			if f.DefValue != "" {
				fmt.Fprintf(flag.CommandLine.Output(), "  (default: %s)", f.DefValue)
			}
			fmt.Fprintf(flag.CommandLine.Output(), "\n")
		})
	}
}

type HelpCommand struct {
	name string
	fs   *flag.FlagSet
}

func NewHelpCommand(name string) *HelpCommand {
	c := &HelpCommand{
		name: name,
		fs:   flag.NewFlagSet(name, flag.ExitOnError),
	}
	return c
}

func (c HelpCommand) Name() string {
	return c.name
}

func (c HelpCommand) FlagSet() *flag.FlagSet {
	return c.fs
}

func (c *HelpCommand) Parse(args []string) error {
	return nil
}

func (c *HelpCommand) Run() error {
	PrintUsage()
	return nil
}

type CommandRunner interface {
	Name() string
	FlagSet() *flag.FlagSet
	Parse([]string) error
	Run() error
}

func DispatchCommand(name string) error {
	var cmd CommandRunner
	for _, c := range cmds {
		if c.Name() == name {
			cmd = c
			break
		}
	}
	if cmd == nil {
		return fmt.Errorf("Unknown command: %s", name)
	}

	if cmd.Name() != "help" {
		fmt.Fprintf(flag.CommandLine.Output(), "Running FMI Toolset command: %s\n", cmd.Name())
	}
	if err := cmd.Parse(os.Args[2:]); err != nil {
		return err
	}
	fmt.Fprintf(flag.CommandLine.Output(), "Options:\n")
	cmd.FlagSet().VisitAll(func(f *flag.Flag) {
		fmt.Fprintf(flag.CommandLine.Output(), "  %-15s: %s\n", f.Name, f.Value)
	})
	return cmd.Run()
}
