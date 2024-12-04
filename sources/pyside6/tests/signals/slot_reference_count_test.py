# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

''' Forced disconnection: Delete one end of the signal connection'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import QObject, Signal


class Dummy(QObject):
    foo = Signal()

    def dispatch(self):
        self.foo.emit()


class PythonSignalRefCount(unittest.TestCase):

    def setUp(self):
        self.emitter = Dummy()

    def tearDown(self):
        self.emitter

    @unittest.skipUnless(hasattr(sys, "getrefcount"), f"{sys.implementation.name} has no refcount")
    def testRefCount(self):
        def cb(*args):
            pass

        self.assertEqual(sys.getrefcount(cb), 2)

        self.emitter.foo.connect(cb)
        self.assertEqual(sys.getrefcount(cb), 3)

        self.emitter.foo.disconnect(cb)
        self.assertEqual(sys.getrefcount(cb), 2)


class CppSignalRefCount(unittest.TestCase):

    def setUp(self):
        self.emitter = QObject()

    def tearDown(self):
        self.emitter

    @unittest.skipUnless(hasattr(sys, "getrefcount"), f"{sys.implementation.name} has no refcount")
    def testRefCount(self):
        def cb(*args):
            pass

        self.assertEqual(sys.getrefcount(cb), 2)

        self.emitter.destroyed.connect(cb)
        self.assertEqual(sys.getrefcount(cb), 3)

        self.emitter.destroyed.disconnect(cb)
        self.assertEqual(sys.getrefcount(cb), 2)


if __name__ == '__main__':
    unittest.main()
