#!/usr/bin/env bash
# Run the full webserv test suite.
# Works both on the HOST (builds + runs Docker) and INSIDE the container (runs directly).
#
# Usage:
#   make test              # from host  — builds image, runs inside Docker
#   make test              # inside container — runs Python runner directly
#   bash tests/run_tests.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# ── Detect environment ────────────────────────────────────────────────────────

if [ -f /.dockerenv ]; then
    # ── Inside the container ──────────────────────────────────────────────
    cd /webserv
    mkdir -p /webserv/tests/reports
    exec python3 /webserv/tests/run_tests.py
else
    # ── On the host ───────────────────────────────────────────────────────
    cd "$PROJECT_ROOT"

    echo "==> Building Docker image..."
    docker build -t webserv . --quiet

    echo "==> Running test suite..."
    mkdir -p tests/reports

    docker run --rm \
        -v "$PROJECT_ROOT/tests/reports:/webserv/tests/reports" \
        webserv \
        python3 /webserv/tests/run_tests.py

    echo ""
    echo "==> Reports written to tests/reports/"
    echo "==> Summary: tests/reports/summary.md"
fi
