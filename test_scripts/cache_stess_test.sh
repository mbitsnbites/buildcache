#!/bin/bash
# -*- mode: sh; tab-width: 4; indent-tabs-mode: nil; -*-
# ------------------------------------------------------------------------------
# Copyright (c) 2021 Marcus Geelnard
#
# This software is provided 'as-is', without any express or implied warranty. In
# no event will the authors be held liable for any damages arising from the use
# of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it freely,
# subject to the following restrictions:
#
#  1. The origin of this software must not be misrepresented; you must not claim
#     that you wrote the original software. If you use this software in a
#     product, an acknowledgment in the product documentation would be
#     appreciated but is not required.
#
#  2. Altered source versions must be plainly marked as such, and must not be
#     misrepresented as being the original software.
#
#  3. This notice may not be removed or altered from any source distribution.
# ------------------------------------------------------------------------------

# These shellcheck warnings are handled implicitly by trap_handler:
# shellcheck disable=SC2164

function cleanup() {
    [[ -n "${BUILD_DIR}" ]] && rm -rf "${BUILD_DIR}"
    [[ -n "${BUILDCACHE_DIR}" ]] && rm -rf "${BUILDCACHE_DIR}"
    [[ -n "${LOG_DIR}" ]] && rm -rf "${LOG_DIR}"
}

# Treat errors with exit code 1.
function trap_handler() {
    MYSELF="$0"
    LASTLINE="$1"
    LASTERR="$2"
    echo "ERROR: ${MYSELF}: line ${LASTLINE}: exit status of last command: ${LASTERR}"

    cleanup
    exit "${LASTERR}"
}
trap 'trap_handler ${LINENO} ${$?}' ERR INT TERM

# Find the buildcache executable.
BUILDCACHEDIR="$(pwd)"
BUILDCACHEEXE="${BUILDCACHEDIR}/buildcache"
if [[ ! -x "${BUILDCACHEEXE}" ]]; then
    echo "ERROR: Not a BuildCache executable: ${BUILDCACHEEXE}"
    echo "Note: This script is expected to be run from the BuildCache build folder."
    exit 1
fi
echo "BuildCache executable: ${BUILDCACHEEXE}"

# Find the dummy tool & wrapper.
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
DUMMYTOOL=${SCRIPTDIR}/dummy/dummy.py
export BUILDCACHE_LUA_PATH=${SCRIPTDIR}/dummy/wrapper

# Configure BuildCache.
export BUILDCACHE_DIR=/tmp/.buildcache-$$
rm -rf "${BUILDCACHE_DIR}"
mkdir -p "${BUILDCACHE_DIR}"
export BUILDCACHE_DEBUG=3
#export BUILDCACHE_DIRECT_MODE=true
export LOG_DIR=/tmp/.buildcache-logs-$$
rm -rf "${LOG_DIR}"
mkdir -p "${LOG_DIR}"

# Create a source file.
export BUILD_DIR=/tmp/.buildcache-build-$$
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
INFILE=${BUILD_DIR}/source
cat <<EOF > "${INFILE}"
This is a test input file
EOF

echo "======== Stressing the cache ========"
buildcache -C

# Repeat...
for _ in {1..100}; do
    # Run a lot of BuildCache instances in parallel.
    pids=""
    for i in {1..100}; do
        OUTFILE=${BUILD_DIR}/out-${i}
        # BUILDCACHE_LOG_FILE="${LOG_DIR}/test-pass-1.log"
        "${BUILDCACHEEXE}" "${DUMMYTOOL}" -o "${OUTFILE}" "${INFILE}" &
        pids+=" $!"
    done

    # Wait for all processes to finish.
    got_error=false
    for p in $pids; do
        if ! wait "$p"; then
            got_error=true
        fi
    done
done

# Did we have an error exit status from any of the processes?
if $got_error; then
    echo "*** FAIL: At least one of the processes failed."
    exit 1
fi

buildcache -s
#echo "--- LOG (pass 1) ---"
#cat "${LOG_DIR}/test-pass-1.log"
EXIT_CODE=0

# Clean up.
cleanup

if [[ "${EXIT_CODE}" == "0" ]]; then
    echo "----------------------------------------------------------"
    echo "The test passed!"
fi
exit ${EXIT_CODE}
