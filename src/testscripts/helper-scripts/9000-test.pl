#! /usr/bin/perl -CASDL

use strict;

printf "0;30:   \e[0m";
for (0..7) {
    printf "\e[%dmXX", 30+$_;
}
printf "\e[m <- should be dark\n";

printf "1;30:   \e[1m";
for (0..7) {
    printf "\e[%dmXX", 30+$_;
}
printf "\e[m <- should be bright, bold or both\n";
printf "\n";

printf "0;38+0: \e[0m";
for (0..7) {
    printf "\e[38;5;%dm\x{2588}", $_;
    printf "\e[48;5;%dm\e[38;5;0mX", $_;
}
printf "\e[m <- should be 8 blocks, dark\n";

printf "1;38+0: \e[1m";
for (0..7) {
    printf "\e[38;5;%dm\x{2588}", $_;
    printf "\e[48;5;%dm\e[38;5;0mX", $_;
}
printf "\e[m <- should be 8 blocks, dark and bold\n";

printf "0;38+8: \e[0m";
for (0..7) {
    printf "\e[38;5;%dm\x{2588}", $_+8;
    printf "\e[48;5;%dm\e[38;5;0mX", $_+8;
}
printf "\e[m <- should be 8 blocks, bright\n";

printf "1;38+8: \e[1m";
for (0..7) {
    printf "\e[38;5;%dm\x{2588}", $_+8;
    printf "\e[48;5;%dm\e[38;5;0mX", $_+8;
}
printf "\e[m <- should be 8 blocks, bright and bold\n";

