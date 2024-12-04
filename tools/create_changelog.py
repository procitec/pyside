# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
from __future__ import annotations

import re
import os
import sys
import textwrap
from argparse import ArgumentParser, Namespace, RawTextHelpFormatter
from pathlib import Path
from subprocess import PIPE, Popen, check_output

content_header = """Qt for Python @VERSION is a @TYPE release.

For more details, refer to the online documentation included in this
distribution. The documentation is also available online:

https://doc.qt.io/qtforpython/

Some of the changes listed in this file include issue tracking numbers
corresponding to tasks in the Qt Bug Tracker:

https://bugreports.qt.io/

Each of these identifiers can be entered in the bug tracker to obtain more
information about a particular change.

****************************************************************************
*                                  PySide6                                 *
****************************************************************************
"""

shiboken_header = """****************************************************************************
*                                  Shiboken6                               *
****************************************************************************
"""

description = """
PySide6 changelog tool

Example usage:
tools/create_changelog.py -v -r 6.5.3
"""


def change_log(version: list) -> Path:
    """Return path of the changelog of the version."""
    name = f"changes-{version[0]}.{version[1]}.{version[2]}"
    return Path(__file__).parents[1] / "doc" / "changelogs" / name


def is_lts_version(version: list) -> bool:
    return version[0] == 5 or version[1] in (2, 5)


def version_tag(version: list) -> str:
    """Return the version tag."""
    tag = f"v{version[0]}.{version[1]}.{version[2]}"
    return tag + "-lts" if is_lts_version(version) else tag


def revision_range(version: list) -> str:
    """Determine a git revision_range from the version. Either log from
       the previous version tag or since the last update to the changelog."""
    changelog = change_log(version)
    if changelog.is_file():
        output = check_output(["git", "log", "-n", "1", "--format=%H",
                               os.fspath(changelog)])
        if output:
            return output.strip().decode("utf-8") + "..HEAD"

    last_version = version.copy()
    if version[2] == 0:
        adjust_idx = 0 if version[1] == 0 else 1
    else:
        adjust_idx = 2
    last_version[adjust_idx] -= 1
    return version_tag(last_version) + "..HEAD"


def parse_options() -> Namespace:
    tag_msg = ("Tags, branches, or SHA to compare\n"
               "e.g.: v5.12.1..5.12\n"
               "      v5.12.0..v5.12.1\n"
               "      cebc32a5..5.12")

    options = ArgumentParser(description=description,
                             formatter_class=RawTextHelpFormatter)
    options.add_argument("-d",
                         "--directory",
                         type=str,
                         help="Repository directory, '.' is default.")
    options.add_argument("-v",
                         "--versions",
                         type=str,
                         help=tag_msg)
    options.add_argument("-r",
                         "--release",
                         type=str,
                         help="Release version: e.g.: 5.12.4",
                         required=True)
    options.add_argument("-t",
                         "--type",
                         type=str,
                         help="Release type: bug-fix, minor, or major")

    options.add_argument("-e",
                         "--exclude",
                         action="store_true",
                         help="Exclude commits with a 'Pick-to' line",
                         default=False)

    args = options.parse_args()

    release_version = list(int(v) for v in args.release.split("."))
    if len(release_version) != 3:
        print("Error: --release must be of form major.minor.patch")
        sys.exit(-1)

    # Some auto-detection smartness
    if not args.type:
        if release_version[2] == 0:
            args.type = "major" if release_version[1] == 0 else "minor"
        else:
            args.type = "bug-fix"
        # For major/minor releases, skip all fixes with "Pick-to: " since they
        # appear in bug-fix releases.
        if args.type != "bug-fix":
            args.exclude = True
        print(f'Assuming "{args.type}" version', file=sys.stderr)

    if args.type not in ("bug-fix", "minor", "major"):
        print("Error: "
              "-y/--type needs to be: bug-fix (default), minor, or major")
        sys.exit(-1)

    if not args.versions:
        args.versions = revision_range(release_version)
        print(f"Assuming range {args.versions}", file=sys.stderr)

    args.release_version = release_version
    return args


