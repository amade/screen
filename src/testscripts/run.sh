#!/bin/sh

#############
# RUN TESTS #
#############

for i in `ls -1 tests/*.sh`; do
	sh ${i}
done
