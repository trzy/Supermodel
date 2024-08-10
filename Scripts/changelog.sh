#!/bin/bash

TEMPFILE=$(mktemp)
cleanup() {
    test -f "${TEMPFILE}" && rm "${TEMPFILE}"
}
trap 'cleanup' EXIT

FROM_REF=${FROM_REF:-06594a5}
REF=${REF:-$(git rev-parse --short HEAD)}
OUTPUT=${OUTPUT:-/dev/stdout}

# Log from first commit after 0.2a release until now

if ! git -c color.ui=never log ${FROM_REF}...${REF} --no-decorate --format=format:"Commit %h on %ad by %an%n%n%w(80,4,4)%B" --date=short > "${TEMPFILE}" ; then
    echo >&2 "Unable to obtain Git log"
    return 1
fi

cat <<EOF | cat - "${TEMPFILE}" > "${OUTPUT}"

  ####                                                      ###           ###
 ##  ##                                                      ##            ##
 ###     ##  ##  ## ###   ####   ## ###  ##  ##   ####       ##   ####     ##
  ###    ##  ##   ##  ## ##  ##   ### ## ####### ##  ##   #####  ##  ##    ##
    ###  ##  ##   ##  ## ######   ##  ## ####### ##  ##  ##  ##  ######    ##
 ##  ##  ##  ##   #####  ##       ##     ## # ## ##  ##  ##  ##  ##        ##
  ####    ### ##  ##      ####   ####    ##   ##  ####    ### ##  ####    ####
                 ####

                       A Sega Model 3 Arcade Emulator.

                   Copyright 2003-2024 The Supermodel Team

                                 CHANGE LOG

EOF

