#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Utility functions for make update and make tests

WARNING:
- Python 3.6 is used on the Linux VM (Rocky8) to run "make update" to checkout LFS.
- Python 3.9 is used on the built-bot.

Take care *not* to use features from the Python version used by Blender!

NOTE:
Some type annotations are quoted to avoid errors in older Python versions.
These can be unquoted eventually.
"""

__all__ = (
    "call",
    "check_output",
    "command_missing",
    "git_branch",
    "git_branch_exists",
    "git_enable_submodule",
    "git_get_remote_url",
    "git_is_remote_repository",
    "git_remote_exist",
    "git_set_config",
    "git_update_submodule",
    "is_git_submodule_enabled",
    "parse_blender_version",
    "remove_directory",
)

import re
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

from types import (
    TracebackType,
)
from typing import (
    Any,
)

if sys.version_info >= (3, 9):
    from collections.abc import (
        Callable,
        Sequence,
    )
else:
    from typing import (
        Callable,
        Sequence,
    )


def call(
        cmd: Sequence[str],
        exit_on_error: bool = True,
        silent: bool = False,
        env: "dict[str, str] | None" = None,
) -> int:
    if not silent:
        cmd_str = ""
        if env:
            cmd_str += " ".join([f"{item[0]}={item[1]}" for item in env.items()])
            cmd_str += " "
        cmd_str += " ".join([str(x) for x in cmd])
        print(cmd_str)

    env_full = None
    if env:
        env_full = os.environ.copy()
        for key, value in env.items():
            env_full[key] = value

    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    if silent:
        retcode = subprocess.call(
            cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env_full)
    else:
        retcode = subprocess.call(cmd, env=env_full)

    if exit_on_error and retcode != 0:
        sys.exit(retcode)
    return retcode


def check_output(cmd: Sequence[str], exit_on_error: bool = True) -> str:
    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, universal_newlines=True)
    except subprocess.CalledProcessError as e:
        if exit_on_error:
            sys.stderr.write(" ".join(cmd) + "\n")
            sys.stderr.write(e.output + "\n")
            sys.exit(e.returncode)
        output = ""

    return output.strip()


def git_local_branch_exists(git_command: str, branch: str) -> bool:
    return (
        call([git_command, "rev-parse", "--verify", branch], exit_on_error=False, silent=True) == 0
    )


def git_remote_branch_exists(git_command: str, remote: str, branch: str) -> bool:
    return call([git_command, "rev-parse", "--verify", f"remotes/{remote}/{branch}"],
                exit_on_error=False, silent=True) == 0


def git_branch_exists(git_command: str, branch: str) -> bool:
    return (
        git_local_branch_exists(git_command, branch) or
        git_remote_branch_exists(git_command, "upstream", branch) or
        git_remote_branch_exists(git_command, "origin", branch)
    )


def git_get_remote_url(git_command: str, remote_name: str) -> str:
    return check_output((git_command, "ls-remote", "--get-url", remote_name))


def git_remote_exist(git_command: str, remote_name: str) -> bool:
    """Check whether there is a remote with the given name"""
    # `git ls-remote --get-url upstream` will print an URL if there is such remote configured, and
    # otherwise will print "upstream".
    remote_url = check_output((git_command, "ls-remote", "--get-url", remote_name))
    return remote_url != remote_name


def git_is_remote_repository(git_command: str, repo: str) -> bool:
    """Returns true if the given repository is a valid/clonable git repo"""
    exit_code = call((git_command, "ls-remote", repo, "HEAD"), exit_on_error=False, silent=True)
    return exit_code == 0


def git_get_remotes(git_command: str) -> Sequence[str]:
    """Get a list of git remotes"""
    # Additional check if the remote exists, for safety in case the output of this command
    # changes in the future.
    remotes = check_output([git_command, "remote"]).split()
    return [remote for remote in remotes if git_remote_exist(git_command, remote)]


def git_add_remote(git_command: str, name: str, url: str, push_url: str) -> None:
    """Add a git remote"""
    call((git_command, "remote", "add", name, url), silent=True)
    call((git_command, "remote", "set-url", "--push", name, push_url), silent=True)


def git_branch(git_command: str) -> str:
    """Get current branch name."""

    try:
        branch = subprocess.check_output([git_command, "rev-parse", "--abbrev-ref", "HEAD"])
    except subprocess.CalledProcessError:
        # No need to print the exception, error text is written to the output already.
        sys.stderr.write("Failed to get Blender git branch\n")
        sys.exit(1)

    return branch.strip().decode('utf8')


def git_get_config(git_command: str, key: str, file: "str | None" = None) -> str:
    if file:
        return check_output([git_command, "config", "--file", file, "--get", key])

    return check_output([git_command, "config", "--get", key])


def git_set_config(git_command: str, key: str, value: str, file: "str | None" = None) -> str:
    if file:
        return check_output([git_command, "config", "--file", file, key, value])

    return check_output([git_command, "config", key, value])


def _git_submodule_config_key(submodule_dir: Path, key: str) -> str:
    submodule_dir_str = submodule_dir.as_posix()
    return f"submodule.{submodule_dir_str}.{key}"


def is_git_submodule_enabled(git_command: str, submodule_dir: Path) -> bool:
    """Check whether submodule denoted by its directory within the repository is enabled"""

    git_root = Path(check_output([git_command, "rev-parse", "--show-toplevel"]))
    gitmodules = git_root / ".gitmodules"

    # Check whether the submodule actually exists.
    # Request path of an unknown submodule will cause non-zero exit code.
    path = git_get_config(
        git_command, _git_submodule_config_key(submodule_dir, "path"), str(gitmodules))
    if not path:
        return False

    # When the "update" strategy is not provided explicitly in the local configuration
    # `git config` returns a non-zero exit code. For those assume the default "checkout"
    # strategy.
    update = check_output(
        (git_command, "config", "--local", _git_submodule_config_key(submodule_dir, "update")),
        exit_on_error=False)
    if update == "":
        # The repository is not in our local configuration.
        # Check the default `.gitmodules` setting.
        update = check_output(
            (git_command, "config", "--file", str(gitmodules), _git_submodule_config_key(submodule_dir, "update")),
            exit_on_error=False)
    return update.lower() != "none"


def git_enable_submodule(git_command: str, submodule_dir: Path) -> None:
    """Enable submodule denoted by its directory within the repository"""

    command = (git_command,
               "config",
               "--local",
               _git_submodule_config_key(submodule_dir, "update"),
               "checkout")
    call(command, exit_on_error=True, silent=True)


def git_update_submodule(git_command: str, submodule_dir: Path) -> bool:
    """
    Update the given submodule.

    The submodule is denoted by its path within the repository.
    This function will initialize the submodule if it has not been initialized.

    Returns true if the update succeeded
    """

    # Use the two stage update process:
    # - Step 1: checkout the submodule to the desired (by the parent repository) hash, but
    #           skip the LFS smudging.
    # - Step 2: Fetch LFS files, if needed.
    #
    # This allows to show download progress, potentially allowing resuming the download
    # progress, and even recovering from partial/corrupted checkout of submodules.
    #
    # This bypasses the limitation of submodules which are configured as "update=checkout"
    # with regular `git submodule update` which, depending on the Git version will not report
    # any progress. This is because submodule--helper.c configures Git checkout process with
    # the "quiet" flag, so that there is no detached head information printed after submodule
    # update, and since Git 2.33 the LFS messages "Filtering contents..." is suppressed by
    #
    #   https://github.com/git/git/commit/7a132c628e57b9bceeb88832ea051395c0637b16
    #
    # Doing `git lfs pull` after checkout with `GIT_LFS_SKIP_SMUDGE=true` seems to be the
    # valid process. For example, https://www.mankier.com/7/git-lfs-faq

    env = {"GIT_LFS_SKIP_SMUDGE": "1"}

    if call((git_command, "submodule", "update", "--init", "--progress", str(submodule_dir)),
            exit_on_error=False, env=env) != 0:
        return False

    return call((git_command, "-C", str(submodule_dir), "lfs", "pull"),
                exit_on_error=False) == 0


def command_missing(command: str) -> bool:
    # Support running with Python 2 for macOS
    if sys.version_info >= (3, 0):
        return shutil.which(command) is None
    return False


class BlenderVersion:
    def __init__(self, version: int, patch: int, cycle: str):
        # 293 for 2.93.1
        self.version = version
        # 1 for 2.93.1
        self.patch = patch
        # 'alpha', 'beta', 'release', maybe others.
        self.cycle = cycle

    def is_release(self) -> bool:
        return self.cycle == "release"

    def __str__(self) -> str:
        """Convert to version string.

        >>> str(BlenderVersion(293, 1, "alpha"))
        '2.93.1-alpha'
        >>> str(BlenderVersion(327, 0, "release"))
        '3.27.0'
        """
        version_major = self.version // 100
        version_minor = self.version % 100
        as_string = f"{version_major}.{version_minor}.{self.patch}"
        if self.is_release():
            return as_string
        return f"{as_string}-{self.cycle}"


def parse_blender_version() -> BlenderVersion:
    blender_srcdir = Path(__file__).absolute().parent.parent.parent
    version_path = blender_srcdir / "source/blender/blenkernel/BKE_blender_version.h"

    version_info = {}
    line_re = re.compile(r"^#define (BLENDER_VERSION[A-Z_]*)\s+([0-9a-z]+)$")

    with version_path.open(encoding="utf-8") as version_file:
        for line in version_file:
            match = line_re.match(line.strip())
            if not match:
                continue
            version_info[match.group(1)] = match.group(2)

    return BlenderVersion(
        int(version_info["BLENDER_VERSION"]),
        int(version_info["BLENDER_VERSION_PATCH"]),
        version_info["BLENDER_VERSION_CYCLE"],
    )


def remove_directory(directory: Path) -> None:
    """
    Recursively remove the given directory

    Takes care of clearing read-only attributes which might prevent deletion on
    Windows.
    """
    # NOTE: unquote typing once Python 3.6x is dropped.
    def remove_readonly(
            func: Callable[..., Any],
            path: str,
            _: "tuple[type[BaseException], BaseException, TracebackType]",
    ) -> None:
        "Clear the read-only bit and reattempt the removal."
        os.chmod(path, stat.S_IWRITE)
        func(path)

    shutil.rmtree(directory, onerror=remove_readonly)
