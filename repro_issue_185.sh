#!/usr/bin/env bash
#
# repro_issue_185.sh — Reproduce the TCP-reset bug behind issue #185 / PR #389.
#
# Symptom: `hey`/clients see "connection reset by peer" (and pooled keepalive
# clients log `Unsolicited response received on idle HTTP channel starting with
# "HTTP/1.1 ..."`) when SLEdge answers a request *before* it has finished reading
# the request body.
#
# Why a plain bodyless GET does NOT reproduce it:
#   429/500/503 are emitted from on_client_request_received(), which only runs
#   after the FULL request (body included) has been read. So those are clean.
#
# The reproducible path is 404. In on_client_request_receiving()
# (runtime/src/listener_thread.c, ~line 200) the route is matched the moment the
# URL is parsed from the *request line* — before the body arrives:
#
#     if (session->route == NULL && strlen(session->http_request.full_url) > 0) {
#         route = http_router_match_route(...);
#         if (route == NULL) { ...404...; on_client_response_header_sending(); return; }
#     }
#
# So: POST a sizeable body to a NON-existent route. SLEdge writes the 404 and
# close()es the socket while the client is still sending. On Linux, close() with
# unread data in the kernel receive buffer discards it and emits a RST instead of
# a graceful FIN -> "connection reset by peer".
#
# Expected result:
#   * On master / fix/docker-dev-setup (unpatched): a nonzero number of
#     "connection reset by peer" errors -> BUG REPRODUCED.
#   * On fix/issue-185-graceful-close (patched tcp_session_close): 0 resets.
#
# Usage:
#   ./repro_issue_185.sh
#
# Tunables (env vars):
#   PORT=10000            tenant listen port
#   REQUESTS=2400         total requests (hey -n)
#   CONCURRENCY=32        concurrent connections (hey -c)
#   BODY_BYTES=100000     request body size (~100 KB); larger = more resets
#
set -euo pipefail

PORT="${PORT:-10000}"
REQUESTS="${REQUESTS:-2400}"
CONCURRENCY="${CONCURRENCY:-32}"
BODY_BYTES="${BODY_BYTES:-100000}"

# --- locate the repo (this script lives at the repo root) ---------------------
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
BIN_DIR="$REPO_ROOT/runtime/bin"
SLEDGERT="$BIN_DIR/sledgert"
WASM="$BIN_DIR/empty.wasm.so"

