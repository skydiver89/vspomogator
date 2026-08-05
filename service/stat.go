package main

import (
	"encoding/json"
	"fmt"
	"log"
	"strconv"
	"strings"
	"time"

	procstat "github.com/c9s/goprocinfo/linux"
	"github.com/distatus/battery"
	sensors "github.com/ssimunic/gosensors"
)

var prevStats []procstat.CPUStat
var prevTime time.Time
var prevRx, prevTx uint64

type Stat struct {
	Cores   int    `json:"cores"`
	Percs   []int  `json:"percs"`
	Mem     int    `json:"mem"`
	CPUTemp int    `json:"cputemp"`
	NetTx   string `json:"nettx"`
	NetRx   string `json:"netrx"`
	Bat     int    `json:"bat"`
	BatStat string `json:"batstat"`
}

func initStats() {
	stat, err := procstat.ReadStat("/proc/stat")
	if err != nil {
		log.Fatalln(err)
	}
	cores := len(stat.CPUStats)
	for i := 0; i < cores; i++ {
		prevStats = append(prevStats, procstat.CPUStat{})
	}
}

func calcSingleCoreUsage(curr, prev procstat.CPUStat) int {

	PrevIdle := prev.Idle + prev.IOWait
	Idle := curr.Idle + curr.IOWait

	PrevNonIdle := prev.User + prev.Nice + prev.System + prev.IRQ + prev.SoftIRQ + prev.Steal
	NonIdle := curr.User + curr.Nice + curr.System + curr.IRQ + curr.SoftIRQ + curr.Steal

	PrevTotal := PrevIdle + PrevNonIdle
	Total := Idle + NonIdle

	totald := Total - PrevTotal
	idled := Idle - PrevIdle

	CPU_Percentage := (float32(totald) - float32(idled)) / float32(totald)

	return int(CPU_Percentage * 100)
}

func calcNetWorkUsage(tx uint64, rx uint64) (restx, resrx string) {
	elapsed := time.Since(prevTime)
	if float32(elapsed/time.Millisecond) == 0 {
		return "0.00MB/s", "0.00MB/s"
	}
	txSpeed := int(float32(tx-prevTx) / (float32(elapsed/time.Millisecond) / 1000))
	rxSpeed := int(float32(rx-prevRx) / (float32(elapsed/time.Millisecond) / 1000))
	prevRx = rx
	prevTx = tx
	prevTime = time.Now()
	restx = fmt.Sprintf("%.2f", float32(txSpeed)/1024/1024) + "MB/s"
	resrx = fmt.Sprintf("%.2f", float32(rxSpeed)/1024/1024) + "MB/s"
	return restx, resrx
}

func getStats() []byte {
	var res Stat
	stat, err := procstat.ReadStat("/proc/stat")
	if err != nil {
		log.Fatalln(err)
	}
	res.Cores = len(stat.CPUStats)
	for i := 0; i < res.Cores; i++ {
		perc := calcSingleCoreUsage(stat.CPUStats[i], prevStats[i])
		prevStats[i] = stat.CPUStats[i]
		res.Percs = append(res.Percs, perc)
	}

	mem, err := procstat.ReadMemInfo("/proc/meminfo")
	if err != nil {
		log.Fatalln(err)
	}
	res.Mem = int(float64(mem.MemTotal-mem.MemAvailable) / float64(mem.MemTotal) * 100)
	sens, err := sensors.NewFromSystem()
	if err != nil {
		log.Fatalln(err)
	}

	for chip := range sens.Chips {
		for key, value := range sens.Chips[chip] {
			if key == "Package id 0" {
				res.CPUTemp, _ = strconv.Atoi(strings.Split(value, ".")[0][1:])
			}
		}
	}

	netstats, err := procstat.ReadNetworkStat("/proc/net/dev")
	if err != nil {
		log.Fatalln(err)
	}
	var rx, tx uint64
	for _, dev := range netstats {
		rx += dev.RxBytes
		tx += dev.TxBytes
	}
	res.NetTx, res.NetRx = calcNetWorkUsage(tx, rx)

	bat, err := battery.Get(0)
	if err != nil {
		res.Bat = 0
		res.BatStat = "Unknown"
		fmt.Println(err)
	} else {
		res.Bat = int(bat.Current * 100 / bat.Full)
		res.BatStat = bat.State.String()
	}

	//fmt.Printf("%#v", res)
	js, err := json.Marshal(res)
	if err != nil {
		log.Fatalln(err)
	}
	return js
}
