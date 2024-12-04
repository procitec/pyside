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
from smart import Integer, StdDoublePtr, StdSharedPtrTestBench, StdSharedPtrVirtualMethodTester, std


def call_func_on_ptr(ptr):
    ptr.printInteger()


class VirtualTester(StdSharedPtrVirtualMethodTester):

    def doModifyInteger(self, p):
        p.setValue(p.value() * 2)
        return p


class StdSharedPtrTests(unittest.TestCase):
    def testInteger(self):
        p = StdSharedPtrTestBench.createInteger()
        # PYSIDE-2462: Ensure Integer's __dir__ entries in the pointer's
        self.assertTrue("printInteger" in dir(p))
        StdSharedPtrTestBench.printInteger(p)
        self.assertTrue(p)
        call_func_on_ptr(p)

        np = StdSharedPtrTestBench.createNullInteger()
        StdSharedPtrTestBench.printInteger(np)
        self.assertFalse(np)
        self.assertRaises(AttributeError, call_func_on_ptr, np)

        iv = Integer()
        iv.setValue(42)
        np = std.shared_ptr_Integer(iv)
        self.assertEqual(np.value(), 42)

    def testInt(self):
        np = StdSharedPtrTestBench.createNullInt()
        StdSharedPtrTestBench.printInt(np)
        self.assertFalse(np)
        p = StdSharedPtrTestBench.createInt()
        StdSharedPtrTestBench.printInt(p)
        ip = std.StdIntPtr(42)
        StdSharedPtrTestBench.printInt(ip)

    def testDouble(self):
        np = StdSharedPtrTestBench.createNullDouble()
        StdSharedPtrTestBench.printDouble(np)
        self.assertFalse(np)
        p = StdSharedPtrTestBench.createDouble(67)
        StdSharedPtrTestBench.printDouble(p)
        dp = StdDoublePtr(42)
        StdSharedPtrTestBench.printDouble(dp)

    def testString(self):
        np = StdSharedPtrTestBench.createNullString()
        StdSharedPtrTestBench.printString(np)
        self.assertFalse(np)
        p = StdSharedPtrTestBench.createString("bla")
        StdSharedPtrTestBench.printString(p)

    def testVirtuals(self):
        """Test whether code generating virtual function overrides is generated
           correctly."""
        p = StdSharedPtrTestBench.createInteger()
        p.setValue(42)
        v = StdSharedPtrVirtualMethodTester()
        r = v.callModifyInteger(p)  # Base implementation increments
        self.assertEqual(r.value(), 43)

        p.setValue(42)
        v = VirtualTester()
        r = v.callModifyInteger(p)  # Derived implementation doubles
        self.assertEqual(r.value(), 84)


if __name__ == '__main__':
    unittest.main()
