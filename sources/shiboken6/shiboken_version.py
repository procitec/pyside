# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
from __future__ import annotations

major_version = "@shiboken_MAJOR_VERSION@"
minor_version = "@shiboken_MINOR_VERSION@"
patch_version = "@shiboken_MICRO_VERSION@"

# For example: "a", "b", "rc"
# (which means "alpha", "beta", "release candidate").
# An empty string means the generated package will be an official release.
release_version_type = "@shiboken_PRE_RELEASE_VERSION_TYPE@"

# For example: "1", "2" (which means "beta1", "beta2", if type is "b").
pre_release_version = "@shiboken_PRE_RELEASE_VERSION@"

if __name__ == '__main__':
    # Used by CMake.
    print(f'{major_version};{minor_version};{patch_version};'
          f'{release_version_type};{pre_release_version}')
