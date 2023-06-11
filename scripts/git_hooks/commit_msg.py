import sys
import re
from pathlib import Path
from common import valid_types, valid_scopes

commit_msg_path = Path(sys.argv[1])
commit_msg = commit_msg_path.read_text()
is_valid = True

# Prefix is required, scope is optional.
first_line = commit_msg.splitlines()[0]
match = re.match(r'(\w+)\((\w+)\): (.+)',
                 first_line) or re.match(r'(\w+):( )(.+)', first_line)
if not match:
    print('commit message must match expected pattern, using an expected type and optional scope.\nexamples:\n\tfix: fix the thing\n\tfix(zc): fix the thing\n')
    print(f'valid types (the first word of the commit) are:', ', '.join(valid_types))
    print(f'valid scopes (the optional text in parentheses) are:',
          ', '.join(valid_scopes))
    print()
    is_valid = False
else:
    type, scope, oneliner = match.groups()
    scope = scope.strip()

    is_valid = True
    if type not in valid_types:
        print(
            f'invalid type: {type}\nMust be one of: {", ".join(valid_types)}')
        is_valid = False
    if scope and scope not in valid_scopes:
        print(
            f'invalid scope: {scope}\nMust be one of: {", ".join(valid_scopes)}')
        is_valid = False

if not is_valid:
    print('============== FAILED TO COMMIT ==================')
    print('commit message is not valid, please rewrite it')
    print('to skip this validation, commit again with the --no-verify flag')
    print('==================================================\n')
    print(f'for reference, this was the commit message:\n\n{commit_msg}')
    exit(1)
