#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Usage:
#   convert.py input_filename
# input_filename is a file of Wikipedia article titles, one title per line.
# Outputs one word per line (simplified Chinese, filtered).

import logging
import regex
import sys

import opencc

# Require at least 2 characters
_MINIMUM_LEN = 2
_LIST_PAGE_ENDINGS = [
    '列表',
    '对照表',
]
_LOG_EVERY = 1000

# https://ayaka.shn.hk/hanregex/
# INTERPUNCT   \u00b7 -> ·
# HYPHEN-MINUS \u002d -> -
# HYPHEN       \u2010 -> ‐
# EN DASH      \u2013 -> –
# EM DASH      \u2014 -> —
_HANZI_RE = regex.compile(r"([\p{Unified_Ideograph}\u3006\u3007\u00b7\u002d\u2010\u2013\u2014][\ufe00-\ufe0f\U000e0100-\U000e01ef]?)+")
_TO_SIMPLIFIED_CHINESE = opencc.OpenCC('t2s')

logging.basicConfig(level=logging.INFO)


def is_good_title(title, previous_title=None):
    if not _HANZI_RE.fullmatch(title):
        return False

    # Skip single character & too long pages
    if len(title) < _MINIMUM_LEN:
        return False

    # Skip list pages
    if title.endswith(tuple(_LIST_PAGE_ENDINGS)):
        return False

    if previous_title and \
      len(previous_title) >= 4 and \
      title.startswith(previous_title):
        return False

    return True


def log_count(count):
    logging.info(f'{count} words generated')


def main():
    previous_title = None
    result_count = 0
    with open(sys.argv[1]) as f:
        for line in f:
            title = _TO_SIMPLIFIED_CHINESE.convert(line.strip())
            if is_good_title(title, previous_title):
                print(title)
                result_count += 1
                if result_count % _LOG_EVERY == 0:
                    log_count(result_count)
                previous_title = title
    log_count(result_count)


if __name__ == '__main__':
    main()
