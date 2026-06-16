#!/bin/bash
# =============================================================================
# Run the SLEdge runtime under Valgrind / Memcheck.
#
# The runtime is hard for Valgrind to analyze: signal-based preemption, custom
# user-level context switches, and direct mmap of sandbox memory all produce
# false positives (see docs/valgrind.md). This script offers two modes:
#
#   control-plane (default) - preemption OFF, a single worker. This is the
#       configuration the per-test `make valgrind` targets use. Valgrind output
#       is clean, so it is the right mode for hunting real leaks and errors in
#       the runtime control plane (request parsing, scheduling bookkeeping,
#       teardown, etc.).
#
#   preemptive - preemption ON with multiple workers. The architectural false
#       positives are filtered with sledge.supp so that any *remaining* report
#       is worth investigating.
#
# Usage:
#   run-valgrind.sh [-m control-plane|preemptive] [-w NWORKERS] [-d SPEC_DIR] [-- valgrind-args...]
#
# Examples:
#   runtime/tools/valgrind/run-valgrind.sh -d tests/multi-tenancy-sample
#   runtime/tools/valgrind/run-valgrind.sh -m preemptive -w 8 -d tests/multi-tenancy-sample
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BINARY_DIR="$REPO_ROOT/runtime/bin"
SUPP="$SCRIPT_DIR/sledge.supp"

MODE="control-plane"
NWORKERS=""
SPEC_DIR="$PWD"

while [[ $# -gt 0 ]]; do
	case "$1" in
		-m | --mode) MODE="$2"; shift 2 ;;
		-w | --workers) NWORKERS="$2"; shift 2 ;;
		-d | --dir) SPEC_DIR="$2"; shift 2 ;;
		--) shift; break ;;
		-h | --help) sed -n '2,30p' "$0"; exit 0 ;;
		*) echo "Unknown option: $1" >&2; exit 1 ;;
	esac
done
EXTRA_VG_ARGS=("$@")

if [[ ! -x "$BINARY_DIR/sledgert" ]]; then
	echo "error: $BINARY_DIR/sledgert not found. Build the runtime first (make -C runtime)." >&2
	exit 1
fi
if [[ ! -f "$SPEC_DIR/spec.json" ]]; then
	echo "error: $SPEC_DIR/spec.json not found. Pass a test directory with -d." >&2
	exit 1
fi

# The suppression file only filters provably-false-positive classes (signal
# machinery, JIT'd guest code, WASI shims touching guest memory), none of which
# is where a real control-plane bug would hide, so it is applied in both modes.
VG_COMMON=(
	--leak-check=full
	--show-leak-kinds=all
	--max-stackframe=11150456
	--run-libc-freeres=no
	--run-cxx-freeres=no
	--error-exitcode=99
	"--suppressions=$SUPP"
)

# Force eager symbol binding so the dynamic linker resolves the dlopen'd wasm
# module up front instead of lazily at call time, where Valgrind reports false
# "Invalid read" in _dl_fixup/do_lookup_x against the module's PLT/GOT.
export LD_BIND_NOW=1

case "$MODE" in
	control-plane)
		export SLEDGE_DISABLE_PREEMPTION=true
		export SLEDGE_NWORKERS="${NWORKERS:-1}"
		;;
	preemptive)
		unset SLEDGE_DISABLE_PREEMPTION || true
		export SLEDGE_NWORKERS="${NWORKERS:-4}"
		;;
	*)
		echo "error: unknown mode '$MODE' (expected control-plane or preemptive)" >&2
		exit 1
		;;
esac

echo "Mode: $MODE | Workers: $SLEDGE_NWORKERS | Spec: $SPEC_DIR/spec.json | Suppressions: $SUPP"

cd "$SPEC_DIR"
export LD_LIBRARY_PATH="$BINARY_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec valgrind "${VG_COMMON[@]}" "${EXTRA_VG_ARGS[@]}" "$BINARY_DIR/sledgert" spec.json
