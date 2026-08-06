package main

import (
	"errors"
	"flag"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/tarm/serial"
)

var VERSION = "0.1.0"
var GITREV = ""
var BUILDTIME = ""

const (
	cmdStart = "begincmd"
	cmdEnd   = "endcmd"

	// firmware setup(): Serial.begin + short delay + TFT init
	espBootGrace = 3 * time.Second
	// give up and fully reconnect if INITREQ never arrives after open
	handshakeTimeout = 5 * time.Second
	deviceStableFor  = 300 * time.Millisecond
)

var (
	errHandshakeTimeout = errors.New("handshake timeout: no INITREQ")
	configFile          = "vspomogator.yml"
	config              Config
)

func parseFlags() {
	flag.Usage = func() {
		fmt.Printf("Usage: %s [-c config] [-h] [-v]\n", os.Args[0])
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

func main() {
	parseFlags()
	if err := config.load(configFile); err != nil {
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

	initStats()

	for {
		port, id, err := openPort(serialConfig)
		if err != nil {
			log.Fatalln(err)
		}
		log.Println("port open:", config.Port)
		err = serve(port, config.Port, id)
		port.Close()
		log.Printf("port closed: %v — reconnecting", err)
		time.Sleep(500 * time.Millisecond)
	}
}
