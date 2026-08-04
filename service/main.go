package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"syscall"
	"time"

	"github.com/tarm/serial"
	"golang.org/x/sys/unix"
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

func extractCmds(buf string) (cmds []string, rest string) {
	for {
		start := strings.Index(buf, cmdStart)
		if start < 0 {
			return cmds, trailingPrefix(buf, cmdStart)
		}
		buf = buf[start:]
		bodyStart := len(cmdStart)
		end := strings.Index(buf[bodyStart:], cmdEnd)
		if end < 0 {
			return cmds, buf
		}
		cmds = append(cmds, buf[bodyStart:bodyStart+end])
		buf = buf[bodyStart+end+len(cmdEnd):]
	}
}

func trailingPrefix(s, marker string) string {
	max := len(marker) - 1
	if max <= 0 {
		return ""
	}
	if len(s) > max {
		s = s[len(s)-max:]
	}
	for n := len(s); n > 0; n-- {
		if strings.HasPrefix(marker, s[len(s)-n:]) {
			return s[len(s)-n:]
		}
	}
	return ""
}

func writeAll(port *serial.Port, data []byte) error {
	for len(data) > 0 {
		n, err := port.Write(data)
		if n > 0 {
			data = data[n:]
		}
		if err != nil {
			return err
		}
		if n == 0 {
			return io.ErrShortWrite
		}
	}
	return nil
}

func deviceID(path string) (string, error) {
	info, err := os.Stat(path)
	if err != nil {
		return "", err
	}
	stat, ok := info.Sys().(*syscall.Stat_t)
	if !ok {
		return fmt.Sprintf("mt:%d", info.ModTime().UnixNano()), nil
	}
	return fmt.Sprintf("%d:%d", stat.Dev, stat.Ino), nil
}

func setModemLines(fd int, dtr, rts bool) error {
	status, err := unix.IoctlGetInt(fd, unix.TIOCMGET)
	if err != nil {
		return err
	}
	if dtr {
		status |= unix.TIOCM_DTR
	} else {
		status &^= unix.TIOCM_DTR
	}
	if rts {
		status |= unix.TIOCM_RTS
	} else {
		status &^= unix.TIOCM_RTS
	}
	return unix.IoctlSetPointerInt(fd, unix.TIOCMSET, status)
}

// pulseESPReset does esptool hard_reset: EN low via RTS, IO0 inactive via DTR.
func pulseESPReset(path string) error {
	f, err := os.OpenFile(path, unix.O_RDWR|unix.O_NOCTTY, 0)
	if err != nil {
		return err
	}
	defer f.Close()

	fd := int(f.Fd())
	if err := setModemLines(fd, false, true); err != nil {
		return err
	}
	time.Sleep(100 * time.Millisecond)
	if err := setModemLines(fd, false, false); err != nil {
		return err
	}
	time.Sleep(50 * time.Millisecond)
	return nil
}

// idleRead is a ReadTimeout with no data (Linux often surfaces this as EOF).
func idleRead(n int, err error) bool {
	return n == 0 && (err == nil || errors.Is(err, io.EOF))
}

func sendConfig(port *serial.Port) error {
	jsonData, err := json.Marshal(config.Layouts)
	if err != nil {
		return err
	}
	log.Printf("init → %d bytes config", len(jsonData))
	return writeAll(port, append(jsonData, '\n'))
}

func checkDevice(path, id string) error {
	cur, err := deviceID(path)
	if err != nil {
		return fmt.Errorf("serial device disappeared: %w", err)
	}
	if cur != id {
		return errors.New("serial device re-enumerated")
	}
	return nil
}

func serve(port *serial.Port, path, id string) error {
	buf := make([]byte, 1024)
	pending := ""
	handshook := false
	deadline := time.Now().Add(handshakeTimeout)

	for {
		n, err := port.Read(buf)
		if n > 0 {
			pending += string(buf[:n])
			cmds, rest := extractCmds(pending)
			pending = rest

			wantConfig := false
			for _, cmd := range cmds {
				switch {
				case cmd == "init":
					wantConfig = true
					handshook = true
				case cmd == "initok":
					log.Println("device ready")
					handshook = true
				case strings.HasPrefix(cmd, "button "):
					log.Printf("button %s", strings.TrimPrefix(cmd, "button "))
				default:
					log.Printf("cmd: %q", cmd)
				}
			}
			if wantConfig {
				if err := sendConfig(port); err != nil {
					return err
				}
			}
		}
		if idleRead(n, err) {
			if !handshook && time.Now().After(deadline) {
				return errHandshakeTimeout
			}
			if err := checkDevice(path, id); err != nil {
				return err
			}
			continue
		}
		if err != nil {
			return err
		}
	}
}

// waitDeviceStable returns when path exists and its inode is unchanged for hold.
func waitDeviceStable(path string, hold time.Duration) (string, error) {
	for {
		id, err := deviceID(path)
		if err != nil {
			return "", err
		}
		time.Sleep(hold)
		id2, err := deviceID(path)
		if err != nil {
			return "", err
		}
		if id == id2 {
			return id, nil
		}
		log.Println("device still enumerating...")
	}
}

func openPort(cfg *serial.Config) (*serial.Port, string, error) {
	for {
		if _, err := waitDeviceStable(cfg.Name, deviceStableFor); err != nil {
			log.Printf("waiting for %s: %v", cfg.Name, err)
			time.Sleep(time.Second)
			continue
		}

		port, err := serial.OpenPort(cfg)
		if err != nil {
			log.Printf("can't open %s: %v", cfg.Name, err)
			time.Sleep(time.Second)
			continue
		}

		// Explicit reset, then drop the FD — USB often re-enumerates and the
		// old handle becomes a black hole that still looks "idle".
		if err := pulseESPReset(cfg.Name); err != nil {
			log.Printf("esp reset failed: %v", err)
		} else {
			log.Println("esp reset")
		}
		_ = port.Flush()
		port.Close()

		if _, err := waitDeviceStable(cfg.Name, deviceStableFor); err != nil {
			log.Printf("device lost after reset: %v", err)
			time.Sleep(time.Second)
			continue
		}

		port, err = serial.OpenPort(cfg)
		if err != nil {
			log.Printf("can't reopen %s: %v", cfg.Name, err)
			time.Sleep(time.Second)
			continue
		}

		// Final open toggles DTR on most adapters — wait for that boot too.
		_ = port.Flush()
		time.Sleep(espBootGrace)
		_ = port.Flush()

		id, err := deviceID(cfg.Name)
		if err != nil {
			port.Close()
			log.Printf("port lost during esp boot: %v", err)
			time.Sleep(time.Second)
			continue
		}
		return port, id, nil
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
