# Same as run_python.py but no sudo for mac.

#!/usr/bin/env bash

set -euxo pipefail

case "$(uname -sr)" in
   Darwin*)
        ulimit -c unlimited
        python -Xutf8 "$@"
        ;;

   Linux*)
        ulimit -c unlimited
        sudo -E xvfb-run --auto-servernum python -Xutf8 "$@"
        ;;

   CYGWIN*|MINGW*|MINGW32*|MSYS*)
        python -Xutf8 "$@"
        ;;

   *)
      exit 1
esac
