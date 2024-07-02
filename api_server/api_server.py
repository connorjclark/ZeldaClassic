import json
import subprocess

from http import HTTPStatus
from pathlib import Path
from typing import Dict, List, Tuple

import boto3

from flask import Flask, flash, redirect, request, url_for
from werkzeug.exceptions import HTTPException

replays_dir = Path('.tmp/api_server/replays')

app = Flask(__name__)
app.config['S3_ACCESS_KEY_ID'] = 'DO00NWBCY2Z2HAZH6CC8'
app.config['S3_SECRET_ACCESS_KEY'] = os.environ['S3_SECRET_ACCESS_KEY']
app.config['S3_URL'] = 'https://nyc3.digitaloceanspaces.com'
app.config['S3_REGION'] = 'ny3'
app.config['S3_BUCKET'] = 'zc-replays'
bucket_name = app.config['S3_BUCKET']

def load_manifest():
    manifest_path = Path('.tmp/database/manifest.json')
    manifest = json.loads(manifest_path.read_text())
    qst_hash_dict = {}
    for entry_id, qst in manifest.items():
        if not entry_id.startswith('quests'):
            continue

        for release in qst['releases']:
            for i, resource in enumerate(release['resources']):
                if resource.endswith('.qst'):
                    qst_hash = release['resourceHashes'][i]
                    path = f'{entry_id}/{release["name"]}/{resource}'
                    qst_hash_dict[qst_hash] = path
    return qst_hash_dict

def connect_s3():
    session = boto3.session.Session()
    s3 = session.client('s3',
                            region_name=app.config['S3_REGION'],
                            endpoint_url=app.config['S3_URL'],
                            aws_access_key_id=app.config['S3_ACCESS_KEY_ID'],
                            aws_secret_access_key=app.config['S3_SECRET_ACCESS_KEY'])
    print(f'syncing {bucket_name} ...')
    replays_dir.mkdir(exist_ok=True, parents=True)
    subprocess.check_call(f's3cmd sync --no-preserve --acl-public s3://{bucket_name}/ {replays_dir}/'.split(' '))
    return s3

def parse_meta(replay_text: str) -> Dict[str, str]:
    meta = {}

    for line in replay_text.splitlines():
        if not line.startswith('M'):
            break

        _, key, value = line.strip().split(' ', 2)
        meta[key] = value

    return meta


def parse_replay(replay_text: str) -> Tuple[Dict[str, str], List[str]]:
    """Returns tuple: meta, steps"""
    meta = {}
    steps = []
    done_with_meta = False

    for line in replay_text.splitlines():
        if not line.startswith('M'):
            done_with_meta = True
        if done_with_meta and line:
            steps.append(line)
        else:
            _, key, value = line.strip().split(' ', 2)
            meta[key] = value

    return meta, steps

s3 = connect_s3()
qst_hash_dict = load_manifest()

replay_guid_to_path = {}
for path in replays_dir.rglob('*.zplay'):
    replay_guid_to_path[path.stem] = path


@app.route('/api/v1/quests', methods=['GET'])
def quests():
    return [{'hash': key, 'TEST': 1} for key in qst_hash_dict.keys()]


@app.route('/api/v1/replays/<guid>/length')
def check(guid):
    length = -1
    path = replay_guid_to_path[guid]
    if path.exists():
        meta = parse_meta(path.read_text('utf-8'))
        length = int(meta['length'])
    return {
        'length': length,
    }


@app.route('/api/v1/replays', methods=['PUT'])
def replays():
    # No larger than 30 MB.
    if len(request.data) > 30e6:
        return {'error': 'too large, max is 30 MB'}, HTTPStatus.REQUEST_ENTITY_TOO_LARGE

    data = request.data.decode('utf-8')
    if not data.startswith('M'):
        return {'error': 'invalid replay'}, HTTPStatus.BAD_REQUEST

    meta, steps = parse_replay(data)
    qst_hash = meta.get('qst_hash')
    guid = meta.get('guid')

    if not qst_hash or not guid:
        return {'error': 'replay is too old'}, HTTPStatus.BAD_REQUEST

    if not qst_hash.isalnum() or not guid.isalnum():
        return {'error': 'invalid qst_hash or guid'}, HTTPStatus.BAD_REQUEST

    if int(meta.get('length')) != len(steps):
        return {'error': 'invalid length'}, HTTPStatus.BAD_REQUEST

    if qst_hash not in qst_hash_dict:
        return {'error': 'unknown qst'}, HTTPStatus.BAD_REQUEST

    key = f'{qst_hash}/{guid}.zplay'
    path = replays_dir / key
    status = HTTPStatus.CREATED
    if path.exists():
        status = HTTPStatus.OK
        existing_meta, existing_steps = parse_replay(path.read_text('utf-8'))
        if int(existing_meta['length']) > int(meta['length']):
            return {'error': 'replay is older than current'}, HTTPStatus.CONFLICT
        # Replays are append-only.
        for i, existing_step in enumerate(existing_steps):
            if existing_step != steps[i]:
                return {'error': 'replay is different than current'}, HTTPStatus.CONFLICT

    path.parent.mkdir(exist_ok=True)
    path.write_text(data)
    s3.upload_file(path, bucket_name, key)

    return {'key': key}, status


@app.errorhandler(HTTPException)
def handle_exception(e):
    """Return JSON instead of HTML for HTTP errors."""
    response = e.get_response()
    response.data = json.dumps({
        "code": e.code,
        "name": e.name,
        "description": e.description,
    })
    response.content_type = "application/json"
    return response
