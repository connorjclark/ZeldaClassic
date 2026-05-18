#!/bin/bash

# The first argument is the GitHub username, passed via command= in authorized_keys
GIT_LFS_USER="$1"
export GIT_LFS_USER

# Read the requested SSH command into an array
read -r -a args <<< "$SSH_ORIGINAL_COMMAND"
cmd="${args[0]}"

# Check if the very first word is a permitted command
if [[ "$cmd" == "git-lfs-authenticate" ]]; then
    # Execute the exact absolute path to the binary, passing the remaining arguments
    exec /usr/local/bin/git-lfs-authenticate "${args[@]:1}"
elif [[ "$cmd" == "git-lfs-transfer" ]]; then
    # Git LFS 3.0+ tries this first. Giftless doesn't support pure SSH transfer,
    # so we exit with 1 to force the client to fall back to the authenticate/HTTPS path.
    exit 1
else
    echo "Restricted account: Command '$cmd' not permitted."
    exit 1
fi
