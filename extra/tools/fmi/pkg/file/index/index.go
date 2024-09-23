// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package index

import (
	"fmt"
	"io/fs"
	"log/slog"
	"os"
	"path/filepath"

	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/handler"
	"github.com/boschdevcloud.com/dse.fmi/extra/tools/fmi/pkg/file/handler/kind"
)

type YamlKindLabel struct {
	name  string
	value string
}

type YamlFileIndex struct {
	DocMap map[string][]kind.KindDoc
}

func NewYamlFileIndex() *YamlFileIndex {
	index := &YamlFileIndex{}
	index.DocMap = make(map[string][]kind.KindDoc)
	return index
}

func (index *YamlFileIndex) Scan(path string) {
	// Collect the list of yaml files within path.
	file_exts := []string{".yml", ".yaml"}
	files := []string{}
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
		for _, doc := range docs.([]kind.KindDoc) {
			slog.Info(fmt.Sprintf("kind: %s; name=%s (%s)", doc.Kind, doc.Metadata.Name, doc.File))
			if _, ok := index.DocMap[doc.Kind]; !ok {
				index.DocMap[doc.Kind] = []kind.KindDoc{}
			}
			index.DocMap[doc.Kind] = append(index.DocMap[doc.Kind], doc)
		}
	}
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
		slog.Debug(fmt.Sprintf("IndexFiles: %s (%t, %s)", s, d.IsDir(), filepath.Ext(s)))
		if !d.IsDir() && filepath.Ext(s) == extension {
			files = append(files, filepath.Join(path, s))
		}
		return nil
	})

	return files, err
}
