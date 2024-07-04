#!/bin/bash

set -e

if [ ! -d ../venv ] ; then
	python -m venv ../venv
fi

source ../venv/bin/activate
pip install -r ../requirements.txt

export FLASK_APP=server
export FLASK_S3_BUCKET=zc-replays
export FLASK_S3_URL=https://nyc3.digitaloceanspaces.com
export FLASK_S3_REGION=ny3
export FLASK_DATA_DIR=../.tmp/api-server

CONFIG_PATH="$PWD/secrets.sh"
if [ -f "$CONFIG_PATH" ] ; then
	source "$CONFIG_PATH"
	if [[ -z "${FLASK_S3_ACCESS_KEY}" ]]; then
		echo "missing FLASK_S3_ACCESS_KEY"
		exit 1
	fi
	if [[ -z "${FLASK_S3_SECRET_ACCESS_KEY}" ]]; then
		echo "missing FLASK_S3_SECRET_ACCESS_KEY"
		exit 1
	fi
else
	echo "No config file at $CONFIG_PATH"
fi

echo "syncing from s3://$FLASK_S3_BUCKET/ ..."
mkdir -p "$FLASK_DATA_DIR/replays"
s3cmd sync --no-preserve --acl-public "s3://$FLASK_S3_BUCKET/" "$FLASK_DATA_DIR/replays/"
echo "done syncing from s3"

curl https://data.zquestclassic.com/manifest.json > "$FLASK_DATA_DIR/manifest.json"

if [[ -z "${FLASK_PRODUCTION}" ]]; then
	flask run -p 8080
else
	uwsgi --ini server.ini
fi
