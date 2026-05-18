#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "$SCRIPT_DIR"

set -e

SERVER=api.zquestclassic.com

ssh root@$SERVER "cd /home/zc/ZQuestClassic && git fetch && git checkout origin/main"

scp nginx/* root@$SERVER:/etc/nginx/conf.d/
scp systemd/* root@$SERVER:/etc/systemd/system/
scp crontab/* root@$SERVER:/etc/cron.d/
scp sshd/* root@$SERVER:/etc/ssh/sshd_config.d/

# Deploy LFS server (Docker)
ssh root@$SERVER "cd /home/zc/ZQuestClassic/lfs_server && \
    set -a && . .env && set +a && \
    envsubst < giftless.template.yaml > giftless.yaml && \
    docker compose --env-file .env down && docker compose --env-file .env up -d"

# Install SSH LFS authenticator
scp ../lfs_server/git_lfs_authenticate.py root@$SERVER:/usr/local/bin/git-lfs-authenticate
ssh root@$SERVER "chmod +x /usr/local/bin/git-lfs-authenticate"
ssh root@$SERVER "apt-get update && apt-get install -y python3-jwt"

# Security hardening for 'git' user
# Ensure the user exists and has a shell (needed for sshd to bootstrap ForceCommand)
ssh root@$SERVER "id -u git >/dev/null 2>&1 || useradd -m -s /bin/bash git"
ssh root@$SERVER "usermod -s /bin/bash git"

# Deploy SSH wrapper script
scp ../lfs_server/git-lfs-wrapper.sh root@$SERVER:/usr/local/bin/git-lfs-wrapper.sh
ssh root@$SERVER "chmod +x /usr/local/bin/git-lfs-wrapper.sh"

ssh root@$SERVER "sshd -t"
ssh root@$SERVER "nginx -t"
ssh root@$SERVER "systemctl daemon-reload"
ssh root@$SERVER "systemctl restart cron nginx zc.api_server ssh"
