// Command burntool uploads a firmware image (ELF) to a board running the
// burnd service and restarts the remote core with it.
//
// Two transports are available: the USB serial port spoken by the burnd
// daemon (protocol.go, client.go) and HTTP against the small API in
// http.go, which a board can provide with sdk/http on top of "burnd load".
package main

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"time"
)

// version is stamped at build time with -ldflags "-X main.version=...".
var version = "dev"

type options struct {
	port      string
	url       string
	firmware  string
	baud      int
	chip      string // accepted for compatibility with the old tool, unused
	verbose   bool
	version   bool
	timeout   time.Duration
	tailDelay time.Duration
}

func main() {
	os.Exit(run(os.Args[1:], os.Stdout, os.Stderr))
}

// run is main without the process exit, so it can be tested.
func run(args []string, stdout, stderr io.Writer) int {
	o, err := parseFlags(args, stderr)
	if errors.Is(err, flag.ErrHelp) {
		return 0
	}
	if err != nil {
		fmt.Fprintln(stderr, "error:", err)
		return 2
	}
	if o.version {
		fmt.Fprintln(stdout, "burntool", version)
		return 0
	}

	fmt.Fprintf(stdout, "burntool %s\n", version)
	if err := upload(o, stdout, stderr); err != nil {
		fmt.Fprintln(stderr, "error:", err)
		return 1
	}
	return 0
}

func parseFlags(args []string, stderr io.Writer) (*options, error) {
	o := &options{}
	fs := flag.NewFlagSet("burntool", flag.ContinueOnError)
	fs.SetOutput(stderr)

	for _, name := range []string{"p", "port"} {
		fs.StringVar(&o.port, name, os.Getenv("BURNTOOL_PORT"), "")
	}
	for _, name := range []string{"u", "url"} {
		fs.StringVar(&o.url, name, os.Getenv("BURNTOOL_URL"), "")
	}
	for _, name := range []string{"f", "firmware"} {
		fs.StringVar(&o.firmware, name, os.Getenv("BURNTOOL_FIRMWARE"), "")
	}
	for _, name := range []string{"b", "baud"} {
		fs.IntVar(&o.baud, name, envInt("BURNTOOL_BAUD", 115200), "")
	}
	for _, name := range []string{"c", "chip"} {
		fs.StringVar(&o.chip, name, "", "")
	}
	for _, name := range []string{"v", "verbose"} {
		fs.BoolVar(&o.verbose, name, false, "")
	}
	fs.BoolVar(&o.version, "version", false, "")
	fs.DurationVar(&o.timeout, "timeout", defaultReplyTimeout, "")
	fs.DurationVar(&o.tailDelay, "tail-delay", defaultTailDelay, "")

	fs.Usage = func() {
		fmt.Fprintf(stderr, `Usage: burntool -p PORT -f FIRMWARE [options]
       burntool -u URL  -f FIRMWARE [options]

Upload a firmware image to a board running burnd and restart its remote core,
over the USB serial port or over HTTP.

Options:
  -p, --port PORT        serial port, e.g. /dev/ttyACM0 or COM3 (env BURNTOOL_PORT)
  -u, --url URL          HTTP endpoint instead of a serial port, e.g.
                         http://192.168.42.1/cgi-bin/burn (env BURNTOOL_URL)
  -f, --firmware FILE    firmware image to upload (env BURNTOOL_FIRMWARE)
  -b, --baud RATE        baud rate, ignored by USB CDC ports (default 115200)
  -c, --chip NAME        accepted for compatibility, unused
  -v, --verbose          print every frame exchanged with the board
      --timeout D        reply timeout per command (default %s)
      --tail-delay D     pause before the end-of-transfer frame (default %s)
      --version          print the version and exit
  -h, --help             show this help
`, defaultReplyTimeout, defaultTailDelay)
	}

	if err := fs.Parse(args); err != nil {
		return nil, err
	}
	if fs.NArg() > 0 {
		return nil, fmt.Errorf("unexpected argument %q", fs.Arg(0))
	}
	if o.version {
		return o, nil
	}
	if o.port == "" && o.url == "" {
		return nil, errors.New("serial port (-p) or URL (-u) not set")
	}
	if o.port != "" && o.url != "" {
		return nil, errors.New("use either a serial port (-p) or a URL (-u), not both")
	}
	if o.firmware == "" {
		return nil, errors.New("firmware file not set (-f)")
	}
	return o, nil
}

func envInt(name string, def int) int {
	if v, err := strconv.Atoi(os.Getenv(name)); err == nil {
		return v
	}
	return def
}

// upload reads the image, opens the port and runs the client.
func upload(o *options, stdout, stderr io.Writer) error {
	firmware, err := os.ReadFile(o.firmware)
	if err != nil {
		return fmt.Errorf("read firmware: %w", err)
	}
	fmt.Fprintf(stdout, "firmware: %s (%d bytes)\n", o.firmware, len(firmware))

	var logf func(format string, args ...any)
	if o.verbose {
		logf = func(format string, args ...any) {
			fmt.Fprintf(stderr, format+"\n", args...)
		}
	}

	if o.url != "" {
		u := newHTTPUploader(o.url)
		u.Progress = progressPrinter(stdout)
		u.Logf = logf
		fmt.Fprintf(stdout, "url: %s\n", u.Base)
		if err := u.Upload(firmware); err != nil {
			return err
		}
		fmt.Fprintln(stdout, "upload done, firmware running")
		return nil
	}

	port, err := openSerial(o.port, o.baud)
	if err != nil {
		return err
	}
	defer port.Close()
	fmt.Fprintf(stdout, "port: %s\n", o.port)

	c := NewClient(port)
	c.ReplyTimeout = o.timeout
	c.TailDelay = o.tailDelay
	c.Progress = progressPrinter(stdout)
	c.Logf = logf

	fmt.Fprintln(stdout, "connecting...")
	if err := c.Upload(firmware); err != nil {
		return err
	}
	fmt.Fprintln(stdout, "upload done, firmware running")
	return nil
}

// progressPrinter reports transfer progress in 10% steps, one line each,
// which renders cleanly in the Arduino IDE console.
func progressPrinter(w io.Writer) func(sent, total int) {
	last := -1
	return func(sent, total int) {
		pct := sent * 100 / total
		step := pct / 10
		if step != last {
			last = step
			fmt.Fprintf(w, "sending... %3d%%\n", pct)
		}
	}
}
