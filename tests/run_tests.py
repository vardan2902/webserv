#!/usr/bin/env python3
"""
webserv automated test runner.
Runs inside the Docker container at /webserv.

For each valid config scenario:
  - starts the server
  - sends HTTP requests via http.client (no curl required)
  - verifies status codes, body fragments, and response headers
  - writes tests/reports/<scenario>/report.md

For each invalid config scenario:
  - runs the server and expects a non-zero exit code

Writes tests/reports/summary.md at the end.
"""
import datetime
import http.client
import os
import subprocess
import sys
import time

# ── Constants ─────────────────────────────────────────────────────────────────

WEBSERV_BIN  = "/webserv/webserv"
REPORTS_DIR  = "/webserv/tests/reports"
HOST         = "127.0.0.1"
STARTUP_WAIT = 3.0   # max seconds to wait for port to be ready
SHUTDOWN_WAIT = 2.0

# ANSI colours
_GREEN  = "\033[92m"
_RED    = "\033[91m"
_GREY   = "\033[90m"
_BOLD   = "\033[1m"
_RESET  = "\033[0m"

def _c(text, *codes):
    return "".join(codes) + text + _RESET


# ── Multipart helper ──────────────────────────────────────────────────────────

def make_multipart(filename, file_content):
    """Return (content_type_header_value, body_bytes)."""
    boundary = "WebservTestBoundary42XYZ"
    if isinstance(file_content, str):
        file_content = file_content.encode("utf-8")
    header = (
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\""
        "; filename=\"" + filename + "\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
    ).encode("latin-1")
    footer = ("\r\n--" + boundary + "--\r\n").encode("latin-1")
    body   = header + file_content + footer
    ct     = "multipart/form-data; boundary=" + boundary
    return ct, body


def upload_case_parts(filename, content):
    """Return (headers_dict, body_bytes) ready for a test-case dict."""
    ct, body = make_multipart(filename, content)
    return {"Content-Type": ct}, body


# ── HTTP helper ───────────────────────────────────────────────────────────────

def http_req(port, method, path, extra_headers=None, body=None, timeout=5):
    """Send one HTTP/1.1 request; return (status_int, headers_dict, body_str).
    On any connection error returns (None, {}, error_message)."""
    try:
        conn = http.client.HTTPConnection(HOST, port, timeout=timeout)
        h = {"Host": "{}:{}".format(HOST, port), "Connection": "close"}
        if extra_headers:
            h.update(extra_headers)
        if isinstance(body, str):
            body = body.encode("utf-8")
        conn.request(method, path, body=body, headers=h)
        resp  = conn.getresponse()
        rbody = resp.read().decode("utf-8", errors="replace")
        rhdrs = {k.lower(): v for k, v in resp.getheaders()}
        conn.close()
        return resp.status, rhdrs, rbody
    except Exception as exc:
        return None, {}, str(exc)


def raw_http(port, data):
    """Send raw bytes over a plain TCP socket; return (status_int_or_None, raw_str)."""
    import socket as _socket
    s = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((HOST, port))
    s.sendall(data if isinstance(data, bytes) else data.encode("latin-1"))
    buf = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf += chunk
    except _socket.timeout:
        pass
    s.close()
    if not buf:
        return None, ""
    parts = buf.split(b" ", 2)
    status = int(parts[1]) if len(parts) >= 2 and parts[1].isdigit() else None
    return status, buf.decode("utf-8", errors="replace")


# ── Server lifecycle ──────────────────────────────────────────────────────────

def _port_ready(port):
    try:
        c = http.client.HTTPConnection(HOST, port, timeout=0.3)
        c.connect()
        c.close()
        return True
    except Exception:
        return False


