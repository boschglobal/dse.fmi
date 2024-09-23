//go:build test_e2e
// +build test_e2e

package main

import (
	"fmt"
	"os"
	"strings"
	"testing"

	"github.com/rogpeppe/go-internal/testscript"
)

func TestMain(m *testing.M) {
	os.Exit(testscript.RunMain(m, map[string]func() int{
		"fmi": main_,
	}))
}

func TestE2E(t *testing.T) {
	testscript.Run(t, testscript.Params{
		Dir: "testdata/script",
		Cmds: map[string]func(ts *testscript.TestScript, neg bool, args []string){
			"filecontains": func(ts *testscript.TestScript, neg bool, args []string) {
				if len(args) != 2 {
					ts.Fatalf("filecontains <file> <text>")
				}
				got := ts.ReadFile(args[0])
				want := args[1]
				if strings.Contains(got, want) == neg {
					ts.Fatalf("filecontains %q; %q not found in file:\n%q", args[0], want, got)
				}
			},
		},
		Setup: func(e *testscript.Env) error {
			var vars = []string{
				fmt.Sprintf("BUILD_DIR=%s", os.Getenv("BUILD_DIR")),
				fmt.Sprintf("REPO_DIR=%s", os.Getenv("REPO_DIR")),
			}
			e.Vars = append(e.Vars, vars...)
			return nil
		},
	})
}
