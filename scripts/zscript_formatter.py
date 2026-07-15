"""
ZScript Formatter
=================

Formats the doc comments in resources/include/bindings/*.zh.

Usage:
------
    # Check for formatting issues (exits 1 and lists them)
    python3 scripts/zscript_formatter.py

    # Automatically fix issues
    python3 scripts/zscript_formatter.py --fix

    # Limit to specific files
    python3 scripts/zscript_formatter.py --fix resources/include/bindings/bitmap.zh
"""

import argparse
import os
import re
import sys

from pathlib import Path

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
bindings_dir = root_dir / 'resources/include/bindings'

COMMENT_RE = re.compile(r'^(\s*)//(.*)$')
TAG_RE = re.compile(r'^\s*@([a-zA-Z_][a-zA-Z0-9_]*)')
ENUM_START_RE = re.compile(r'^\s*(?:export\s+)?enum\b')
LIST_ITEM_RE = re.compile(r'\s*(?:[-*+]|\d+[.)])\s')

# Prose in comments is wrapped to this column (with tabs counted as 4).
COLUMN_LIMIT = 100

# Anything else is reported as a typo. If a new tag is introduced, add it here.
KNOWN_TAGS = {
    'alias',
    'default',
    'delete',
    'deprecated',
    'deprecated_alias',
    'deprecated_future',
    'deprecated_getter',
    'exit',
    'extends',
    'index',
    'length',
    'param',
    'reassign_ptr',
    'see',
    'soft_deprecated',
    'tutorial',
    'value',
    'versionadded',
    'versionchanged',
    'zasm',
    'zasm_ref',
    'zasm_var',
}

# Tags that may appear at most once per doc comment. Keep in sync with
# SINGLE_VALUE_TAGS in zscript_doc_parser.py, which enforces the same thing
# much later in the pipeline.
SINGLE_USE_TAGS = {
    'delete',
    'deprecated',
    'deprecated_future',
    'deprecated_getter',
    'exit',
    'extends',
    'index',
    'length',
    'reassign_ptr',
    'soft_deprecated',
    'value',
    'versionadded',
    'zasm',
    'zasm_ref',
    'zasm_var',
}

# Tags listed here sort first, in this order. Everything else sorts
# alphabetically after them.
TAG_PRIORITY = [
    'alias',
    'deprecated_alias',
    'deprecated',
    'deprecated_future',
    'deprecated_getter',
    'soft_deprecated',
    'param',
    'length',
    'index',
    'value',
]


def is_comment_line(line):
    return line.lstrip().startswith('//')


def comment_content(line):
    return COMMENT_RE.match(line).group(2)


def is_empty_comment_line(line):
    return is_comment_line(line) and not comment_content(line).strip()


def get_tag_name(line):
    if not is_comment_line(line):
        return None
    m = TAG_RE.match(comment_content(line))
    return m.group(1) if m else None


def tag_sort_key(name):
    if name in TAG_PRIORITY:
        return (0, TAG_PRIORITY.index(name), '')
    return (1, 0, name)


def find_comment_blocks(lines):
    """Yields (start, end) index pairs of runs of whole-line comments."""
    blocks = []
    start = None
    for i, line in enumerate(lines):
        if is_comment_line(line):
            if start is None:
                start = i
        elif start is not None:
            blocks.append((start, i))
            start = None
    if start is not None:
        blocks.append((start, len(lines)))
    return blocks


