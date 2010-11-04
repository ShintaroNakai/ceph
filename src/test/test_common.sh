#!/bin/bash -x

#
# test_common.sh
#
# Common routines for tests
#
#
# Environment variables that affect tests:
# KEEP_TEMPDIR                  If set, the tempdir will not be deleted
#                               when the test is over.
#

# Clean up the temporary directory
cleanup() {
        if [ -n ${TEMPDIR} ]; then
                rm -rf "${TEMPDIR}"
        fi
}

# Create a temporary directory where test files will be stored.
setup_tempdir() {
        TEMPDIR=`mktemp -d`
        if [ -z $KEEP_TEMPDIR ]; then
                trap cleanup INT TERM EXIT
        fi
}

# Standard initialization function for tests
init() {
        setup_tempdir
        cd `dirname $0`/..
}

# Exit with an error message.
die() {
        echo $@
        exit 1
}

# Stop an OSD started by vstart
stop_osd() {
        osd_index=$1
        pidfile="out/osd.$osd_index.pid"
        if [ -e $pidfile ]; then
                kill `cat $pidfile` && return 0
        else
                echo "cosd process $osd_index is not running"
        fi
        return 1
}

# Restart an OSD started by vstart
restart_osd() {
        osd_index=$1
        ./cosd -i $osd_index -c ceph.conf &
}

# Ask the user a yes/no question and get the response
yes_or_no_choice() {
        while true; do
                echo -n "${1} [y/n] "
                read ans
                case "${ans}" in
                        y|Y|yes|YES|Yes) return 0 ;;
                        n|N|no|NO|No) return 1 ;;
                        *) echo "Please type yes or no."
                        echo ;;
                esac
        done
}

# Block until the user says "continue" or "c"
continue_prompt() {
        prompt=${1:-"to go on"}
        while true; do
                echo "Please type 'c' or 'continue' ${prompt}."
                read ans
                case "${ans}" in
                        c|continue) return 0 ;;
                        *) echo ;;
                esac
        done
}

# Write a bunch of objects to rados
write_objects() {
        start_ver=$1
        stop_ver=$2
        num_objs=$3
        obj_size=$4
        [ -d "${TEMPDIR}" ] || die "must setup_tempdir"
        for v in `seq $start_ver $stop_ver`; do
                chr=`perl -e "print chr(48+$v)"`
                head -c $obj_size /dev/zero  | tr '\0' "$chr" > $TEMPDIR/ver$v
                for i in `seq -w 1 $num_objs`; do
                        ./rados -p data put obj$i $TEMPDIR/ver$v || die "radostool failed"
                done
        done
}

init