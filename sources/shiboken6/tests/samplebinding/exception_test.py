#!/usr/bin/env python
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

from sample import ExceptionTest


class CppExceptionTest(unittest.TestCase):

    def testVoid(self):
        exceptionCount = 0
        et = ExceptionTest()

        et.voidThrowStdException(False)

        try:
            et.voidThrowStdException(True)
        except:  # noqa: E722
            exceptionCount += 1

        et.voidThrowInt(False)

        try:
            et.voidThrowInt(True)
        except:  # noqa: E722
            exceptionCount += 1

        self.assertEqual(exceptionCount, 2)

    def testReturnValue(self):
        exceptionCount = 0
        et = ExceptionTest()

        result = et.intThrowStdException(False)

        try:
            result = et.intThrowStdException(True)
        except:  # noqa: E722
            exceptionCount += 1

        result = et.intThrowInt(False)

        try:
            result = et.intThrowInt(True)  # noqa: F841
        except:  # noqa: E722
            exceptionCount += 1

        self.assertEqual(exceptionCount, 2)

        def testModifications(self):
            """PYSIDE-1995, test whether exceptions are propagated
               when return ownership modifications are generated."""
            exceptionCount = 0
            try:
                et = ExceptionTest.create(True)  # noqa: F841
            except:  # noqa: E722
                exceptionCount += 1
            self.assertEqual(exceptionCount, 1)


if __name__ == '__main__':
    unittest.main()