red()   { printf '\033[1;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
info()  { printf '\033[0;36m==> %s\033[0m\n' "$*"; }

# --- prerequisites ------------------------------------------------------------
if ! command -v hey >/dev/null 2>&1; then
	red "ERROR: 'hey' is not installed. Install it with:"
	echo "       go install github.com/rakyll/hey@latest   (then add \$(go env GOPATH)/bin to PATH)"
	echo "       or:  apt-get install -y hey"
	exit 1
fi

if [[ ! -x "$SLEDGERT" ]]; then
	red "ERROR: $SLEDGERT not found. Build the runtime first, e.g.:"
	echo "       make runtime            # builds runtime/bin/sledgert"
	echo "       make install            # full build incl. wasm apps"
	exit 1
fi

if [[ ! -f "$WASM" ]]; then
	red "ERROR: $WASM not found. Build the sample apps first, e.g.:"
	echo "       make install            # builds the wasm apps incl. empty.wasm.so"
	exit 1
fi

# Warn (don't block) if running on a branch that already contains the fix.
if grep -q 'shutdown(client_socket' "$REPO_ROOT/runtime/include/tcp_session.h" 2>/dev/null; then
	info "NOTE: tcp_session.h contains the graceful-close fix — you are on a PATCHED"
	info "      checkout, so you should see 0 resets (the fix working). To see the BUG,"
	info "      check out an unpatched branch (e.g. master) and 'make runtime' first."
fi

# --- write a minimal tenant spec ----------------------------------------------
SPEC="$(mktemp /tmp/issue185-spec.XXXXXX.json)"
cat > "$SPEC" <<EOF
[
  {
    "name": "gwu",
    "port": $PORT,
    "routes": [
      {
        "route": "/empty",
        "path": "empty.wasm.so",
        "admissions-percentile": 70,
        "relative-deadline-us": 50000,
        "http-resp-content-type": "text/plain"
      }
    ]
  }
]
EOF

# --- generate the request body ------------------------------------------------
BODY="$(mktemp /tmp/issue185-body.XXXXXX)"
head -c "$BODY_BYTES" /dev/zero | tr '\0' 'x' > "$BODY"

# --- launch sledgert ----------------------------------------------------------
LOG="$(mktemp /tmp/issue185-sledge.XXXXXX.log)"
SLEDGE_PID=""

cleanup() {
	[[ -n "$SLEDGE_PID" ]] && kill "$SLEDGE_PID" 2>/dev/null || true
	rm -f "$SPEC" "$BODY" "$LOG"
}
trap cleanup EXIT

info "Starting sledgert on port $PORT (route /empty -> empty.wasm.so)"
# sledgert resolves the relative "empty.wasm.so" path against its CWD, and needs
# runtime/bin on LD_LIBRARY_PATH for libsledge/libck. 'exec' makes sledgert
# replace the subshell so $! is sledgert's own PID (so cleanup kills it).
( cd "$BIN_DIR" && exec env LD_LIBRARY_PATH="$BIN_DIR:${LD_LIBRARY_PATH:-}" "$SLEDGERT" "$SPEC" ) >"$LOG" 2>&1 &
SLEDGE_PID=$!

# --- wait for the tenant port to accept connections ---------------------------
info "Waiting for port $PORT to come up..."
for _ in $(seq 1 50); do
	if ! kill -0 "$SLEDGE_PID" 2>/dev/null; then
		red "sledgert exited during startup. Log:"
		cat "$LOG"
		exit 1
	fi
	if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
		exec 3>&- 3<&- 2>/dev/null || true
		break
	fi
	sleep 0.2
done

# Sanity checks: valid route -> 200, missing route (bodyless) -> 404 (clean).
ok="$(curl -s -o /dev/null -w '%{http_code}' -X POST --data hi "http://127.0.0.1:$PORT/empty" || true)"
nf="$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/nope" || true)"
info "Sanity: POST /empty -> $ok , GET /nope (bodyless) -> $nf"
if [[ "$ok" != "200" ]]; then
	red "sledgert is not serving the valid route as expected. Log:"
	cat "$LOG"
	exit 1
fi

# --- the load that triggers the bug -------------------------------------------
info "Firing $REQUESTS POSTs ($BODY_BYTES-byte body, concurrency $CONCURRENCY) at /nope (non-existent route)"
HEY_OUT="$(mktemp /tmp/issue185-hey.XXXXXX)"
hey -n "$REQUESTS" -c "$CONCURRENCY" -m POST -D "$BODY" "http://127.0.0.1:$PORT/nope" > "$HEY_OUT" 2>&1 || true

echo
echo "----- hey status code distribution -----"
sed -n '/Status code distribution/,/^$/p' "$HEY_OUT" || true

# Each reset is a distinct connection (unique source port), so count lines.
RESETS="$(grep -c 'connection reset by peer' "$HEY_OUT" || true)"
EPIPES="$(grep -c 'broken pipe' "$HEY_OUT" || true)"
rm -f "$HEY_OUT"

echo
echo "============================================================"
echo "  'connection reset by peer' errors : $RESETS"
echo "  'broken pipe' (EPIPE) errors       : $EPIPES   (known large-body limitation)"
echo "============================================================"
if [[ "${RESETS:-0}" -gt 0 ]]; then
	red "BUG REPRODUCED: SLEdge sent RSTs on early 404 responses (issue #185)."
	echo "On fix/issue-185-graceful-close this count drops to 0."
else
	green "No resets observed. If you are on the patched branch, this is the FIX working."
	echo "If you expected the bug: confirm you 'make runtime' on an unpatched branch,"
	echo "and try a larger BODY_BYTES (e.g. BODY_BYTES=1000000) or higher CONCURRENCY."
fi
