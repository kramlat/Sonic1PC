#!/bin/sh
# Launches the game directly under gdb, so it becomes gdb's direct child
# process -- this system's ptrace_scope (kernel.yama.ptrace_scope=1)
# refuses to let gdb attach to an already-running process it didn't spawn
# itself, so attaching after the fact doesn't work here. Starting under gdb
# from the beginning sidesteps that entirely.
#
# Deliberately scoped to only the debug-oriented builds (ASan+Debug from
# build_asan, or a plain Debug build from build if one exists) -- release/
# premier/showcase builds aren't what this is for.
#
# Usage:
#   ./debug.sh [asan|debug]              interactive: play normally, Ctrl+C
#                                         in this terminal to drop to (gdb).
#   ./debug.sh --trace [asan|debug]      background session driven by a
#                                         FIFO, so an external tool/session
#                                         (not just this terminal) can send
#                                         gdb commands at any time via
#                                         gdb-cmd.sh -- see that script.
#
# -nx skips this system's own ~/.gdbinit, which throws a Python error on
# load and isn't needed for any of this.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION_DIR="$SCRIPT_DIR/.gdbsession"

TRACE=0
if [ "$1" = "--trace" ]; then
    TRACE=1
    shift
fi

case "${1:-asan}" in
    asan)
        BINARY="$SCRIPT_DIR/bin/Debug/Sonic" # build_asan's own output (CMAKE_BUILD_TYPE=Debug, -fsanitize=address)
        ;;
    debug)
        BINARY="$SCRIPT_DIR/bin/Sonic" # plain build/ output
        ;;
    *)
        echo "Usage: $0 [--trace] [asan|debug]" >&2
        exit 1
        ;;
esac

if [ ! -x "$BINARY" ]; then
    echo "Not found or not built yet: $BINARY" >&2
    exit 1
fi

if [ "$TRACE" = "0" ]; then
    # Once it's running: play normally. Press Ctrl+C in this terminal at any
    # point to interrupt and drop to a (gdb) prompt without killing the game
    # (e.g. "print dle_routine", "print *player") -- type "continue" to resume.
    exec gdb -nx -ex run --args "$BINARY"
fi

# --trace mode: a FIFO-driven background session. gdb's stdin reads from the
# FIFO instead of this terminal, so gdb-cmd.sh (run from anywhere, any time)
# can feed it commands. Output goes to out.log instead of the terminal.
mkdir -p "$SESSION_DIR"
rm -f "$SESSION_DIR/cmd_fifo" "$SESSION_DIR/out.log"
mkfifo "$SESSION_DIR/cmd_fifo"
: > "$SESSION_DIR/out.log"

# Hold a writer open on the FIFO for the life of this script, so it doesn't
# see EOF (and gdb doesn't exit) the instant gdb-cmd.sh's own single "echo >
# fifo" writer closes after each command.
exec 3> "$SESSION_DIR/cmd_fifo"

echo "$$" > "$SESSION_DIR/launcher.pid"
gdb -nx -q -ex run --args "$BINARY" < "$SESSION_DIR/cmd_fifo" > "$SESSION_DIR/out.log" 2>&1 &
echo "$!" > "$SESSION_DIR/gdb.pid"
echo "gdb pid $(cat "$SESSION_DIR/gdb.pid"), log: $SESSION_DIR/out.log, fifo: $SESSION_DIR/cmd_fifo"
wait
