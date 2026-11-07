#!/usr/bin/env bash

# Workaround XProtect race condition for "hdiutil create" for MacOS
# https://gitlab.kitware.com/cmake/cmake/-/issues/25671
# https://github.com/HEXRD/hexrdgui/blob/master/packaging/hdiutil_repeat.sh

# BSD 3-Clause License

# Copyright (c) 2024, Kitware, Inc., Lawrence Livermore National Laboratory,
# and Air Force Research Laboratory.
# All rights reserved.

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:

# 1. Redistributions of source code must retain the above copyright notice, this
#     list of conditions and the following disclaimer.

# 2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.

# 3. Neither the name of the copyright holder nor the names of its contributors
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e

# Debugging: log every hdiutil call, and what is holding the volume when one fails.
# CPack doesn't show hdiutil's output, so this goes to a file the workflow prints.
log="${HDIUTIL_REPEAT_LOG:-${RUNNER_TEMP:-/tmp}/hdiutil_repeat.log}"
out_file=$(mktemp)
err_file=$(mktemp)
trap 'rm -f "$out_file" "$err_file"' EXIT

# Some of these can hang on a wedged volume.
with_timeout() {
	perl -e 'alarm shift; exec @ARGV or die "exec failed: $!"' "$@"
}

diagnose() {
	local vol="$1"
	{
		echo "---- diagnostics: $(date -u +%H:%M:%S) ----"
		echo "## lsof $vol"
		# sudo to see root processes (mds, XProtect); CI runners have passwordless sudo.
		with_timeout 60 sudo -n lsof -n -P "$vol" 2>&1 || with_timeout 60 lsof -n -P "$vol" 2>&1 || true
		echo "## hdiutil info"
		with_timeout 30 hdiutil info 2>&1 || true
		echo "## mount"
		mount 2>&1 || true
		echo "## mdutil -s $vol"
		with_timeout 30 mdutil -s "$vol" 2>&1 || true
		echo "## ps (by cpu)"
		ps -Ao pid,ppid,user,%cpu,%mem,etime,command -r 2>&1 | head -40 || true
		echo "## log show (last 2m)"
		with_timeout 120 log show --last 2m --style compact --predicate \
			'process IN {"diskarbitrationd", "hdiutil", "diskimagesiod", "diskimages-helper", "Finder", "osascript", "mds", "mds_stores", "mdworker_shared", "XprotectService", "XProtect", "syspolicyd", "fseventsd", "kernel"} OR eventMessage CONTAINS[c] "ZQuest Classic"' \
			2>&1 | tail -400 || true
		echo "---- end diagnostics ----"
	} >> "$log"
}

# Try repeatedly, up to 30 times.
# This prevents spurious errors caused by a race condition with XProtect
# See https://github.com/actions/runner-images/issues/7522
i=0
args=("$@")
while true; do
	start=$(date +%s)
	set +e
	hdiutil "${args[@]}" > "$out_file" 2> "$err_file"
	code=$?
	set -e
	# CPack parses the output of "hdiutil attach", so pass it through unchanged.
	cat "$out_file"
	cat "$err_file" >&2
	{
		echo "[$(date -u +%H:%M:%S)] try $i: hdiutil ${args[*]} -> exit $code ($(( $(date +%s) - start ))s)"
		if [ $code -ne 0 ]; then
			sed 's/^/  stdout: /' "$out_file"
			sed 's/^/  stderr: /' "$err_file"
		fi
	} >> "$log"
	if [ $code -eq 0 ]; then break; fi

	case $i in
		0|1|4|5|10|20|30) diagnose "${args[${#args[@]}-1]}" ;;
	esac

	if [ $i -eq 30 ]; then exit 1; fi
	i=$((i+1))
	# Local edit: a volume can stay busy (Spotlight, XProtect, Finder) for longer than all the
	# retries, so after a few tries force the detach.
	if [ "$1" = "detach" ] && [ $i -eq 5 ]; then
		args=(detach -force "${@:2}")
	fi
	sleep 5
done
