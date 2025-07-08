// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package index

import (
	"bytes"
	"fmt"
	"io/fs"
	"log/slog"
	"os"
	"path/filepath"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/handler"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/handler/kind"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"gopkg.in/yaml.v3"
)

type YamlKindLabel struct {
	name  string
	value string
}

type docMapIndex struct {
	kind  string
	index int
}
type YamlFileIndex struct {
	DocMap   map[string][]kind.KindDoc
	FileMap  map[string][]docMapIndex // Points to items in DocMap.
	SaveList map[string]struct{}
}

func NewYamlFileIndex() *YamlFileIndex {
	index := &YamlFileIndex{}
	index.DocMap = make(map[string][]kind.KindDoc)
	index.FileMap = make(map[string][]docMapIndex)
	index.SaveList = make(map[string]struct{})
	return index
}

func (index *YamlFileIndex) Scan(path string) {
	// Collect the list of yaml files within path.
	files := []string{}
	file_exts := []string{".yml", ".yaml"}
	for _, ext := range file_exts {
		indexed_files, _ := indexFiles(path, ext)
		files = append(files, indexed_files...)
	}

	// Load each file into the index.
	for _, f := range files {
		// Each file may contain several yaml documents.
		_, docs, err := handler.ParseFile(f)
		if err != nil {
			slog.Debug(fmt.Sprintf("Parse failed (%s) on file: %s", err.Error(), f))
			continue
		}
		docMapIndexList := []docMapIndex{}
		for _, doc := range docs.([]kind.KindDoc) {
			slog.Info(fmt.Sprintf("kind: %s; name=%s (%s)", doc.Kind, doc.Metadata.Name, doc.File))
			if _, ok := index.DocMap[doc.Kind]; !ok {
				index.DocMap[doc.Kind] = []kind.KindDoc{}
			}
			index.DocMap[doc.Kind] = append(index.DocMap[doc.Kind], doc)
			docMapIndexList = append(docMapIndexList, docMapIndex{kind: doc.Kind, index: len(index.DocMap[doc.Kind]) - 1})
		}
		index.FileMap[f] = docMapIndexList
	}
}

func (index *YamlFileIndex) Updated(path string) {
	if _, ok := index.FileMap[path]; ok {
		index.SaveList[path] = struct{}{}
	}
}

func (index *YamlFileIndex) save(file string) error {
	append := false // Truncate file for first doc.
	if fileMap, ok := index.FileMap[file]; ok {
		for _, docIndex := range fileMap {
			doc := index.DocMap[docIndex.kind][docIndex.index]
			switch doc.Kind {
			case "Stack":
				d := schema_kind.Stack{
					Kind: schema_kind.StackKind(doc.Kind),
					Metadata: &schema_kind.ObjectMetadata{
						Name:        &doc.Metadata.Name,
						Labels:      (*schema_kind.Labels)(&doc.Metadata.Labels),
						Annotations: (*schema_kind.Annotations)(&doc.Metadata.Annotations),
					},
					Spec: *doc.Spec.(*schema_kind.StackSpec),
				}
				if err := writeYaml(d, file, append); err != nil {
					return err
				}
			case "SignalGroup":
				d := schema_kind.SignalGroup{
					Kind: schema_kind.SignalGroupKind(doc.Kind),
					Metadata: &schema_kind.ObjectMetadata{
						Name:        &doc.Metadata.Name,
						Labels:      (*schema_kind.Labels)(&doc.Metadata.Labels),
						Annotations: (*schema_kind.Annotations)(&doc.Metadata.Annotations),
					},
					Spec: *doc.Spec.(*schema_kind.SignalGroupSpec),
				}
				if err := writeYaml(d, file, append); err != nil {
					return err
				}
			case "Model":
				d := schema_kind.Model{
					Kind: schema_kind.ModelKind(doc.Kind),
					Metadata: &schema_kind.ObjectMetadata{
						Name:        &doc.Metadata.Name,
						Labels:      (*schema_kind.Labels)(&doc.Metadata.Labels),
						Annotations: (*schema_kind.Annotations)(&doc.Metadata.Annotations),
					},
					Spec: *doc.Spec.(*schema_kind.ModelSpec),
				}
				if err := writeYaml(d, file, append); err != nil {
					return err
				}
			default:
				return fmt.Errorf("Unsupported doc kind; %s", doc.Kind)
			}

			append = true // Append remaining docs.
		}
	}
	return nil
}

func (index *YamlFileIndex) Save() error {
	for file, _ := range index.SaveList {
		if err := index.save(file); err != nil {
			return err
		}
	}
	return nil
}

func (index *YamlFileIndex) SaveAll() error {
	for file, _ := range index.FileMap {
		if err := index.save(file); err != nil {
			return err
		}
	}
	return nil
}

func writeYaml(v interface{}, path string, append bool) error {
	var b bytes.Buffer
	enc := yaml.NewEncoder(&b)
	enc.SetIndent(2)
	if err := enc.Encode(v); err != nil {
		return fmt.Errorf("Error encoding yaml: %v", err)
	}
	enc.Close()
	if append {
		f, err := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			return fmt.Errorf("Error appending yaml: %v", err)
		}
		defer f.Close()
		if _, err := f.Write([]byte("---\n")); err != nil {
			return fmt.Errorf("Error appending yaml: %v", err)
		}
		if _, err := f.Write(b.Bytes()); err != nil {
			return fmt.Errorf("Error appending yaml: %v", err)
		}
	} else {
		if err := os.WriteFile(path, b.Bytes(), 0644); err != nil {
			return fmt.Errorf("Error writing yaml: %v", err)
		}
	}
	return nil
}

func (index *YamlFileIndex) FindByName(kind string, name string) (*kind.KindDoc, bool) {
	if docs, ok := index.DocMap[kind]; ok {
		for _, doc := range docs {
			if doc.Metadata.Name == name {
				return &doc, true
			}
		}
	}
	return nil, false
}

func (index *YamlFileIndex) FindByLabel(kind string, labels []YamlKindLabel) (*kind.KindDoc, bool) {
	if docs, ok := index.DocMap[kind]; ok {
		for _, doc := range docs {
			match := false
			for _, label := range labels {
				if doc.Metadata.Labels[label.name] == label.value {
					match = true
				} else {
					match = false
					break
				}
			}
			if match {
				return &doc, true
			}
		}
	}
	return nil, false
}

func indexFiles(path string, extension string) ([]string, error) {
	files := []string{}
	fileSystem := os.DirFS(path)
	err := fs.WalkDir(fileSystem, ".", func(s string, d fs.DirEntry, e error) error {
		if e != nil {
			return nil
		}
		slog.Debug(fmt.Sprintf("IndexFiles: %s (%t, %s)", s, d.IsDir(), filepath.Ext(s)))
		if !d.IsDir() && filepath.Ext(s) == extension {
			files = append(files, filepath.Join(path, s))
		}
		return nil
	})

	return files, err
}
