#!/bin/sh

# Testcase:
# Check if it runs at all
#
exit

source ./lib.sh

check_char() {
	touch /tmp/_check_char_file
	printf "$1\n" >> /tmp/_check_char_file
	printf " $1\n" >> /tmp/_check_char_file
	printf "  $1\n" >> /tmp/_check_char_file
	printf "   $1\n" >> /tmp/_check_char_file
	printf "    $1\n" >> /tmp/_check_char_file
	printf ".$1.\n" >> /tmp/_check_char_file
	printf " .$1.\n" >> /tmp/_check_char_file
	printf "  .$1.\n" >> /tmp/_check_char_file
	printf "   .$1.\n" >> /tmp/_check_char_file
	printf "    .$1.\n" >> /tmp/_check_char_file

	${SCREEN_BIN} -S "testsuite" -X stuff "clear\n"
	${SCREEN_BIN} -S "testsuite" -X stuff "cat /tmp/_check_char_file\n"

	rm /tmp/_check_char_file
	check_scrot "0101-result-$1.png"
}

prepare $0

run_term -S "testsuite" -c /dev/null $SHELL $_SHELLOPTS

check_session "testsuite"

# skip ANSI
for i in `seq 256 65535`; do
	hex=$(( echo "obase=16"; echo $i ) | bc)
	check_char "\u${hex}"
done

${SCREEN_BIN} -S "testsuite" -X quit

cleanup $0
