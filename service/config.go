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

type Command struct {
	Com      string   `yaml:"com"`
	Args     []string `yaml:"args"`
	KeySeq   []string `yaml:"keyseq"`
	HitEnter bool     `yaml:"hitenter"`
}

type Button struct {
	Command Command `yaml:"command"`
	Label   string  `yaml:"label"`
	Type    string  `yaml:"type"`
}

func (c *Config) load(fname string) error {
	yamlData, err := os.ReadFile(fname)
	if err != nil {
		return err
	}
	err = yaml.Unmarshal(yamlData, c)
	return err
}

type ButtonLight struct {
	Label string `json:"Label"`
}
type LayoutLight struct {
	Buttons []ButtonLight `json:"Buttons"`
}
type ConfigLight struct {
	Layouts []LayoutLight `json:"Layouts"`
}

func (c *Config) toLight() ConfigLight {
	light := ConfigLight{
		Layouts: make([]LayoutLight, len(c.Layouts)),
	}

	for i, layout := range c.Layouts {
		light.Layouts[i] = LayoutLight{
			Buttons: make([]ButtonLight, len(layout.Buttons)),
		}
		for j, btn := range layout.Buttons {
			light.Layouts[i].Buttons[j] = ButtonLight{
				Label: btn.Label,
			}
		}
	}
	return light
}
