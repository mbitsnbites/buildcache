#!/bin/bash
# These shellcheck warnings are handled implicitly by trap_handler:
# shellcheck disable=SC2164

# Check if we're running on Windows.
function is_windows() {
    local u
    u=$(uname -s)
    if [ "${u:0:5}" == "MINGW" ]; then
        return 0
    fi
    return 1
}

function cleanup() {
    [[ -n "${PROJDIR}" ]] && rm -rf "${PROJDIR}"
    [[ -n "${BUILDCACHE_DIR}" ]] && rm -rf "${BUILDCACHE_DIR}"
    [[ -n "${LOG_DIR}" ]] && rm -rf "${LOG_DIR}"
    [[ -n "${SYMLINKSDIR}" ]] && rm -rf "${SYMLINKSDIR}"
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

# Set up a working environment for Windows if needed.
EXESUFFIX=
if is_windows; then
    EXESUFFIX=".exe"
fi

# Find the buildcache executable.
BUILDCACHEDIR="$(pwd)"
BUILDCACHEEXE="${BUILDCACHEDIR}/buildcache${EXESUFFIX}"
if [[ ! -x "${BUILDCACHEEXE}" ]]; then
    echo "ERROR: Not a BuildCache executable: ${BUILDCACHEEXE}"
    echo "Note: This script is expected to be run from the BuildCache build folder."
    exit 1
fi
echo "BuildCache executable: ${BUILDCACHEEXE}"

# Install BuildCache and symlinks to BuildCache for common compilers.
SYMLINKSDIR="${BUILDCACHEDIR}/symlinks"
echo "Install symlinks in: ${SYMLINKSDIR}"
rm -rf "${SYMLINKSDIR}" ; mkdir -p "${SYMLINKSDIR}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/cc${EXESUFFIX}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/c++${EXESUFFIX}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/gcc${EXESUFFIX}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/g++${EXESUFFIX}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/clang${EXESUFFIX}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/clang++${EXESUFFIX}"
ln -s "${BUILDCACHEEXE}" "${SYMLINKSDIR}/cl${EXESUFFIX}"
export PATH="${SYMLINKSDIR}:${BUILDCACHEDIR}:${PATH}"

export CMAKE_OSX_ARCHITECTURES="x86_64;arm64"

# Configure BuildCache.
export BUILDCACHE_DIR=/tmp/.buildcache-$$
rm -rf "${BUILDCACHE_DIR}" ; mkdir -p "${BUILDCACHE_DIR}"
export BUILDCACHE_DEBUG=2
export BUILDCACHE_DIRECT_MODE=true
export LOG_DIR=/tmp/.buildcache-logs-$$
rm -rf "${LOG_DIR}" ; mkdir -p "${LOG_DIR}"

# Clone the project.
PROJDIR="/tmp/proj-$$"
SRCDIR="${PROJDIR}/src"
BUILDDIR="${PROJDIR}/out"
rm -rf "${PROJDIR}"
PROJURL=https://github.com/mbitsnbites/buildcache.git
PROJVERSION=master
git clone --branch ${PROJVERSION} --depth 1 ${PROJURL} "${PROJDIR}"

echo "======== First build (cold cache) ========"
buildcache -C

cd "${PROJDIR}"
rm -rf "${BUILDDIR}" ; mkdir -p "${BUILDDIR}"
cd "${BUILDDIR}"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release "${SRCDIR}"

time BUILDCACHE_LOG_FILE="${LOG_DIR}/test-pass-1.log" ninja
buildcache -s
echo "--- LOG (pass 1) ---"
cat "${LOG_DIR}/test-pass-1.log"

echo "======== Second build (warm cache, direct mode OFF ) ========"
cd "${PROJDIR}"
rm -rf "${BUILDDIR}" ; mkdir -p "${BUILDDIR}"
cd "${BUILDDIR}"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release "${SRCDIR}"

time BUILDCACHE_LOG_FILE="${LOG_DIR}/test-pass-2.log" BUILDCACHE_DIRECT_MODE=false ninja
buildcache -s
echo "--- LOG (pass 2) ---"
cat "${LOG_DIR}/test-pass-2.log"

echo "======== Third build (warm cache, direct mode ON ) ========"
cd "${PROJDIR}"
rm -rf "${BUILDDIR}" ; mkdir -p "${BUILDDIR}"
cd "${BUILDDIR}"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release "${SRCDIR}"

time BUILDCACHE_LOG_FILE="${LOG_DIR}/test-pass-3.log" ninja
buildcache -s
echo "--- LOG (pass 3) ---"
cat "${LOG_DIR}/test-pass-3.log"

# Make sure that there were no cache misses during the second pass.
EXIT_CODE=0
if grep -q "Cache miss" "${LOG_DIR}/test-pass-2.log"; then
    echo "*** FAIL: The second build pass contained cache misses."
    EXIT_CODE=1
fi

# Make sure that there were no direct mode cache misses during the third pass.
if grep -q "Direct mode cache miss" "${LOG_DIR}/test-pass-3.log"; then
    echo "*** FAIL: The third build pass contained cache misses."
    EXIT_CODE=1
fi

# Clean up.
cleanup

if [[ "${EXIT_CODE}" == "0" ]]; then
    echo "----------------------------------------------------------"
    echo "The test passed!"
fi
exit ${EXIT_CODE}