def format_text(text: str) -> str:
    """Format an entry with a leading dash, 80 columns"""
    return textwrap.fill(text, width=77, initial_indent=" - ",
                         subsequent_indent="   ")


def check_tag(tag: str) -> bool:
    output = False

    if tag[0] == "v":
        # Git tag
        command = "git tag -l {}".format(tag)
        print("{}: {}".format(check_tag.__name__, command), file=sys.stderr)
        if check_output(command.split()):
            output = True
    elif re.match(r"^\d\.\d?", tag):
        # Git branch (origin)
        command = "git show-ref {}".format(tag)
        print("{}: {}".format(check_tag.__name__, command), file=sys.stderr)
        if check_output(command.split()):
            output = True
    else:
        # Git sha
        command = "git cat-file -t {}".format(tag)
        print("{}: {}".format(check_tag.__name__, command), file=sys.stderr)
        if check_output(command.split()):
            output = True

    return output


def get_commit_content(sha: str) -> str:
    command = "git log {} -n 1 --pretty=format:%s%n%n%b".format(sha)
    print("{}: {}".format(get_commit_content.__name__, command),
          file=sys.stderr)
    out, err = Popen(command, stdout=PIPE, shell=True).communicate()
    if err:
        print(err, file=sys.stderr)
    return out.decode("utf-8")


def git_get_sha1s(versions: list[str], pattern: str):
    """Return a list of SHA1s matching a pattern"""
    command = "git rev-list --reverse --grep '^{}'".format(pattern)
    command += " {}..{}".format(versions[0], versions[1])
    command += " | git cat-file --batch"
    command += " | grep -o -E \"^[0-9a-f]{40} commit\""
    command += " | awk '{print $1}'"
    print("{}: {}".format(git_command.__name__, command), file=sys.stderr)
    out_sha1, err = Popen(command, stdout=PIPE, shell=True).communicate()
    if err:
        print(err, file=sys.stderr)

    pick_to_sha1 = []

    if exclude_pick_to:
        # if '-e', we exclude all the 'Pick-to' changes
        command = "git rev-list --reverse --grep '^Pick-to:'"
        command += " {}..{}".format(versions[0], versions[1])
        command += " | git cat-file --batch"
        command += " | grep -o -E \"^[0-9a-f]{40} commit\""
        command += " | awk '{print $1}'"
        print("{}: {}".format(git_command.__name__, command), file=sys.stderr)
        out_e_sha1, err = Popen(command, stdout=PIPE, shell=True).communicate()
        if err:
            print(err, file=sys.stderr)
        pick_to_sha1 = out_e_sha1.splitlines()

    return [s.decode("utf-8") for s in out_sha1.splitlines() if s not in pick_to_sha1]


def git_command(versions: list[str], pattern: str):
    task_number_re = re.compile(r'^.*-(\d+)\s*$')
    for sha in git_get_sha1s(versions, pattern):
        content = get_commit_content(sha).splitlines()
        # First line is title
        title = content[0]
        # Look for PYSIDE-XXXX
        task = None
        for line in content[::-1]:
            if line.startswith(pattern):
                task = line.replace("{}:".format(pattern), "").strip()
                break

        if not task:
            continue
        task_number = -1
        task_number_match = task_number_re.match(task)
        if task_number_match:
            task_number = int(task_number_match.group(1))
        entry = {"title": title, "task": task, "task-number": str(task_number)}
        if "shiboken" in title:
            if sha not in shiboken6_commits:
                shiboken6_commits[sha] = entry
        else:
            if sha not in pyside6_commits:
                pyside6_commits[sha] = entry


def create_fixes_log(versions: list[str]) -> None:
    git_command(versions, "Fixes: ")


def create_task_log(versions: list[str]) -> None:
    git_command(versions, "Task-number: ")