class Formatter:
    def __init__(self, path, text):
        self.path = path
        self.issues = []
        self.lines = text.split('\n')

    def issue(self, line_index, message, fixable=True):
        entry = (line_index + 1, message, fixable)
        if entry not in self.issues:
            self.issues.append(entry)

    def format(self):
        # One rule's fix can trip another (e.g. adding terminal punctuation
        # can push a line over COLUMN_LIMIT), so run the passes to a fixpoint.
        for _ in range(10):
            before = list(self.lines)
            self.strip_trailing_whitespace()
            self.add_missing_comment_spaces()
            self.collapse_blank_lines()
            self.merge_split_doc_comments()
            for start, end in reversed(find_comment_blocks(self.lines)):
                self.format_block(start, end)
            self.align_enum_comments()
            if self.lines == before:
                break
        return '\n'.join(self.lines)

    def strip_trailing_whitespace(self):
        for i, line in enumerate(self.lines):
            stripped = line.rstrip()
            if stripped != line:
                self.issue(i, 'trailing whitespace')
                self.lines[i] = stripped

    def add_missing_comment_spaces(self):
        for i, line in enumerate(self.lines):
            if not is_comment_line(line):
                continue
            indent, content = COMMENT_RE.match(line).groups()
            if content and not content[0].isspace():
                self.issue(i, 'missing space after //')
                self.lines[i] = f'{indent}// {content}'

    def collapse_blank_lines(self):
        collapsed = []
        for i, line in enumerate(self.lines):
            if not line and collapsed and not collapsed[-1]:
                self.issue(i, 'consecutive blank lines')
                continue
            collapsed.append(line)
        self.lines = collapsed

    def merge_split_doc_comments(self):
        # A doc comment accidentally split by a blank line, where the part
        # after the blank line is more tags, should be one doc comment.
        while True:
            blocks = find_comment_blocks(self.lines)
            merged = False
            for (start_a, end_a), (start_b, _) in zip(blocks, blocks[1:]):
                gap = self.lines[end_a:start_b]
                if not gap or any(l.strip() for l in gap):
                    continue
                block_a_has_tag = any(
                    get_tag_name(l) for l in self.lines[start_a:end_a]
                )
                if block_a_has_tag and get_tag_name(self.lines[start_b]):
                    self.issue(end_a, 'blank line splits a doc comment')
                    del self.lines[end_a:start_b]
                    merged = True
                    break
            if not merged:
                return

    def reflow_description(self, indent, desc, start):
        # Rewraps prose paragraphs containing a line over COLUMN_LIMIT. Code
        # fences, preformatted/indented lines, and tables are kept verbatim.
        # A list item starts its own paragraph and wraps without a hanging
        # indent, continuing the item (markdown's "lazy continuation").
        prefix = f'{indent}// '

        def fits(line):
            return len(line.expandtabs(4)) <= COLUMN_LIMIT

        def fill(words, first_prefix, cont_prefix):
            # Keep [bracketed] and `quoted` spans together when they contain
            # spaces.
            tokens = []
            for word in words:
                prev = tokens[-1] if tokens else ''
                if prev.count('[') > prev.count(']') or prev.count('`') % 2:
                    tokens[-1] += f' {word}'
                else:
                    tokens.append(word)
            filled = []
            current = ''
            current_prefix = first_prefix
            for token in tokens:
                candidate = f'{current} {token}' if current else token
                if current and not fits(current_prefix + candidate):
                    filled.append(current_prefix + current)
                    current_prefix = cont_prefix
                    current = token
                else:
                    current = candidate
            if current:
                filled.append(current_prefix + current)
            return filled

        out = []
        paragraph = []

        def flush():
            if not paragraph:
                return
            lines = [line for _, line in paragraph]
            first_content = comment_content(lines[0])
            # Keep the indentation of the paragraph's first line (e.g. a
            # nested list item).
            extra_indent = re.match(r' ?( *)', first_content).group(1)
            first_prefix = prefix + extra_indent
            # A wrapped list item's continuation lines align with the text
            # after the item's marker.
            marker = re.match(r' ?\s*((?:[-*+]|\d+[.)])\s+)', first_content)
            cont_prefix = first_prefix + ' ' * len(marker.group(1) if marker else '')

            message = None
            if not all(fits(line) for line in lines):
                message = f'prose exceeds {COLUMN_LIMIT} columns'
            elif any(
                len(c) - len(c.lstrip()) != len(cont_prefix) - len(f'{indent}//')
                for c in map(comment_content, lines[1:])
            ):
                message = (
                    'list item continuation not aligned with item text'
                    if marker
                    else 'paragraph continuation has stray indentation'
                )
            if message:
                words = ' '.join(
                    comment_content(line).strip() for line in lines
                ).split()
                filled = fill(words, first_prefix, cont_prefix)
                # Unbreakable content (e.g. a long URL) may not be fixable.
                if filled != lines:
                    self.issue(paragraph[0][0], message)
                    lines = filled
            out.extend(lines)
            paragraph.clear()

        in_fence = False
        for k, line in enumerate(desc):
            content = comment_content(line)
            stripped = content.strip()
            if stripped.startswith('```'):
                flush()
                in_fence = not in_fence
                out.append(line)
                continue
            if in_fence or not stripped or stripped[0] in '|>#':
                flush()
                out.append(line)
                continue
            if LIST_ITEM_RE.match(content):
                flush()
            elif re.match(r'\s\s', content) and not paragraph:
                # Indented content at the start of a paragraph is preformatted;
                # mid-paragraph it is a hanging continuation and joins.
                out.append(line)
                continue
            paragraph.append((start + k, line))
        flush()
        return out

    def add_terminal_punctuation(self, desc, start):
        # The last prose line of a description should end a sentence. Only
        # appends a period after a letter or digit - anything else (a code
        # span, link, quote, parenthetical, ...) is left alone.
        last = None
        for k, line in enumerate(desc):
            if comment_content(line).strip():
                last = k
        if last is None:
            return
        content = comment_content(desc[last])
        stripped = content.strip()
        in_fence = False
        for line in desc[:last]:
            if comment_content(line).strip().startswith('```'):
                in_fence = not in_fence
        if (
            not in_fence
            and not stripped.startswith('```')
            and stripped[0] not in '|>#'
            and not re.match(r'\s\s', content)
            and stripped[-1].isalnum()
            # A period would corrupt a trailing URL.
            and not re.match(r'https?://', stripped.split()[-1])
        ):
            self.issue(start + last, 'description missing terminal punctuation')
            desc[last] += '.'

    def format_block(self, start, end):
        block = self.lines[start:end]
        first_tag = next((i for i, l in enumerate(block) if get_tag_name(l)), None)
        if first_tag is None:
            indent = COMMENT_RE.match(block[0]).group(1)
            formatted = self.reflow_description(indent, block, start)
            if formatted != block:
                self.lines[start:end] = formatted
            return

        indent = COMMENT_RE.match(block[first_tag]).group(1)

        desc = block[:first_tag]
        num_trailing_empty = 0
        while desc and is_empty_comment_line(desc[-1]):
            desc.pop()
            num_trailing_empty += 1
        desc = self.reflow_description(indent, desc, start)
        self.add_terminal_punctuation(desc, start)

        # Group each tag line with its continuation lines, dropping empty
        # comment lines within the tag section.
        tags = []
        for i in range(first_tag, len(block)):
            line = block[i]
            name = get_tag_name(line)
            if name:
                if name not in KNOWN_TAGS:
                    self.issue(start + i, f'unknown tag @{name}', fixable=False)
                tags.append((name, [line]))
            elif is_empty_comment_line(line):
                self.issue(start + i, 'empty comment line within tags')
            else:
                tags[-1][1].append(line)

        names = [name for name, _ in tags]
        for name in sorted(set(names)):
            if names.count(name) > 1 and name in SINGLE_USE_TAGS:
                self.issue(
                    start + first_tag, f'duplicate tag @{name}', fixable=False
                )

        if desc:
            if num_trailing_empty == 0:
                self.issue(
                    start + first_tag,
                    'missing empty comment line between description and tags',
                )
            elif num_trailing_empty > 1:
                self.issue(
                    start + first_tag,
                    'extra empty comment lines between description and tags',
                )

        sorted_tags = sorted(tags, key=lambda t: tag_sort_key(t[0]))
        if [t[0] for t in sorted_tags] != [t[0] for t in tags]:
            self.issue(
                start + first_tag,
                'tags out of order: '
                + ', '.join(f'@{t[0]}' for t in tags)
                + ' should be '
                + ', '.join(f'@{t[0]}' for t in sorted_tags),
            )

        formatted = list(desc)
        if desc:
            formatted.append(f'{indent}//')
        for _, tag_lines in sorted_tags:
            formatted.extend(tag_lines)

        if formatted != block:
            self.lines[start:end] = formatted

    def align_enum_comments(self):
        i = 0
        while i < len(self.lines):
            line = self.lines[i]
            if is_comment_line(line) or not ENUM_START_RE.match(line):
                i += 1
                continue
            # Find the enum's opening brace, then its closing brace.
            open_index = next(
                (
                    j
                    for j in range(i, min(i + 3, len(self.lines)))
                    if '{' in self.lines[j]
                ),
                None,
            )
            if open_index is None:
                i += 1
                continue
            end = open_index + 1
            while end < len(self.lines) and '}' not in self.lines[end]:
                end += 1
            self.align_enum_body(open_index + 1, end)
            i = end + 1

    def align_enum_body(self, start, end):
        # Group the body's trailing comments (and their whole-line continuation
        # comments) into runs, broken by blank lines and doc comments, and give
        # every comment in a run the same column.
        def width(s):
            return len(s.expandtabs(4))

        member_indent = ''
        runs = [[]]
        prev_had_comment = False
        for i in range(start, end):
            line = self.lines[i]
            if not line.strip():
                runs.append([])
                prev_had_comment = False
                continue
            if is_comment_line(line):
                indent = COMMENT_RE.match(line).group(1)
                if prev_had_comment and width(indent) > width(member_indent):
                    # A continuation of the previous trailing comment.
                    runs[-1].append((i, None, line[len(indent) :], width(indent)))
                else:
                    # A doc comment.
                    runs.append([])
                    prev_had_comment = False
                continue
            if not member_indent:
                member_indent = line[: len(line) - len(line.lstrip())]
            comment_index = line.find('//')
            if comment_index == -1:
                prev_had_comment = False
                continue
            code = line[:comment_index].rstrip()
            runs[-1].append((i, code, line[comment_index:], width(line[:comment_index])))
            prev_had_comment = True

        for run in runs:
            if len(run) < 2:
                continue
            gaps = [col - width(code) for _, code, _, col in run if code is not None]
            if all(gap == 1 for gap in gaps):
                # Compact style: every comment directly follows its code.
                continue
            columns = [col for _, _, _, col in run]
            mode = max(set(columns), key=lambda col: (columns.count(col), col))
            has_continuation = any(code is None for _, code, _, _ in run)
            if columns.count(mode) == 1 and not has_continuation:
                # No two comments agree on a column; use the compact style.
                for i, code, comment, col in run:
                    if col - width(code) != 1:
                        self.issue(i, 'enum trailing comment not aligned')
                        self.lines[i] = f'{code} {comment}'
                continue
            # Aligned style: comments share a column, except comments on
            # members too long for that column, which directly follow the code.
            for i, code, comment, col in run:
                if code is None:
                    target = mode
                elif width(code) < mode:
                    target = mode
                else:
                    target = width(code) + 1
                if col == target:
                    continue
                self.issue(i, 'enum trailing comment not aligned')
                prefix = code if code is not None else member_indent
                self.lines[i] = prefix + ' ' * (target - width(prefix)) + comment


