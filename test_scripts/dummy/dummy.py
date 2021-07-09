#!/usr/bin/env python3
# -*- mode: python3; tab-width: 4; indent-tabs-mode: nil; -*-
# ------------------------------------------------------------------------------
# Copyright (c) 2021 Marcus Geelnard
#
# This software is provided 'as-is', without any express or implied warranty. In
# no event will the authors be held liable for any damages arising from the use
# of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it freely,
# subject to the following restrictions:
#
#  1. The origin of this software must not be misrepresented; you must not claim
#     that you wrote the original software. If you use this software in a
#     product, an acknowledgment in the product documentation would be
#     appreciated but is not required.
#
#  2. Altered source versions must be plainly marked as such, and must not be
#     misrepresented as being the original software.
#
#  3. This notice may not be removed or altered from any source distribution.
# ------------------------------------------------------------------------------

import argparse
import codecs
import sys


def dummy(source, output):
    # Read the data from the source file.
    with open(source, "r", encoding="utf8") as f:
        data = f.read()

    # Scramble the data (just to do some work).
    data = codecs.encode(data, 'rot_13')

    # Write the data to the output file.
    with open(output, "w", encoding="utf8") as f:
        f.write(data)


def main():
    parser = argparse.ArgumentParser(description="Dummy compiler.")
    parser.add_argument("-o", "--output", required=True, help="output file")
    parser.add_argument("source", help="source file")
    args = parser.parse_args()

    try:
        dummy(args.source, args.output)
        return 0
    except:
        print(f"Error processing {args.source}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
