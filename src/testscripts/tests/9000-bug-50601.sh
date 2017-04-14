#!/bin/sh

# Testcase:
# bug #50601: Screen rewrites 38;5;nn escapes even when surrounding terminal supports them
#

source ./lib.sh

prepare $0

run_term -S "testsuite" -c /dev/null $SHELL $_SHELLOPTS

${SCREEN_BIN} -S "testsuite" -X stuff "./helper-scripts/9000-test.pl\n"

check_session "testsuite"
check_scrot "9000-result.png"

${SCREEN_BIN} -S "testsuite" -X "quit"

cleanup $0
