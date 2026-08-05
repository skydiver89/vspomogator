package main

import (
	"log"
	"os/exec"
	"strconv"
	"strings"

	"github.com/go-vgo/robotgo"
)

func execButton(but string) {
	buf := strings.TrimPrefix(but, "button ")
	lst := strings.Split(buf, "-")
	laynum, _ := strconv.Atoi(lst[0])
	laynum--
	butnum, _ := strconv.Atoi(lst[1])
	butnum--
	if laynum >= len(config.Layouts) {
		return
	}
	if butnum >= len(config.Layouts[laynum].Buttons) {
		return
	}
	button := config.Layouts[laynum].Buttons[butnum]
	btype := button.Type
	if btype == "" {
		btype = "command"
	}
	if btype == "keytype" {
		robotgo.Type(button.Command.Com)
		if button.Command.HitEnter {
			robotgo.KeyTap("enter")
		}
	}
	if btype == "keyseq" {
		robotgo.KeyTap(button.Command.Com, button.Command.Args)
	}
	if btype == "command" {
		cmd := exec.Command(button.Command.Com, button.Command.Args...)
		cmd.Stdin = nil
		cmd.Stdout = nil
		cmd.Stderr = nil
		err := cmd.Start()
		if err != nil {
			log.Println(err)
			return
		}
		cmd.Process.Release()
	}
}
