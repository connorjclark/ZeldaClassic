#!/bin/bash

# Ensure the script exits if a command fails
set -e
# Error if any variable is unset.
set -u

# ==========================================
# Configuration
# ==========================================
source /home/zc/secrets.env

GITHUB_ORG="ZQuestClassic"
GITHUB_TOKEN="$LFS_GH_PAT"
AUTH_KEYS_FILE="/home/git/.ssh/authorized_keys"
TEMP_FILE="$(mktemp)"

echo "Starting GitHub SSH key sync for organization: $GITHUB_ORG"

# ==========================================
# Fetch Organization Members
# ==========================================

# TODO: it'll be a happy day if 100 isn't enough and we need pagination here.
MEMBERS_URL="https://api.github.com/orgs/$GITHUB_ORG/members?per_page=100"

echo "Fetching members..."
MEMBERS=$(curl -s -H "Authorization: token $GITHUB_TOKEN" \
                 -H "Accept: application/vnd.github.v3+json" \
                 "$MEMBERS_URL" | jq -r '.[].login')

if [ -z "$MEMBERS" ] || [ "$MEMBERS" == "null" ]; then
    echo "Error: Failed to retrieve members. Check your token and org name."
    rm -f "$TEMP_FILE"
    exit 1
fi

# ==========================================
# Fetch and Build Keys
# ==========================================
echo "Building new authorized_keys file..."

# Add a warning header so no one edits this file manually
echo "# This file is automatically managed by sync-github-keys.sh" > "$TEMP_FILE"
echo "# Do not edit manually! Your changes will be overwritten." >> "$TEMP_FILE"
echo "" >> "$TEMP_FILE"

for USER in $MEMBERS; do
    echo "Fetching keys for $USER..."
    
    KEYS_URL="https://api.github.com/users/$USER/keys"
    KEYS=$(curl -s -H "Authorization: token $GITHUB_TOKEN" \
                  -H "Accept: application/vnd.github.v3+json" \
                  "$KEYS_URL" | jq -r '.[].key')

    # If the user has keys, append them to the temp file
    if [ -n "$KEYS" ] && [ "$KEYS" != "null" ]; then
        echo "# GitHub User: $USER" >> "$TEMP_FILE"
        # We loop through the keys in case the user has multiple
        while IFS= read -r key; do
            # We use the command= prefix to pass the username to the wrapper.
            # We also include security flags as a backup to the sshd_config settings.
            echo "command=\"/usr/local/bin/git-lfs-wrapper.sh $USER\",no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty $key $USER@github" >> "$TEMP_FILE"
        done <<< "$KEYS"
        echo "" >> "$TEMP_FILE"
    fi
done

# ==========================================
# Atomic Replacement and Permissions
# ==========================================
echo "Applying new keys..."

# Ensure the .ssh directory exists with correct permissions
mkdir -p "$(dirname "$AUTH_KEYS_FILE")"
chmod 700 "$(dirname "$AUTH_KEYS_FILE")"
chown git:git "$(dirname "$AUTH_KEYS_FILE")"

# Atomically move the temp file into place
mv "$TEMP_FILE" "$AUTH_KEYS_FILE"

# Set strict permissions on the authorized_keys file
chmod 600 "$AUTH_KEYS_FILE"
chown git:git "$AUTH_KEYS_FILE"

echo "Sync complete! Keys updated successfully."
