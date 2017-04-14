#!/bin/sh

# Testcase:
# Check if it runs at all
#

source ./lib.sh

prepare $0

run_term -S "testsuite" -c /dev/null $SHELL $_SHELLOPTS

check_session "testsuite"
check_scrot "0001-result.png"

${SCREEN_BIN} -S "testsuite" -X "quit"

cleanup $0
