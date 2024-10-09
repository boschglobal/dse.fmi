//go:build test_e2e
// +build test_e2e

package main

import (
	"fmt"
	"os"
	"strings"
	"testing"

	"github.com/goccy/go-yaml"
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
			"yamlcontains": func(ts *testscript.TestScript, neg bool, args []string) {
				// YAMLPath rule
				// $     : the root object/element
				// .     : child operator
				// ..    : recursive descent
				// [num] : object/element of array by number
				// [*]   : all objects/elements for array.
				//
				// example: "$.store.book[0].author"
				if len(args) != 3 {
					ts.Fatalf("yamlcontains <file> <ypath> <text>")
				}
				got := ts.ReadFile(args[0])
				ypath := args[1]
				want := args[2]
				path, err := yaml.PathString(ypath)
				if err != nil {
					ts.Fatalf("%v", err)
				}
				var value string
				err = path.Read(strings.NewReader(got), &value)
				if err != nil {
					if neg {
						return
					} else {
						ts.Fatalf("yamlcontains %q; %q path _not_ found in file", args[0], ypath)
					}
				}
				if (value == want) == neg {
					if neg {
						ts.Fatalf("yamlcontains %q; %q == %q found in file (unexpected)", args[0], ypath, want)
					} else {
						ts.Fatalf("yamlcontains %q; %q == %q not found in file", args[0], ypath, want)
					}
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
