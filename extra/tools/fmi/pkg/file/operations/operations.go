// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package operations

import (
	"archive/zip"
	"encoding/csv"
	"fmt"
	"io"
	"log/slog"
	"os"
	"path/filepath"
)

func Unzip(filename string, dest string) error {
	archive, err := zip.OpenReader(filename)
	if err != nil {
		return fmt.Errorf("UNZIP (%v)", err)
	}
	defer archive.Close()

	for _, file := range archive.File {
		filePath := filepath.Join(dest, file.Name)

		if file.FileInfo().IsDir() {
			if err := os.MkdirAll(filePath, os.ModePerm); err != nil {
				return fmt.Errorf("UNZIP (%v)", err)
			}
			continue
		}

		if err := os.MkdirAll(filepath.Dir(filePath), os.ModePerm); err != nil {
			return fmt.Errorf("UNZIP (%v)", err)
		}
		destFile, err := os.OpenFile(filePath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, file.Mode())
		if err != nil {
			return fmt.Errorf("UNZIP (%v)", err)
		}
		defer destFile.Close()

		srcFile, err := file.Open()
		if err != nil {
			return fmt.Errorf("UNZIP (%v)", err)
		}
		defer srcFile.Close()

		_, err = io.Copy(destFile, srcFile)

		if err != nil {
			return fmt.Errorf("UNZIP (%v)", err)
		}
	}

	return nil
}

func Zip(source, destination string) error {
	out, err := os.Create(destination)

	if err != nil {
		return fmt.Errorf("ZIP (%v)", err)
	}
	defer out.Close()

	w := zip.NewWriter(out)

	sourceInfo, err := os.Stat(source)
	if err != nil {
		return fmt.Errorf("ZIP (%v)", err)
	}
	defer w.Close()

	var baseDir string
	if sourceInfo.IsDir() {
		baseDir = filepath.Base(source)
	}

	return filepath.Walk(source, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		if filepath.Base(path) == baseDir {
			return nil
		}

		header, err := zip.FileInfoHeader(info)
		if err != nil {
			return fmt.Errorf("ZIP (%v)", err)
		}

		if baseDir != "" {
			name, err := filepath.Rel(source, path)
			if err != nil {
				return err
			}
			header.Name = filepath.ToSlash(name)
		}

		if info.IsDir() {
			header.Name += "/"
			header.Method = zip.Store
		}

		writer, err := w.CreateHeader(header)
		if err != nil {
			return fmt.Errorf("ZIP (%v)", err)
		}

		if info.IsDir() {
			return nil
		}

		if header.Mode().IsRegular() {
			file, err := os.Open(path)
			if err != nil {
				return fmt.Errorf("ZIP (%v)", err)
			}
			defer file.Close()

			_, err = io.CopyN(writer, file, info.Size())
			if err != nil && err != io.EOF {
				return fmt.Errorf("ZIP (%v)", err)
			}
		}
		return nil
	})
}

func CopyDirectory(scrDir string, dest string) error {

	if _, err := os.Stat(scrDir); err != nil {
		return fmt.Errorf("Directory does not exist (%s)", err)
	}

	entries, err := os.ReadDir(scrDir)
	if err != nil {
		return err
	}

	for _, entry := range entries {
		sourcePath := filepath.Join(scrDir, entry.Name())
		destPath := filepath.Join(dest, entry.Name())

		fileInfo, err := os.Stat(sourcePath)
		if err != nil {
			return err
		}

		if fileInfo.IsDir() {
			if _, err := os.Stat(destPath); os.IsNotExist(err) {
				if err := os.MkdirAll(destPath, 0755); err != nil {
					return fmt.Errorf("Error: %s", err)
				}
			}
			if err := CopyDirectory(sourcePath, destPath); err != nil {
				return err
			}
		} else {
			if err := Copy(sourcePath, destPath); err != nil {
				return err
			}
		}
	}
	return nil
}

func Copy(srcFile, destFile string) error {
	slog.Debug(fmt.Sprintf("Copy file: %s -> %s\n", srcFile, destFile))

	if _, err := os.Stat(destFile); err == nil {
		return nil
	}
	dest, err := os.Create(destFile)
	if err != nil {
		return err
	}
	defer dest.Close()

	src, err := os.Open(srcFile)
	if err != nil {
		return err
	}
	defer src.Close()

	_, err = io.Copy(dest, src)
	if err != nil {
		return err
	}

	return nil
}

func LoadCsv(srcFile string) ([][]string, error) {
	src, err := os.Open(srcFile)
	if err != nil {
		return nil, err
	}
	defer src.Close()

	csvReader := csv.NewReader(src)
	records, err := csvReader.ReadAll()
	if err != nil {
		return nil, err
	}

	return records, err
}
