package main

import (
	"bytes"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// fakeBoardHTTP mimics the CGI in sdk/http: PUT /firmware stores the image.
func fakeBoardHTTP(t *testing.T, fail bool) (*httptest.Server, *bytes.Buffer) {
	t.Helper()
	var got bytes.Buffer
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.Method + " " + r.URL.Path {
		case "PUT /cgi-bin/burn/firmware":
			if fail {
				http.Error(w, "burnd: remote core did not become running", http.StatusInternalServerError)
				return
			}
			io.Copy(&got, r.Body)
			io.WriteString(w, "loaded\n")
		case "POST /cgi-bin/burn/state":
			io.WriteString(w, "ok\n")
		default:
			http.NotFound(w, r)
		}
	}))
	t.Cleanup(srv.Close)
	return srv, &got
}

func TestHTTPUpload(t *testing.T) {
	srv, got := fakeBoardHTTP(t, false)
	fw := bytes.Repeat([]byte{1, 2, 3}, 5000)

	var last int
	u := newHTTPUploader(srv.URL + "/cgi-bin/burn/")
	u.Progress = func(sent, total int) { last = sent }
	if err := u.Upload(fw); err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(got.Bytes(), fw) {
		t.Fatalf("board received %d bytes, want %d", got.Len(), len(fw))
	}
	if last != len(fw) {
		t.Fatalf("progress stopped at %d", last)
	}
	if err := u.SetState("stop"); err != nil {
		t.Fatal(err)
	}
}

func TestHTTPUploadError(t *testing.T) {
	srv, _ := fakeBoardHTTP(t, true)
	u := newHTTPUploader(srv.URL + "/cgi-bin/burn")
	err := u.Upload([]byte{1})
	if err == nil || !strings.Contains(err.Error(), "did not become running") {
		t.Fatalf("err = %v, want board message", err)
	}
	if err := newHTTPUploader(srv.URL + "/nowhere").Upload([]byte{1}); err == nil {
		t.Fatal("404 should fail")
	}
}

func TestRunHTTP(t *testing.T) {
	srv, got := fakeBoardHTTP(t, false)
	fw := filepath.Join(t.TempDir(), "fw.elf")
	os.WriteFile(fw, []byte("hello"), 0o644)

	var out, errOut bytes.Buffer
	if code := run([]string{"-u", srv.URL + "/cgi-bin/burn", "-f", fw, "-v"}, &out, &errOut); code != 0 {
		t.Fatalf("exit %d: %s", code, errOut.String())
	}
	if got.String() != "hello" {
		t.Fatalf("board got %q", got.String())
	}
	if !strings.Contains(out.String(), "firmware running") {
		t.Fatalf("stdout %q", out.String())
	}

	errOut.Reset()
	if code := run([]string{"-u", srv.URL, "-p", "/dev/null", "-f", fw}, &out, &errOut); code != 2 {
		t.Fatalf("port and url together: exit %d", code)
	}
}
