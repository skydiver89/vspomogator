package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"github.com/tarm/serial"
)

var VERSION = "0.1.0"
var GITREV = ""
var BUILDTIME = ""

const (
	INITREQ byte = 255
	STATREQ byte = 254
)

var configFile = "vspomogator.yml"

var config Config

func parseFlags() {
	flag.Usage = func() {
		fmt.Printf("Использование: %s [-c config] [-h] [-v]\n", os.Args[0])
		flag.PrintDefaults()
		os.Exit(0)
	}
	flag.StringVar(&configFile, "c", "vspomogator.yml", "Path to config")
	var showHelp bool
	flag.BoolVar(&showHelp, "h", false, "Show this help")
	var showVersion bool
	flag.BoolVar(&showVersion, "v", false, "Show version")
	flag.Parse()
	if showHelp {
		flag.Usage()
	}
	if showVersion {
		fmt.Println("Версия     : ", VERSION)
		fmt.Println("Коммит git : ", GITREV)
		fmt.Println("Дата сборки: ", BUILDTIME)
		os.Exit(0)
	}
}

func extractLastCmd(text string) string {
	startMarker := "begincmd"
	endMarker := "endcmd"
	var blocks []string
	pos := 0
	for {
		startIdx := strings.Index(text[pos:], startMarker)
		if startIdx == -1 {
			break
		}
		startIdx += pos
		endIdx := strings.Index(text[startIdx+len(startMarker):], endMarker)
		if endIdx == -1 {
			break
		}
		endIdx += startIdx + len(startMarker)
		block := text[startIdx+len(startMarker) : endIdx]
		blocks = append(blocks, block)
		pos = endIdx + len(endMarker)
	}
	if len(blocks) == 0 {
		return ""
	}
	return blocks[len(blocks)-1]
}

func main() {
	parseFlags()
	err := config.load(configFile)
	if err != nil {
		log.Fatalln(err)
	}
	serialConfig := &serial.Config{
		Name:        config.Port,
		Baud:        115200,
		ReadTimeout: time.Second,
		Size:        8,
		StopBits:    1,
		Parity:      serial.ParityNone,
	}

OPEN_PORT:
	fmt.Println("open port")
	port, err := serial.OpenPort(serialConfig)
	if err != nil {
		log.Println("Can't open port", config.Port, ":", err)
		time.Sleep(time.Second)
		goto OPEN_PORT
	}
	defer port.Close()
	//wait port for open
	time.Sleep(time.Second)
	for {
		b := make([]byte, 1024)
		n, err := port.Read(b)
		if err != nil {
			fmt.Println(err)
			port.Close()
			goto OPEN_PORT
		}
		cmd := extractLastCmd(string(b[:n]))
		if cmd == "" {
			continue
		}
		fmt.Println(cmd, n)
		if cmd == "init" {
			jsonData, err := json.Marshal(config.Layouts)
			n, err := port.Write(append(jsonData, '\n'))
			if n == 0 || err != nil {
				port.Close()
				goto OPEN_PORT
			}
		}
	}
}