def extract_change_log(commit_message: list[str]) -> tuple[str, int, str]:
    """Extract a tuple of (component, task-number, change log paragraph)
       from a commit message of the form [ChangeLog][shiboken6] description..."""
    result = ''
    component = 'pyside'
    within_changelog = False
    task_nr = ''
    for line in commit_message:
        if within_changelog:
            if line:
                result += ' ' + line.strip()
            else:
                within_changelog = False
        else:
            if line.startswith('[ChangeLog]'):
                log_line = line[11:]
                if log_line.startswith('['):
                    end = log_line.find(']')
                    if end > 0:
                        component = log_line[1:end]
                        log_line = log_line[end + 1:]
                result = log_line.strip()
                within_changelog = True
            elif line.startswith("Fixes: ") or line.startswith("Task-number: "):
                task_nr = line.split(":")[1].strip()

    task_nr_int = -1
    if task_nr:
        result = f"[{task_nr}] {result}"
        dash = task_nr.find('-')  # "PYSIDE-627"
        task_nr_int = int(task_nr[dash + 1:])

    return (component, task_nr_int, format_text(result))


def create_change_log(versions: list[str]) -> None:
    for sha in git_get_sha1s(versions, r"\[ChangeLog\]"):
        change_log = extract_change_log(get_commit_content(sha).splitlines())
        component, task_nr, text = change_log
        if component.startswith('shiboken'):
            shiboken6_changelogs.append((task_nr, text))
        else:
            pyside6_changelogs.append((task_nr, text))


def format_commit_msg(entry: dict[str, str]) -> str:
    task = entry["task"].replace("Fixes: ", "").replace("Task-number: ", "")
    title = entry["title"]
    if title.startswith("shiboken6: "):
        title = title[11:]
    elif title.startswith("PySide6: "):
        title = title[9:]
    return format_text(f"[{task}] {title}")


def gen_list(d: dict[str, dict[str, str]]) -> str:
    return "\n".join(format_commit_msg(v)
                     for _, v in d.items())


def sort_dict(d: dict[str, dict[str, str]]) -> dict[str, dict[str, str]]:
    return dict(sorted(d.items(), key=lambda kv: int(kv[1]['task-number'])))


def sort_changelog(c: list[tuple[int, str]]) -> list[tuple[int, str]]:
    return sorted(c, key=lambda task_text_tuple: task_text_tuple[0])


if __name__ == "__main__":

    args = parse_options()
    pyside6_commits: dict[str, dict[str, str]] = {}
    shiboken6_commits: dict[str, dict[str, str]] = {}
    # Changelogs are tuples of task number/formatted text
    pyside6_changelogs: list[tuple[int, str]] = []
    shiboken6_changelogs: list[tuple[int, str]] = []

    exclude_pick_to = args.exclude

    # Getting commits information
    directory = args.directory if args.directory else "."
    versions = args.versions.split("..")
    if len(versions) == 2:
        if check_tag(versions[0]) and check_tag(versions[1]):
            create_fixes_log(versions)
            create_task_log(versions)
            create_change_log(versions)

    # Sort commits
    pyside6_commits = sort_dict(pyside6_commits)
    shiboken6_commits = sort_dict(shiboken6_commits)
    pyside6_changelogs = sort_changelog(pyside6_changelogs)
    shiboken6_changelogs = sort_changelog(shiboken6_changelogs)

    # Generate message
    print(content_header.replace("@VERSION", args.release).
          replace("@TYPE", args.type))
    for c in pyside6_changelogs:
        print(c[1])
    print(gen_list(pyside6_commits))
    if not pyside6_changelogs and not pyside6_commits:
        print(" - No changes")
    print(shiboken_header)
    for c in shiboken6_changelogs:
        print(c[1])
    print(gen_list(shiboken6_commits))
    if not shiboken6_changelogs and not shiboken6_commits:
        print(" - No changes")
