package main

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

// httpUploader sends the firmware to a board over the network instead of
// the serial port. The board exposes a tiny HTTP API (see sdk/http in this
// repository for a busybox httpd CGI that implements it on top of
// "burnd load"):
//
//	POST <base>/firmware   body is the image; the board installs it and
//	                       restarts the remote core
//	POST <base>/state      body "start" or "stop"
//	GET  <base>/state      current remoteproc state
//
// Any 2xx answer means success; the response body is relayed as log text.
type httpUploader struct {
	Base   string
	Client *http.Client

	Progress func(sent, total int)
	Logf     func(format string, args ...any)
}

// defaultHTTPTimeout bounds one request including the board-side restart.
const defaultHTTPTimeout = 60 * time.Second

func newHTTPUploader(base string) *httpUploader {
	return &httpUploader{
		Base:   strings.TrimRight(base, "/"),
		Client: &http.Client{Timeout: defaultHTTPTimeout},
	}
}

func (u *httpUploader) logf(format string, args ...any) {
	if u.Logf != nil {
		u.Logf(format, args...)
	}
}

// Upload installs the firmware and restarts the core with one POST.
func (u *httpUploader) Upload(firmware []byte) error {
	if len(firmware) == 0 {
		return fmt.Errorf("firmware is empty")
	}
	body := &progressReader{r: bytes.NewReader(firmware), total: len(firmware), cb: u.Progress}
	req, err := http.NewRequest(http.MethodPost, u.Base+"/firmware", body)
	if err != nil {
		return err
	}
	req.ContentLength = int64(len(firmware))
	req.Header.Set("Content-Type", "application/octet-stream")
	return u.do(req)
}

// SetState sends "start" or "stop" to the board.
func (u *httpUploader) SetState(state string) error {
	req, err := http.NewRequest(http.MethodPost, u.Base+"/state", strings.NewReader(state))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "text/plain")
	return u.do(req)
}

func (u *httpUploader) do(req *http.Request) error {
	u.logf("-> %s %s", req.Method, req.URL)
	resp, err := u.Client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	msg, _ := io.ReadAll(io.LimitReader(resp.Body, 64<<10))
	text := strings.TrimSpace(string(msg))
	u.logf("<- %s %s", resp.Status, text)
	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		if text == "" {
			text = resp.Status
		}
		return fmt.Errorf("board answered %s: %s", resp.Status, text)
	}
	return nil
}

// progressReader reports how much of the body has been consumed.
type progressReader struct {
	r     io.Reader
	total int
	sent  int
	cb    func(sent, total int)
}

func (p *progressReader) Read(b []byte) (int, error) {
	n, err := p.r.Read(b)
	p.sent += n
	if p.cb != nil && n > 0 {
		p.cb(p.sent, p.total)
	}
	return n, err
}
