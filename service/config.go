package main

import (
	"os"

	"gopkg.in/yaml.v3"
)

type Config struct {
	Port    string   `yaml:"port"`
	Layouts []Layout `yaml:"layouts"`
}

type Layout struct {
	Buttons []Button `yaml:"buttons"`
}

type Button struct {
	Command  string `yaml:"command"`
	Label    string `yaml:"label"`
	Type     string `yaml:"type"`
	HitEnter bool   `yaml:"hitenter"`
}

func (c *Config) load(fname string) error {
	yamlData, err := os.ReadFile(fname)
	if err != nil {
		return err
	}
	err = yaml.Unmarshal(yamlData, c)
	return err
}