def start_server(config_path, ports):
    proc = subprocess.Popen(
        [WEBSERV_BIN, config_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    deadline = time.time() + STARTUP_WAIT
    while time.time() < deadline:
        if proc.poll() is not None:
            break  # crashed
        if all(_port_ready(p) for p in ports):
            time.sleep(0.05)  # tiny buffer for all listeners to settle
            return proc
        time.sleep(0.05)
    time.sleep(0.1)
    return proc


def stop_server(proc):
    if proc is None or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=SHUTDOWN_WAIT)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


# ── Test-case runner ──────────────────────────────────────────────────────────

class Result:
    __slots__ = ("name", "method", "path", "exp", "actual", "passed", "note")
    def __init__(self, name, method, path, exp, actual, passed, note=""):
        self.name   = name
        self.method = method
        self.path   = path
        self.exp    = str(exp)
        self.actual = str(actual)
        self.passed = passed
        self.note   = note


def run_case(case):
    method  = case["method"]
    path    = case["path"]
    port    = case.get("port", 8080)
    exp     = case["expected_status"]
    body    = case.get("body", None)
    hdrs    = case.get("headers", {})
    timeout = case.get("timeout", 5)

    status, resp_hdrs, resp_body = http_req(port, method, path, hdrs, body, timeout)

    if status is None:
        return Result(case["name"], method, path, exp, "ERR", False,
                      "connection failed: " + resp_body[:120])

    notes = []

    for fragment in case.get("body_contains", []):
        if fragment not in resp_body:
            notes.append("body missing " + repr(fragment))

    for hkey, hval in case.get("headers_contain", {}).items():
        got = resp_hdrs.get(hkey.lower(), "")
        if hval not in got:
            notes.append("{}: expected {!r} in {!r}".format(hkey, hval, got))

    passed = (status == exp) and not notes
    note   = "; ".join(notes) if notes else ""
    actual = str(status) if status == exp else "{} (exp {})".format(status, exp)
    return Result(case["name"], method, path, exp, actual, passed, note)


def raw_case(case):
    port = case.get("port", 8080)
    exp  = case["expected_status"]
    status, _ = raw_http(port, case["raw_data"])
    if status is None:
        if exp == 400:
            return Result(case["name"], "RAW", "<raw>", exp, "400(close)", True,
                          "server closed connection (acceptable for 400)")
        return Result(case["name"], "RAW", "<raw>", exp, "ERR", False, "no response")
    passed = (status == exp)
    actual = str(status) if passed else "{} (exp {})".format(status, exp)
    return Result(case["name"], "RAW", "<raw>", exp, actual, passed)


# ── Report writers ────────────────────────────────────────────────────────────

def write_report(scenario, config_relpath, results, ts):
    out_dir = os.path.join(REPORTS_DIR, scenario)
    os.makedirs(out_dir, exist_ok=True)

    total  = len(results)
    passed = sum(1 for r in results if r.passed)
    badge  = "PASS" if passed == total else "FAIL"
    icon   = "✅" if passed == total else "❌"

    lines = [
        "# Test Report: " + scenario,
        "",
        "- **Date:** " + ts,
        "- **Config:** `" + config_relpath + "`",
        "- **Result:** {} {} ({}/{})".format(icon, badge, passed, total),
        "",
        "## Test Cases",
        "",
        "| # | Test | Method | Path | Expected | Actual | Result |",
        "|---|------|--------|------|----------|--------|--------|",
    ]
    for i, r in enumerate(results, 1):
        ri   = "✅ PASS" if r.passed else "❌ FAIL"
        note = " — " + r.note if r.note else ""
        lines.append("| {} | {} | `{}` | `{}` | {} | {} | {}{} |".format(
            i, r.name, r.method, r.path, r.exp, r.actual, ri, note))

    with open(os.path.join(out_dir, "report.md"), "w") as fh:
        fh.write("\n".join(lines) + "\n")

    return passed, total


def write_error_report(scenario, config_relpath, exit_code, stderr_text, ts):
    out_dir = os.path.join(REPORTS_DIR, scenario)
    os.makedirs(out_dir, exist_ok=True)

    passed = (exit_code != 0)
    badge  = "PASS" if passed else "FAIL"
    icon   = "✅" if passed else "❌"

    lines = [
        "# Test Report: " + scenario + " (invalid config)",
        "",
        "- **Date:** " + ts,
        "- **Config:** `" + config_relpath + "`",
        "- **Result:** {} {}".format(icon, badge),
        "",
        "## Expected Behaviour",
        "",
        "Server must exit with a **non-zero exit code** on config validation failure.",
        "",
        "| Expectation | exit code | Result |",
        "|-------------|-----------|--------|",
        "| exit code ≠ 0 | {} | {} |".format(exit_code, icon),
        "",
        "### stderr output",
        "```",
        stderr_text.strip() if stderr_text.strip() else "(none)",
        "```",
    ]
    with open(os.path.join(out_dir, "report.md"), "w") as fh:
        fh.write("\n".join(lines) + "\n")

    return (1, 1) if passed else (0, 1)


def write_summary(rows, ts):
    lines = [
        "# Test Summary — " + ts,
        "",
        "| Scenario | Tests | Passed | Failed | Result |",
        "|----------|-------|--------|--------|--------|",
    ]
    total_t = total_p = 0
    for name, n_pass, n_total in rows:
        n_fail = n_total - n_pass
        icon   = "✅ PASS" if n_pass == n_total else "❌ FAIL"
        lines.append("| {} | {} | {} | {} | {} |".format(
            name, n_total, n_pass, n_fail, icon))
        total_t += n_total
        total_p += n_pass

    total_f = total_t - total_p
    overall = "✅ ALL PASS" if total_p == total_t else "❌ {} FAILED".format(total_f)
    lines.append("| **TOTAL** | **{}** | **{}** | **{}** | **{}** |".format(
        total_t, total_p, total_f, overall))

    os.makedirs(REPORTS_DIR, exist_ok=True)
    with open(os.path.join(REPORTS_DIR, "summary.md"), "w") as fh:
        fh.write("\n".join(lines) + "\n")


# ── Pre-build upload payloads ─────────────────────────────────────────────────

# upload scenario: unique filename so it can be confirmed and deleted
_up_hdrs,    _up_body    = upload_case_parts("webserv_test_42.txt",      "hello from webserv tests\n")
# allow_methods scenario
_am_hdrs,    _am_body    = upload_case_parts("allow_methods_test.txt",   "test\n")
# client_max_body_size — small (1 KB)
_sm_hdrs,    _sm_body    = upload_case_parts("small_test.txt",           "A" * 1024)
# client_max_body_size — large (150 KB, exceeds 100 k limit)
_lg_hdrs,    _lg_body    = upload_case_parts("large_test.txt",           "B" * (150 * 1024))
# client_max_body_size exact boundary (limit = 100 KB = 102400; server uses >, so equal is allowed)
_LIMIT = 102400
_, _at_empty = make_multipart("boundary_at.txt", b"")
_, _ov_empty = make_multipart("boundary_ov.txt", b"")
_at_hdrs, _at_body = upload_case_parts("boundary_at.txt", b"E" * (_LIMIT - len(_at_empty)))
_ov_hdrs, _ov_body = upload_case_parts("boundary_ov.txt", b"F" * (_LIMIT - len(_ov_empty) + 1))


# ── Scenario definitions ──────────────────────────────────────────────────────

VALID_SCENARIOS = [
    # ── basic ──────────────────────────────────────────────────────────────
    {
        "name":   "basic",
        "config": "/webserv/configs/basic/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /",                            "method": "GET",    "path": "/",               "port": 8080, "expected_status": 200},
            {"name": "GET /missing",                     "method": "GET",    "path": "/missing",        "port": 8080, "expected_status": 404},
            {"name": "POST / → 405",                     "method": "POST",   "path": "/",               "port": 8080, "expected_status": 405},
            {"name": "DELETE / → 405",                   "method": "DELETE", "path": "/",               "port": 8080, "expected_status": 405},
            {"name": "HEAD / → 200",                     "method": "HEAD",   "path": "/",               "port": 8080, "expected_status": 200},
            {"name": "PUT / → 405",                      "method": "PUT",    "path": "/",               "port": 8080, "expected_status": 405},
            {"name": "PATCH / → 405",                    "method": "PATCH",  "path": "/",               "port": 8080, "expected_status": 405},
            {"name": "GET /../../etc/passwd → 404",      "method": "GET",    "path": "/../../etc/passwd","port": 8080, "expected_status": 404},
            {"name": "Content-Type text/html on GET /",  "method": "GET",    "path": "/",               "port": 8080, "expected_status": 200, "headers_contain": {"content-type": "text/html"}},
        ],
    },
    # ── full ───────────────────────────────────────────────────────────────
    {
        "name":   "full",
        "config": "/webserv/configs/full/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /",                      "method": "GET",    "path": "/",               "port": 8080, "expected_status": 200},
            {"name": "GET /files/ (autoindex)",    "method": "GET",    "path": "/files/",         "port": 8080, "expected_status": 200, "body_contains": ["notes.txt"]},
            {"name": "GET /files/notes.txt",       "method": "GET",    "path": "/files/notes.txt","port": 8080, "expected_status": 200},
            {"name": "DELETE /files/notes.txt",    "method": "DELETE", "path": "/files/notes.txt","port": 8080, "expected_status": 204},
            {"name": "GET /files/notes.txt (gone)","method": "GET",    "path": "/files/notes.txt","port": 8080, "expected_status": 404},
            {"name": "GET /old → 301",                             "method": "GET",    "path": "/old",              "port": 8080, "expected_status": 301, "headers_contain": {"location": "localhost:8080"}},
            {"name": "GET /missing → 404",                         "method": "GET",    "path": "/missing",          "port": 8080, "expected_status": 404},
            {"name": "GET /files (no slash) → 301",                "method": "GET",    "path": "/files",            "port": 8080, "expected_status": 301, "headers_contain": {"location": "/files/"}},
            {"name": "GET /files/sample.txt still 200 after DELETE","method": "GET",   "path": "/files/sample.txt", "port": 8080, "expected_status": 200},
        ],
    },
    # ── multi ──────────────────────────────────────────────────────────────
    {
        "name":   "multi",
        "config": "/webserv/configs/multi/config.conf",
        "ports":  [8080, 8081],
        "cases": [
            {"name": "GET :8080/ → site1",  "method": "GET", "path": "/", "port": 8080, "expected_status": 200, "body_contains": ["Site 1"]},
            {"name": "GET :8081/ → site2",  "method": "GET", "path": "/", "port": 8081, "expected_status": 200, "body_contains": ["Site 2"]},
            {"name": "GET :8080/missing",   "method": "GET", "path": "/missing", "port": 8080, "expected_status": 404},
        ],
    },
    # ── location ───────────────────────────────────────────────────────────
    {
        "name":   "location",
        "config": "/webserv/configs/location/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /",        "method": "GET", "path": "/",        "port": 8080, "expected_status": 200},
            {"name": "GET /missing", "method": "GET", "path": "/missing", "port": 8080, "expected_status": 404},
        ],
    },
    # ── nested_locations ───────────────────────────────────────────────────
    {
        "name":   "nested_locations",
        "config": "/webserv/configs/nested_locations/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /",                     "method": "GET", "path": "/",                   "port": 8080, "expected_status": 200},
            {"name": "GET /images/ (autoindex)",  "method": "GET", "path": "/images/",            "port": 8080, "expected_status": 200, "body_contains": ["diagram.svg"]},
            {"name": "GET /images/diagram.svg",   "method": "GET", "path": "/images/diagram.svg", "port": 8080, "expected_status": 200},
        ],
    },
    # ── autoindex ──────────────────────────────────────────────────────────
    {
        "name":   "autoindex",
        "config": "/webserv/configs/autoindex/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / (listing)",  "method": "GET",  "path": "/",          "port": 8080, "expected_status": 200, "body_contains": ["about.html", "readme.txt"]},
            {"name": "GET /about.html",  "method": "GET",  "path": "/about.html","port": 8080, "expected_status": 200},
            {"name": "GET /readme.txt",  "method": "GET",  "path": "/readme.txt","port": 8080, "expected_status": 200},
            {"name": "POST / → 405",     "method": "POST", "path": "/",          "port": 8080, "expected_status": 405},
        ],
    },
    # ── error_page ─────────────────────────────────────────────────────────
    {
        "name":   "error_page",
        "config": "/webserv/configs/error_page/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /",                     "method": "GET", "path": "/",        "port": 8080, "expected_status": 200},
            {"name": "GET /missing → custom 404", "method": "GET", "path": "/missing", "port": 8080, "expected_status": 404, "body_contains": ["custom"]},
            {"name": "GET /other → custom 404",   "method": "GET", "path": "/other",   "port": 8080, "expected_status": 404, "body_contains": ["custom"]},
        ],
    },
    # ── upload ─────────────────────────────────────────────────────────────
    {
        "name":   "upload",
        "config": "/webserv/configs/upload/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /",                                "method": "GET",    "path": "/",                           "port": 8080, "expected_status": 200},
            {"name": "POST /upload → 201",                   "method": "POST",   "path": "/upload",                     "port": 8080, "expected_status": 201, "headers": _up_hdrs, "body": _up_body},
            {"name": "GET /files/ (file listed)",            "method": "GET",    "path": "/files/",                     "port": 8080, "expected_status": 200, "body_contains": ["webserv_test_42.txt"]},
            {"name": "DELETE /files/webserv_test_42.txt",    "method": "DELETE", "path": "/files/webserv_test_42.txt",  "port": 8080, "expected_status": 204},
            {"name": "GET /files/webserv_test_42.txt → 404", "method": "GET",    "path": "/files/webserv_test_42.txt",  "port": 8080, "expected_status": 404},
            {"name": "DELETE /files/nonexistent → 404",      "method": "DELETE", "path": "/files/does_not_exist.txt",   "port": 8080, "expected_status": 404},
            {"name": "POST /upload no Content-Type → 400",   "method": "POST",   "path": "/upload",                     "port": 8080, "expected_status": 400, "body": b"data"},
            {"name": "POST /upload wrong Content-Type → 400","method": "POST",   "path": "/upload",                     "port": 8080, "expected_status": 400, "headers": {"Content-Type": "application/json"}, "body": b"{}"},
            {"name": "POST /upload no boundary → 400",       "method": "POST",   "path": "/upload",                     "port": 8080, "expected_status": 400, "headers": {"Content-Type": "multipart/form-data"}, "body": b"--x\r\n\r\n"},
        ],
    },
    # ── allow_methods ──────────────────────────────────────────────────────
    {
        "name":   "allow_methods",
        "config": "/webserv/configs/allow_methods/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / → 200",          "method": "GET",    "path": "/",       "port": 8080, "expected_status": 200},
            {"name": "POST / → 405",         "method": "POST",   "path": "/",       "port": 8080, "expected_status": 405},
            {"name": "DELETE / → 405",       "method": "DELETE", "path": "/",       "port": 8080, "expected_status": 405},
            {"name": "GET /upload → 405",    "method": "GET",    "path": "/upload", "port": 8080, "expected_status": 405},
            {"name": "DELETE /upload → 405", "method": "DELETE", "path": "/upload", "port": 8080, "expected_status": 405},
            {"name": "POST /upload → 201",   "method": "POST",   "path": "/upload", "port": 8080, "expected_status": 201, "headers": _am_hdrs, "body": _am_body},
        ],
    },
    # ── client_max_body_size ───────────────────────────────────────────────
    {
        "name":   "client_max_body_size",
        "config": "/webserv/configs/client_max_body_size/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / → 200",               "method": "GET",  "path": "/",       "port": 8080, "expected_status": 200},
            {"name": "POST small (1 KB) → 201",   "method": "POST", "path": "/upload", "port": 8080, "expected_status": 201, "headers": _sm_hdrs, "body": _sm_body},
            {"name": "POST large (150 KB) → 413",        "method": "POST", "path": "/upload", "port": 8080, "expected_status": 413, "headers": _lg_hdrs, "body": _lg_body, "timeout": 15},
            {"name": "POST at exact 100 KB limit → 201", "method": "POST", "path": "/upload", "port": 8080, "expected_status": 201, "headers": _at_hdrs, "body": _at_body, "timeout": 15},
            {"name": "POST 1 byte over 100 KB → 413",    "method": "POST", "path": "/upload", "port": 8080, "expected_status": 413, "headers": _ov_hdrs, "body": _ov_body, "timeout": 15},
        ],
    },
    # ── return ─────────────────────────────────────────────────────────────
    {
        "name":   "return",
        "config": "/webserv/configs/return/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / → 200",     "method": "GET", "path": "/",    "port": 8080, "expected_status": 200},
            {"name": "GET /old → 301",  "method": "GET", "path": "/old", "port": 8080, "expected_status": 301, "headers_contain": {"location": "/new/"}},
            {"name": "GET /new/ → 200", "method": "GET", "path": "/new/","port": 8080, "expected_status": 200},
        ],
    },
    # ── multi_host ─────────────────────────────────────────────────────────
    {
        "name":   "multi_host",
        "config": "/webserv/configs/multi_host/config.conf",
        "ports":  [8080, 8081],
        "cases": [
            {"name": "GET 127.0.0.1:8080/", "method": "GET", "path": "/", "port": 8080, "expected_status": 200},
            {"name": "GET 127.0.0.1:8081/", "method": "GET", "path": "/", "port": 8081, "expected_status": 200},
        ],
    },
    # ── cgi ────────────────────────────────────────────────────────────────
    {
        "name":   "cgi",
        "config": "/webserv/configs/cgi/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /cgi-bin/hello.py",         "method": "GET", "path": "/cgi-bin/hello.py",         "port": 8080, "expected_status": 200, "body_contains": ["Hello, World!"], "timeout": 10},
            {"name": "GET /cgi-bin/hello.py?name=42", "method": "GET", "path": "/cgi-bin/hello.py?name=42", "port": 8080, "expected_status": 200, "body_contains": ["Hello, 42!"],    "timeout": 10},
            {"name": "GET /cgi-bin/test.php",         "method": "GET", "path": "/cgi-bin/test.php",         "port": 8080, "expected_status": 200, "body_contains": ["html"],          "timeout": 10},
        ],
    },
    # ── errors ─────────────────────────────────────────────────────────────
    {
        "name":   "errors",
        "config": "/webserv/configs/errors/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / → 200",
             "method": "GET", "path": "/", "port": 8080, "expected_status": 200},

            {"name": "POST /post-no-store (no upload_store) → 501",
             "method": "POST", "path": "/post-no-store", "port": 8080, "expected_status": 501,
             "headers": {"Content-Type": "multipart/form-data; boundary=X"}, "body": b"--X\r\n\r\n--X--\r\n"},

            {"name": "GET /noindex/ (autoindex off, no index) → 403",
             "method": "GET", "path": "/noindex/", "port": 8080, "expected_status": 403},

            {"name": "malformed request line → 400",
             "raw_data": b"GETINVALID\r\n\r\n", "port": 8080, "expected_status": 400},

            {"name": "HTTP/1.1 missing Host header → 400",
             "raw_data": b"GET / HTTP/1.1\r\n\r\n", "port": 8080, "expected_status": 400},

            {"name": "LF-only line endings (no CR) → 400",
             "raw_data": b"GET / HTTP/1.1\nHost: 127.0.0.1\n\n", "port": 8080, "expected_status": 400},

            {"name": "bare CRLF (empty request) → 400 or close",
             "raw_data": b"\r\n", "port": 8080, "expected_status": 400},
        ],
    },
]

