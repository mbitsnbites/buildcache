#!/usr/bin/env python3
# -*- mode: python3; tab-width: 4; indent-tabs-mode: nil; -*-
# ------------------------------------------------------------------------------
# Copyright (c) 2020 Marcus Geelnard
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
import difflib
import subprocess
import sys
from multiprocessing.pool import ThreadPool
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_SRC_DIR = _SCRIPT_DIR.parent / "src"

_EXCLUDE_DIRS = ["third_party"]


def collect_sources(file_ext):
    # Find all files in the src folder.
    files = []
    files.extend(_SRC_DIR.glob(f"*{file_ext}"))

    # Find all files in non-excluded sub-folders.
    dirs = _SRC_DIR.glob("*")
    dirs = [d for d in dirs if d.is_dir() and d.name not in _EXCLUDE_DIRS]
    for dir in dirs:
        files.extend(dir.glob(f"**/*{file_ext}"))

    return files


def clang_tidy(src, build_path, fix):
    file_name = src.relative_to(_SRC_DIR)
    print(f"clang-tidy: {file_name}")
    args = ["clang-tidy", "-p", build_path]
    if fix:
        args.append("--fix")
    args.append(src)
    proc = subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8"
    )
    return proc.stdout


def read_file(file_name):
    with open(file_name, encoding="utf8") as f:
        return f.readlines()


def files_equal(a, b):
    if len(a) != len(b):
        return False
    for i in range(0, len(a)):
        if a[i] != b[i]:
            return False
    return True


def clang_format(src, fix):
    file_name = src.relative_to(_SRC_DIR)
    print(f"clang-format: {file_name}")
    args = ["clang-format", "-Werror"]
    if fix:
        args.append("-i")
    else:
        args.append("--dry-run")
    args.append(src)
    before = read_file(src)
    proc = subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8"
    )
    after = read_file(src)
    result = proc.stderr
    if not files_equal(after, before):
        if result:
            result += "\n"
        result += f"{src}: warning: code was clang-formatted:\n"
        result += "".join(
            difflib.context_diff(
                before, after, fromfile=str(file_name), tofile=str(file_name), n=0
            )
        )
    return result


def run_clang_tidy(build_path, fix):
    sources = collect_sources(".cpp")

    # Run jobs in a thread pool.
    pool = ThreadPool()
    results = []
    for src in sources:
        results.append(pool.apply_async(clang_tidy, [src, build_path, fix]))

    # Wait for all workers to finish.
    pool.close()
    pool.join()

    return [r.get() for r in results if r.get()]


def run_clang_format(fix):
    sources = collect_sources(".cpp")
    sources.extend(collect_sources(".hpp"))

    # Run jobs in a thread pool.
    pool = ThreadPool()
    results = []
    for src in sources:
        results.append(pool.apply_async(clang_format, [src, fix]))

    # Wait for all workers to finish.
    pool.close()
    pool.join()

    return [r.get() for r in results if r.get()]


def run_linters(build_path, fix):
    errors = []
    errors.extend(run_clang_tidy(build_path, fix))
    errors.extend(run_clang_format(fix))

    all_ok = True
    if errors:
        all_ok = False
        print(f"\n***ERROR: Some checks failed 😣")
        for err in errors:
            print(err)
    else:
        print("\nAll checks passed! 👍")

    return all_ok


def main():
    parser = argparse.ArgumentParser(description="Run linter checks.")
    parser.add_argument("-p", "--build-path", required=True, help="build path")
    parser.add_argument("--fix", action="store_true", help="try to fix problems")
    args = parser.parse_args()

    if not run_linters(args.build_path, args.fix):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
