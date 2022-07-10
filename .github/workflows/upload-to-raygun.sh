#!/usr/bin/env bash

set -euxo pipefail

for pdb in *.pdb; do
  echo $pdb
  curl \
    -H "Host: app.raygun.com" \
    -F "SymbolFile=@$pdb" \
    https://app.raygun.com/upload/breakpadsymbols/2ab7tyz?authToken=$RAYGUN_TOKEN
done
