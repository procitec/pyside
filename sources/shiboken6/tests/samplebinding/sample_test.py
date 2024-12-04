#!/usr/bin/env python
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test cases for libsample bindings module'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

import sample


class ModuleTest(unittest.TestCase):
    '''Test case for module and global functions'''

    def testAddedFunctionAtModuleLevel(self):
        '''Calls function added to module from type system description.'''
        str1 = 'Foo'
        self.assertEqual(sample.multiplyString(str1, 3), str1 * 3)
        self.assertEqual(sample.multiplyString(str1, 0), str1 * 0)

    def testAddedFunctionWithVarargs(self):
        '''Calls function that receives varargs added to module from type system description.'''
        self.assertEqual(sample.countVarargs(1), 0)
        self.assertEqual(sample.countVarargs(1, 2), 1)
        self.assertEqual(sample.countVarargs(1, 2, 3, 'a', 'b', 4, (5, 6)), 6)

    def testSampleComparisonOpInNamespace(self):
        s1 = sample.sample.sample(10)
        s2 = sample.sample.sample(10)
        self.assertEqual(s1, s2)

    def testConstant(self):
        self.assertEqual(sample.sample.INT_CONSTANT, 42)

    def testStringFunctions(self):
        # Test plain ASCII, UCS1 and UCS4 encoding which have different
        # representations in the PyUnicode objects.
        for t1 in ["ascii", "Ümläut", "😀"]:
            expected = t1 + t1
            self.assertEqual(sample.addStdStrings(t1, t1), expected)
            self.assertEqual(sample.addStdWStrings(t1, t1), expected)

    def testNullPtrT(self):
        sample.testNullPtrT(None)
        self.assertRaises(TypeError, sample.testNullPtrT, 42)

    def testRValueRefsWithValueTypes(self):
        """Passing value types by rvalue refs: For value types, nothing should
           happen since the argument is copied in the call and the copy is
           moved from."""
        polygon = sample.Polygon()
        polygon.addPoint(sample.Point(1, 2))
        polygon.addPoint(sample.Point(3, 4))
        point_count = len(polygon.points())
        self.assertEqual(point_count, sample.takePolygon(polygon))

    def testRValueRefsWithObjectTypes(self):
        """Passing object types by rvalue refs: The underlying object should
           be moved from."""
        o = sample.ObjectType()
        object_name = "Name"
        o.setObjectName(object_name)
        self.assertEqual(len(object_name), sample.takeObjectType(o))
        # o should be moved from, name is now empty
        self.assertEqual(len(o.objectName()), 0)


if __name__ == '__main__':
    unittest.main()
