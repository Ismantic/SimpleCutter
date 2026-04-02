#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Extract redirect mappings from zhwiki SQL dumps.
#
# Usage:
#   ./extract-redirects.py zhwiki-YYYYMMDD-page.sql.gz zhwiki-YYYYMMDD-redirect.sql.gz
#
# Outputs "source_title\ttarget_title" per line (namespace 0 only).

import gzip
import sys

import opencc


def parse_sql_values(line):
    """Yield tuples from a MySQL INSERT INTO ... VALUES (...),(...); line."""
    # Find the start of VALUES
    idx = line.find(b"VALUES ")
    if idx == -1:
        return
    data = line[idx + 7:]

    # State machine to parse (val,val,...),(val,val,...) handling quoted strings
    i = 0
    n = len(data)
    while i < n:
        if data[i:i+1] != b'(':
            i += 1
            continue
        i += 1  # skip '('
        fields = []
        while i < n:
            if data[i:i+1] == b"'":
                # Quoted string
                i += 1
                parts = []
                while i < n:
                    if data[i:i+1] == b'\\':
                        parts.append(data[i:i+2])
                        i += 2
                    elif data[i:i+1] == b"'":
                        i += 1
                        break
                    else:
                        parts.append(data[i:i+1])
                        i += 1
                fields.append(b''.join(parts))
            else:
                # Unquoted value (number or NULL)
                j = i
                while i < n and data[i:i+1] not in (b',', b')'):
                    i += 1
                fields.append(data[j:i])
            if i < n and data[i:i+1] == b',':
                i += 1
            elif i < n and data[i:i+1] == b')':
                i += 1
                break
        yield fields


def _unescape_title(raw):
    """Unescape a MySQL-dumped title bytes to a UTF-8 string."""
    title = raw.replace(b'_', b' ')
    title = title.replace(b"\\'", b"'")
    title = title.replace(b"\\\\", b"\\")
    return title.decode('utf-8')


def load_redirects(redirect_sql_gz):
    """Load redirect mappings: rd_from -> rd_title (namespace 0 only)."""
    rd_map = {}  # rd_from -> target_title
    with gzip.open(redirect_sql_gz, 'rb') as f:
        for line in f:
            if not line.startswith(b'INSERT'):
                continue
            for fields in parse_sql_values(line):
                # rd_from, rd_namespace, rd_title, rd_interwiki, rd_fragment
                if len(fields) >= 3:
                    rd_from = int(fields[0])
                    rd_namespace = int(fields[1])
                    if rd_namespace == 0:
                        try:
                            rd_map[rd_from] = _unescape_title(fields[2])
                        except UnicodeDecodeError:
                            pass
    return rd_map


def extract_mappings(page_sql_gz, rd_map):
    """Extract (source_title, target_title) from page table for redirect pages."""
    mappings = []
    with gzip.open(page_sql_gz, 'rb') as f:
        for line in f:
            if not line.startswith(b'INSERT'):
                continue
            for fields in parse_sql_values(line):
                # page_id, page_namespace, page_title, ...
                if len(fields) >= 5:
                    page_id = int(fields[0])
                    page_namespace = int(fields[1])
                    if page_namespace == 0 and page_id in rd_map:
                        try:
                            source = _unescape_title(fields[2])
                            mappings.append((source, rd_map[page_id]))
                        except UnicodeDecodeError:
                            pass
    return mappings


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <page.sql.gz> <redirect.sql.gz>", file=sys.stderr)
        sys.exit(1)

    page_sql_gz = sys.argv[1]
    redirect_sql_gz = sys.argv[2]

    converter = opencc.OpenCC('t2s')

    print("Loading redirect table...", file=sys.stderr)
    rd_map = load_redirects(redirect_sql_gz)
    print(f"  {len(rd_map)} redirects targeting namespace 0", file=sys.stderr)

    print("Extracting mappings from page table...", file=sys.stderr)
    mappings = extract_mappings(page_sql_gz, rd_map)
    print(f"  {len(mappings)} redirect mappings extracted", file=sys.stderr)

    for source, target in sorted(mappings):
        source = converter.convert(source)
        target = converter.convert(target)
        print(f"{source}\t{target}")


if __name__ == '__main__':
    main()
