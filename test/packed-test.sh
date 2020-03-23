#!/bin/bash -x

if [ -z $srcdir ]; then
    echo srcdir variable not set.
    exit 1
fi
source $srcdir/test/test-util.sh

find_db_name

# start a server with 2 second wait,
# 20s timeout, and my_test_db as database
test_start_server 2 20 $test_db_full

sleep 1

#####################

run_to 20 test/sdskv-packed-test $svr_addr 1 $test_db_name 100
if [ $? -ne 0 ]; then
    wait
    exit 1
fi

wait

echo cleaning up $TMPBASE
rm -rf $TMPBASE

exit 0