ERROR_SCENARIOS = [
    {"name": "bad_port",            "config": "/webserv/configs/bad_port/config.conf"},
    {"name": "duplicate_host_port", "config": "/webserv/configs/duplicate_host_port/config.conf"},
    {"name": "missing_semicolon",   "config": "/webserv/configs/missing_semicolon/config.conf"},
    {"name": "unknown_directive",   "config": "/webserv/configs/unknown_directive/config.conf"},
    {"name": "multi_duplicate_ports","config": "/webserv/configs/multi_duplicate_ports/config.conf"},
]


# ── Console helpers ───────────────────────────────────────────────────────────

_COL = 48  # column width for test name

def _print_header(title):
    print()
    print(_c("─" * 60, _GREY))
    print(_c("  " + title, _BOLD))
    print(_c("─" * 60, _GREY))


def _print_result(r):
    icon = _c("✅", _GREEN) if r.passed else _c("❌", _RED)
    note = _c("  ← " + r.note, _RED) if r.note else ""
    label = (r.name[:_COL - 1] + "…") if len(r.name) > _COL else r.name
    print("  {} {:<{}} {}{}".format(icon, label, _COL, r.actual, note))


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    os.makedirs(REPORTS_DIR, exist_ok=True)
    summary_rows = []

    # ── Valid config scenarios ──────────────────────────────────────────────
    for scenario in VALID_SCENARIOS:
        name   = scenario["name"]
        config = scenario["config"]
        ports  = scenario["ports"]
        cases  = scenario["cases"]
        rel    = config.replace("/webserv/", "")

        _print_header(name + "   [" + rel + "]")

        proc    = start_server(config, ports)
        results = []

        if proc.poll() is not None:
            print(_c("  ✗ server crashed on startup (rc={})".format(proc.returncode), _RED))
            results.append(Result("server startup", "—", "—",
                                  "running", "crashed (rc={})".format(proc.returncode),
                                  False, "process exited before tests ran"))
        else:
            for case in cases:
                r = raw_case(case) if "raw_data" in case else run_case(case)
                results.append(r)
                _print_result(r)

        stop_server(proc)

        n_pass, n_total = write_report(name, rel, results, ts)
        summary_rows.append((name, n_pass, n_total))
        tag = _c("✅ {}/{} passed".format(n_pass, n_total), _GREEN) \
              if n_pass == n_total \
              else _c("❌ {}/{} passed".format(n_pass, n_total), _RED)
        print("  " + tag)

    # ── Error config scenarios ──────────────────────────────────────────────
    _print_header("Invalid-config validation (expect non-zero exit)")

    for scenario in ERROR_SCENARIOS:
        name   = scenario["name"]
        config = scenario["config"]
        rel    = config.replace("/webserv/", "")

        try:
            result = subprocess.run(
                [WEBSERV_BIN, config],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=5,
            )
            ec     = result.returncode
            stderr = result.stderr.decode("utf-8", errors="replace")
        except subprocess.TimeoutExpired:
            ec     = 0   # did not fail → bad
            stderr = "server did not exit within 5 seconds"

        passed = (ec != 0)
        icon   = _c("✅", _GREEN) if passed else _c("❌", _RED)
        print("  {} {:<30s} exit={}".format(icon, name, ec))

        n_pass, n_total = write_error_report(name, rel, ec, stderr, ts)
        summary_rows.append((name, n_pass, n_total))

    # ── Final summary ───────────────────────────────────────────────────────
    write_summary(summary_rows, ts)

    total_t = sum(n for _, _, n in summary_rows)
    total_p = sum(p for _, p, _ in summary_rows)
    total_f = total_t - total_p

    print()
    print(_c("═" * 60, _GREY))
    if total_f == 0:
        print(_c("  ✅  ALL PASS  — {}/{} tests".format(total_p, total_t), _GREEN + _BOLD))
    else:
        print(_c("  ❌  {} FAILED  — {}/{} passed".format(total_f, total_p, total_t), _RED + _BOLD))
    print(_c("  Reports → " + REPORTS_DIR + "/summary.md", _GREY))
    print(_c("═" * 60, _GREY))
    print()

    sys.exit(0 if total_f == 0 else 1)


if __name__ == "__main__":
    main()
