# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
from __future__ import annotations

import calendar
import datetime
import os
import site
import sys
from pathlib import Path

from build_scripts.options import has_option, option_value
from build_scripts.utils import (parse_cmake_conf_assignments_by_key,
                                 remove_tree, run_instruction)


class CI:
    def __init__(self):
        # Values must match COIN thrift
        self.HOST_OS = option_value("os")
        self.TARGET_OS = option_value("targetOs")
        self.HOST_ARCH = option_value("hostArch")
        self.TARGET_ARCH = option_value("targetArch")
        self.HOST_OS_VER = option_value("osVer")
        self.ENV_INSTALL_DIR = option_value("instdir")
        self.ENV_AGENT_DIR = option_value("agentdir") or "."
        self.COMPILER = option_value("compiler")
        self.USE_SCCACHE = option_value("compiler-launcher")
        self.INTEGRATION_ID = option_value("coinIntegrationId") or str(
            calendar.timegm(datetime.datetime.now().timetuple())
        )
        self.FEATURES = []
        _ci_features = option_value("features")
        if _ci_features is not None:
            for f in _ci_features.split(", "):
                self.FEATURES.append(f)
        self.RELEASE_CONF = has_option("packaging")
        self.TEST_PHASE = option_value("phase")
        if self.TEST_PHASE not in ["ALL", "BUILD"]:
            self.TEST_PHASE = "ALL"


def get_ci_exe_path(ci_install_dir, ci_host_os, qtexe):
    """
    qtexe can only be 'qmake' or 'qtpaths'
    """
    ext = ""
    if ci_host_os == "Windows":
        ext = ".exe"

    _path = Path(ci_install_dir) / "bin" / f"{qtexe}{ext}"

    return f"--{qtexe}={_path}"


def get_env_or_raise(name: str) -> str:
    o = os.getenv(name)
    if o is None:
        raise Exception(f"Variable not defined: {name}")
    return o


def get_qtci_virtualenv(python_ver, log, host, host_arch, target_arch):
    _exe = "python"
    _env = os.environ.get("PYSIDE_VIRTUALENV") or f"env{python_ver}"
    env_python = f"{_env}/bin/python"
    env_pip = f"{_env}/bin/pip"

    if host == "Windows":
        log.info("New virtualenv to build {target_arch} in {host_arch} host")
        _exe = "python.exe"
        if python_ver.startswith("3"):
            var = f"PYTHON{python_ver}-64_PATH"
            log.info(f"Try to find python from {var} env variable")
            _path = Path(os.getenv(var, ""))
            _exe = _path / "python.exe"
            if not _exe.is_file():
                log.warning(f"Can't find python.exe from {_exe}, using default python3")
                _exe = Path(get_env_or_raise("PYTHON3_PATH")) / "python.exe"
        env_python = rf"{_env}\Scripts\python.exe"
        env_pip = rf"{_env}\Scripts\pip.exe"
    else:
        _exe = f"python{python_ver}"
        try:
            run_instruction([_exe, "--version"], f"Failed to guess python version {_exe}")
        except Exception as e:
            print(f"Exception {type(e).__name__}: {e}")
            _exe = "python3"
    return (_exe, _env, env_pip, env_python)


def get_current_script_path():
    """Returns the absolute path containing this script."""
    try:
        this_file = __file__
    except NameError:
        this_file = sys.argv[0]
    this_file = Path(this_file).resolve()
    return this_file.parents[0]


def is_snapshot_build():
    """
    Returns True if project needs to be built with --snapshot-build

    This is true if the version found in .cmake.conf is not a
    pre-release version (no alphas, betas).

    This eliminates the need to remove the --snapshot-build option
    on a per-release branch basis (less things to remember to do
    for a release).
    """
    # This returns pyside-setup/coin/ so we go one level down
    # to get the root of the repo
    setup_script_dir = get_current_script_path()
    pyside_project_dir = setup_script_dir / ".." / "sources" / "pyside6"

    d = parse_cmake_conf_assignments_by_key(str(pyside_project_dir))
    release_version_type = d.get("pyside_PRE_RELEASE_VERSION_TYPE")
    pre_release_version = d.get("pyside_PRE_RELEASE_VERSION")
    if pre_release_version and release_version_type:
        return True
    return False


def get_architecture(ci):
    return "32" if ci.TARGET_ARCH == "X86" else "64"


def get_python_version(ci):
    python_ver = "3"
    if ci.TARGET_OS == "Linux" and ci.HOST_ARCH != "aarch64":
        python_ver = "3.11"
    elif ci.TARGET_OS == "Windows":
        python_ver = "3.10.0"
    return python_ver


def remove_variables(vars):
    for env_var in vars:
        if os.environ.get(env_var):
            del os.environ[env_var]


