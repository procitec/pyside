#!/usr/bin/env python
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test cases for Derived class'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

import sample
from sample import Abstract, Derived, DerivedUsingCt, OverloadedFuncEnum


class Deviant(Derived):
    def __init__(self):
        Derived.__init__(self)
        self.pure_virtual_called = False
        self.unpure_virtual_called = False

    def pureVirtual(self):
        self.pure_virtual_called = True

    def unpureVirtual(self):
        self.unpure_virtual_called = True

    def className(self):
        return 'Deviant'


class ImplementVirtualWithOutParameter(Derived):
    def __init__(self, value):
        super().__init__()
        self._value = value

    def virtualWithOutParameter(self):
        return self._value


class DerivedTest(unittest.TestCase):
    '''Test case for Derived class'''

    def testParentClassMethodsAvailability(self):
        '''Test if Derived class really inherits its methods from parent.'''
        inherited_methods = set(['callPureVirtual', 'callUnpureVirtual',
                                 'id_', 'pureVirtual', 'unpureVirtual'])
        self.assertTrue(inherited_methods.issubset(dir(Derived)))

    def testOtherOverloadedMethodCall(self):
        '''Another test to check overloaded method calling, just to double check.'''
        derived = Derived()

        result = derived.otherOverloaded(1, 2, True, 3.3)
        self.assertEqual(type(result), Derived.OtherOverloadedFuncEnum)
        self.assertEqual(result, sample.Derived.OtherOverloadedFunc_iibd)

        result = derived.otherOverloaded(1, 2.2)
        self.assertEqual(type(result), Derived.OtherOverloadedFuncEnum)
        self.assertEqual(result, Derived.OtherOverloadedFunc_id)

    def testOverloadedMethodCallWithDifferentNumericTypes(self):
        '''Test if the correct overloaded method accepts a different numeric type as argument.'''
        derived = Derived()
        result = derived.overloaded(1.1, 2.2)
        self.assertEqual(type(result), OverloadedFuncEnum)

    def testOverloadedMethodCallWithWrongNumberOfArguments(self):
        '''Test if a call to an overloaded method with the wrong number of arguments
           raises an exception.'''
        derived = Derived()
        self.assertRaises(TypeError, derived.otherOverloaded, 1, 2, True)

    def testReimplementedPureVirtualMethodCall(self):
        '''Test if a Python override of a implemented pure virtual method is
           correctly called from C++.'''
        d = Deviant()
        d.callPureVirtual()
        self.assertTrue(d.pure_virtual_called)

    def testReimplementedVirtualMethodCall(self):
        '''Test if a Python override of a reimplemented virtual method is
           correctly called from C++.'''
        d = Deviant()
        d.callUnpureVirtual()
        self.assertTrue(d.unpure_virtual_called)

    def testVirtualMethodCallString(self):
        '''Test virtual method call returning string.'''
        d = Derived()
        self.assertEqual(d.className(), 'Derived')
        self.assertEqual(d.getClassName(), 'Derived')

    def testReimplementedVirtualMethodCallReturningString(self):
        '''Test if a Python override of a reimplemented virtual method is
           correctly called from C++.'''
        d = Deviant()
        self.assertEqual(d.className(), 'Deviant')
        self.assertEqual(d.getClassName(), 'Deviant')

    def testSingleArgument(self):
        '''Test singleArgument call.'''
        d = Derived()
        self.assertTrue(d.singleArgument(False))
        self.assertTrue(not d.singleArgument(True))

    def testMethodCallWithDefaultValue(self):
        '''Test method call with default value.'''
        d = Derived()
        self.assertEqual(d.defaultValue(3), 3.1)
        self.assertEqual(d.defaultValue(), 0.1)

    def testCallToMethodWithAbstractArgument(self):
        '''Call to method that expects an Abstract argument.'''
        objId = 123
        d = Derived(objId)
        self.assertEqual(Abstract.getObjectId(d), objId)

    def testObjectCreationWithParentType(self):
        '''Derived class creates an instance of itself in C++ and returns it as
           a pointer to its ancestor Abstract.'''
        obj = Derived.createObject()
        self.assertEqual(type(obj), Derived)

    def testDerivedUsingCt(self):
        '''Test whether a constructor of the base class declared by using works'''
        obj = DerivedUsingCt(42)
        self.assertEqual(obj.value(), 42)

    def testVirtualWithOutParameter(self):
        d = Derived()
        self.assertEqual(d.callVirtualWithOutParameter(), 42)

        d = ImplementVirtualWithOutParameter(1)
        self.assertEqual(d.callVirtualWithOutParameter(), 1)


if __name__ == '__main__':
    unittest.main()