def main():
    arg_parser = argparse.ArgumentParser(
        description='Format doc comments in ZScript binding headers'
    )
    arg_parser.add_argument(
        '--fix',
        '--update',
        action='store_true',
        dest='fix',
        help='Auto-fix issues instead of reporting them',
    )
    arg_parser.add_argument(
        '--stdin',
        action='store_true',
        help='Read source from stdin and write the formatted source to stdout',
    )
    arg_parser.add_argument(
        'files',
        nargs='*',
        type=Path,
        help='Files to format (default: all of resources/include/bindings)',
    )
    args = arg_parser.parse_args()

    if args.stdin:
        text = sys.stdin.read()
        crlf = '\r\n' in text
        if crlf:
            text = text.replace('\r\n', '\n')
        formatted = Formatter(Path('<stdin>'), text).format()
        if crlf:
            formatted = formatted.replace('\n', '\r\n')
        sys.stdout.write(formatted)
        return

    files = args.files or sorted(bindings_dir.glob('*.zh'))

    any_fixable = False
    any_unfixable = False
    for path in files:
        text = path.read_text()
        formatter = Formatter(path, text)
        formatted = formatter.format()
        if not formatter.issues:
            continue

        rel_path = (
            path.relative_to(root_dir) if path.is_relative_to(root_dir) else path
        )
        fixable = [issue for issue in formatter.issues if issue[2]]
        unfixable = [issue for issue in formatter.issues if not issue[2]]
        any_fixable = any_fixable or bool(fixable)
        any_unfixable = any_unfixable or bool(unfixable)

        if args.fix and fixable:
            path.write_text(formatted)
            print(f'fixed {len(fixable)} issues in {path.name}')

        reportable = unfixable if args.fix else formatter.issues
        for line, message, _ in sorted(reportable):
            print(f'{rel_path}:{line}: {message}')

    if any_unfixable or (any_fixable and not args.fix):
        if not args.fix and any_fixable:
            print('\nRun `python3 scripts/zscript_formatter.py --fix` to fix.')
        sys.exit(1)


if __name__ == '__main__':
    main()