def setup_virtualenv(python, exe, env, pip, log):
    # Within Ubuntu 24.04 one can't install anything with pip to outside of
    # virtual env. Trust that we already have proper virtualenv installed.
    if os.environ.get("HOST_OSVERSION_COIN") != "ubuntu_24_04":
        run_instruction(
            [str(python), "-m", "pip", "install", "--user", "virtualenv==20.7.2"],
            "Failed to pin virtualenv",
        )
    # installing to user base might not be in PATH by default.
    env_path = Path(str(site.USER_BASE)) / "bin"
    v_env = env_path / "virtualenv"
    if sys.platform == "win32":
        env_path = os.path.join(site.USER_BASE, "Scripts")
        v_env = os.path.join(env_path, "virtualenv.exe")
    try:
        run_instruction([str(v_env), "--version"], "Using default virtualenv")
    except Exception as e:
        log.info("Failed to use the default virtualenv")
        log.info(f"{type(e).__name__}: {e}")
        v_env = "virtualenv"
    run_instruction([str(v_env), "-p", str(exe), str(env)], "Failed to create virtualenv")
    # Pip is always upgraded when CI template is provisioned,
    # upgrading it in later phase may cause perm issue
    run_instruction(
        [str(pip), "install", "-r", "requirements.txt"], "Failed to install dependencies"
    )


def call_setup(python_ver, ci, phase, log, buildnro=0):
    print("call_setup")
    print("python_ver", python_ver)
    print("phase", phase)
    exe, env, pip, env_python = get_qtci_virtualenv(
        python_ver, log, ci.HOST_OS, ci.HOST_ARCH, ci.TARGET_ARCH
    )

    if phase not in ["BUILD", "TEST"]:
        sys.exit(1)

    remove_tree(env, True)
    # Pinning the virtualenv before creating one
    # Use pip3 if possible while pip seems to install the virtualenv to wrong dir in some OS
    python = "python3"
    if sys.platform == "win32":
        python = Path(get_env_or_raise("PYTHON3_PATH")) / "python.exe"

    if phase == "BUILD":
        setup_virtualenv(python, exe, env, pip, log)
    elif phase == "TEST":

        if ci.HOST_OS == "MacOS" and ci.HOST_ARCH == "ARM64":
            v_env = "virtualenv"
            run_instruction([str(v_env), "-p", str(exe), str(env)], "Failed to create virtualenv")
            run_instruction(
                [pip, "install", "-r", "requirements.txt"], "Failed to install dependencies"
            )
        else:
            setup_virtualenv(python, exe, env, pip, log)
            # Install distro to replace missing platform.linux_distribution() in python3.8
            run_instruction([pip, "install", "distro"], "Failed to install distro")

    if phase == "BUILD":
        cmd = [
            env_python,
            "-u",
            "setup.py",
            "build",
            "--standalone",
            "--unity",
            "--build-tests",
            "--log-level=verbose",
            "--limited-api=yes",
        ]

        if ci.TARGET_ARCH == "X86_64-ARM64":
            cmd += ["--macos-arch='x86_64;arm64'"]

        if ci.USE_SCCACHE:
            cmd += [f"--compiler-launcher={ci.USE_SCCACHE}"]

        if is_snapshot_build():
            cmd += ["--snapshot-build"]

        qtpaths_path = get_ci_exe_path(ci.ENV_INSTALL_DIR, ci.HOST_OS, "qtpaths")
        cmd.append(qtpaths_path)

        # Due to certain older CMake versions generating very long paths
        # (at least with CMake 3.6.2) when using the export() function,
        # pass the shorter paths option on Windows so we don't hit
        # the path character length limit (260).
        if ci.HOST_OS == "Windows":
            cmd += ["--shorter-paths"]

        cmd += ["--package-timestamp=" + ci.INTEGRATION_ID]

        env = os.environ
        run_instruction(cmd, "Failed to run setup.py for build", initial_env=env)
    elif phase == "TEST":
        cmd = [
            env_python,
            "testrunner.py",
            "test",
            "--blacklist",
            "build_history/blacklist.txt",
            f"--buildno={buildnro}",
        ]
        run_instruction(cmd, "Failed to run testrunner.py")

        qmake_path = get_ci_exe_path(ci.ENV_INSTALL_DIR, ci.HOST_OS, "qmake")

        # Try to install built wheels, and build some buildable examples.
        if ci.RELEASE_CONF:
            wheel_tester_path = os.path.join("testing", "wheel_tester.py")
            # Run the test for the new set of wheels
            cmd = [env_python, wheel_tester_path, qmake_path, "--wheels-dir=dist", "--new"]
            run_instruction(cmd, "Error while running wheel_tester.py on new wheels")
