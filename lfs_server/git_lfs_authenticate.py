#!/usr/bin/env python3

import os
import sys
import json
import time
import jwt

# This script is called by Git LFS via SSH:
# ssh user@host git-lfs-authenticate <repo> <operation>
# See https://github.com/git-lfs/git-lfs/blob/main/docs/api/authentication.md

# Only authorized users can login to the 'git' user (see infra/scripts/sync_github_keys.sh) - so if
# this script runs, it always returns an authorized key.

ENV_FILE = "/home/zc/ZQuestClassic/lfs_server/.env"
LFS_URL = "https://lfs.zquestclassic.com"


def get_secret():
    """Read GIFTLESS_JWT_SECRET from .env file."""
    if not os.path.exists(ENV_FILE):
        return None
    with open(ENV_FILE, 'r') as f:
        for line in f:
            if line.startswith('GIFTLESS_JWT_SECRET='):
                return line.split('=', 1)[1].strip()
    return None


SECRET = get_secret()

if not SECRET:
    print(
        f"Error: GIFTLESS_JWT_SECRET not set in {ENV_FILE}",
        file=sys.stderr,
    )
    sys.exit(1)


def main():
    if len(sys.argv) < 3:
        print("Usage: git-lfs-authenticate <repo> <operation>", file=sys.stderr)
        sys.exit(1)

    org_and_repo = sys.argv[1].lstrip('/')
    operation = sys.argv[2]  # 'upload' or 'download'

    if not org_and_repo.startswith('ZQuestClassic/'):
        print("That org is not allowed.", file=sys.stderr)
        sys.exit(1)

    # Determine identity from SSH environment (passed by our wrapper)
    user = os.environ.get('GIT_LFS_USER', 'git')

    now = int(time.time())
    # The scopes format is: obj:{org}/{repo}:{actions}
    actions = "read,write" if operation == "upload" else "read"
    scope = f"obj:{org_and_repo}:{actions}"
    payload = {
        "iat": now,
        "exp": now + 3600,  # 1 hour
        "sub": user,
        "scopes": [scope],
    }

    token = jwt.encode(payload, SECRET, algorithm="HS256")

    # Output JSON expected by Git LFS
    # We use the full normalized repo path in the href to ensure
    # the client uses the correct routing (org/repo)
    result = {
        "header": {"Authorization": f"Bearer {token}"},
        "href": f"{LFS_URL}/{org_and_repo}",
    }

    print(json.dumps(result))


if __name__ == "__main__":
    main()
