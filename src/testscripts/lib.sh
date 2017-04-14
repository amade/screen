#!/bin/sh

# FIXME
#add check for xterm scrot xdotool
# xterm
# scrot

# FIXME

# *.font:                 -unscii-unscii-medium-r-normal-full-17-160-75-75-c-80-iso10646-1
# use GNU Unifont instead

###############
# ENVIRONMENT #
###############

export XTERM_BIN=${XTERM_BIN:-xterm}
export SCREEN_BIN=${SCREEN_BIN:-screen}
export SHELL_BIN=${SHELL_BIN:-bash}
export XDOTOOL_BIN=${XDOTOOL_BIN:-xdotool}

####################
# HELPER FUNCTIONS #
####################

prepare() {
	echo "===================="
	echo "Starting $1 testrun"
	echo "--------------------"

	export TMPDIR=$(mktemp -d)
	mkdir ${TMPDIR}/screendir

	# run "standardized" shell
	export SHELL=$(which ${SHELL_BIN})
	export _SHELLOPTS="--noprofile --norc"
	export PS1="$"
	export RESULTS=0
	export PASSED=0

	#mkdir ${TMPDIR}/fonts
	#cp fonts/*.ttf ${TMPDIR}/fonts #Ugh...
	#pushd ${TMPDIR}/fonts
	#mkfontdir
	#xset fp+ ${PWD}/fonts
	#xset fp rehash
	#popd
}

cleanup() {
	echo "--------------------"
	echo "Finished $1 testrun"
	if [ ${PASSED} -eq ${RESULTS} ]; then
		tput setaf 2;
	else
		tput setaf 1;
	fi
	echo "Passed ${PASSED} of ${RESULTS}"
	tput sgr0
	echo "===================="

	rm -rf ${TMPDIR}
	export TMPDIR=

	export RESULTS=
	export PASSED=
}

check_scrot() {
	echo "Comparing screenshots"
	export RESULTS=$(( ${RESULTS} + 1 ))

	sleep 1 # wait to settle

	scrot -u ${TMPDIR}/result.png
	cp ${TMPDIR}/result.png /tmp

	# compare PNGs using diff...
	diff -uNr results/$1 ${TMPDIR}/result.png 2>&1 >/dev/null
	local res=$?
	if [[ ${res} -eq 0 ]]; then
		result_success
		export PASSED=$(( ${PASSED} + 1 ))
	else
		result_failure
	fi

}

check_session() {
	echo "Checking for session"
	export RESULTS=$(( ${RESULTS} + 1 ))

	sleep 1 # wait to settle

	${SCREEN_BIN} -ls $1 2>&1 >/dev/null
	local res=$?
	if [[ ${res} -eq 0 ]]; then
		result_success
		export PASSED=$(( ${PASSED} + 1 ))
	else
		result_failure
	fi
}

result_success () {
	tput setaf 2; echo PASSED; tput sgr0
}

result_failure () {
	tput setaf 1; echo FAILED; tput sgr0
}

run_term() {
	# +bc	- disable cursor blinking
	# -tn	- make sure we got all colors (xterm-256-color)
	# -e	- what to run, needs to be last

	# redirect xterm to background, so we can return and continue executing

	${XTERM_BIN} +bc \
		-font "-gnu-unifont csur-*-*-*-*-*-*-*-*-*-*-iso10646-1" \
		-tn xterm-256color \
		-e ${SCREEN_BIN} ${@} &

	# sleep, cause xterm takes time to start
	sleep 1
}

