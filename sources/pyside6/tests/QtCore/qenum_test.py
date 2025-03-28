#!/usr/bin/python
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test cases for QEnum and QFlags'''

import gc
import os
import sys
import pickle
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import Qt, QIODevice, QObject, QEnum, QFlag


class TestEnum(unittest.TestCase):
    def testOperations(self):
        k = Qt.Key.Key_1

        # Integers
        self.assertEqual(k + 2, 2 + k)
        self.assertEqual(k - 2, -(2 - k))
        self.assertEqual(k * 2, 2 * k)

    @unittest.skipUnless(getattr(sys, "getobjects", None), "requires --with-trace-refs")
    @unittest.skipUnless(getattr(sys, "gettotalrefcount", None), "requires --with-pydebug")
    def testEnumNew_NoLeak(self):
        gc.collect()
        total = sys.gettotalrefcount()
        for _ in range(1000):
            ret = Qt.Key(42)  # noqa: F841

        gc.collect()
        delta = sys.gettotalrefcount() - total
        print("delta total refcount =", delta)
        if abs(delta) >= 10:
            all = [(sys.getrefcount(x), x) for x in sys.getobjects(0)]
            all.sort(key=lambda x: x[0], reverse=True)
            for ob in all[:10]:
                print(ob)
        self.assertTrue(abs(delta) < 10)


class TestQFlags(unittest.TestCase):

    def testToItn(self):
        om = QIODevice.NotOpen
        omcmp = om.value

        self.assertEqual(om, QIODevice.NotOpen)
        self.assertTrue(omcmp == 0)

        self.assertTrue(omcmp != QIODevice.ReadOnly)
        self.assertTrue(omcmp != 1)

    def testToIntInFunction(self):
        om = QIODevice.WriteOnly
        self.assertEqual(int(om.value), 2)

    def testNonExtensibleEnums(self):
        try:
            om = QIODevice.OpenMode(QIODevice.WriteOnly)  # noqa: F841
            self.assertFail()
        except:  # noqa: E722
            pass


# PYSIDE-15: Pickling of enums
class TestEnumPickling(unittest.TestCase):
    def testPickleEnum(self):

        # Pickling of enums with different depth works.
        ret = pickle.loads(pickle.dumps(QIODevice.Append))
        self.assertEqual(ret, QIODevice.Append)

        ret = pickle.loads(pickle.dumps(Qt.Key.Key_Asterisk))
        self.assertEqual(ret, Qt.Key.Key_Asterisk)
        self.assertEqual(ret, Qt.Key(42))

        # We can also pickle the whole enum class (built in):
        ret = pickle.loads(pickle.dumps(QIODevice))

        # This works also with nested classes for Python 3, after we
        # introduced the correct __qualname__ attribute.
        func = lambda: pickle.loads(pickle.dumps(Qt.Key))  # noqa: E731
        func()

# PYSIDE-957: The QEnum macro


try:
    import enum
    HAVE_ENUM = True
except ImportError:
    HAVE_ENUM = False
    QEnum = QFlag = lambda x: x  # noqa: F811
    import types

    class Enum:
        pass
    enum = types.ModuleType("enum")
    enum.Enum = enum.Flag = enum.IntEnum = enum.IntFlag = Enum
    Enum.__module__ = "enum"
    Enum.__members__ = {}
    del Enum
    # PYSIDE-535: Need to collect garbage in PyPy to trigger deletion
    gc.collect()
    enum.auto = lambda: 42

HAVE_FLAG = hasattr(enum, "Flag")


@QEnum
class OuterEnum(enum.Enum):
    A = 1
    B = 2


class SomeClass(QObject):

    @QEnum
    class SomeEnum(enum.Enum):
        A = 1
        B = 2
        C = 3

    @QEnum
    class OtherEnum(enum.IntEnum):
        A = 1
        B = 2
        C = 3

    class InnerClass(QObject):

        @QEnum
        class InnerEnum(enum.Enum):
            X = 42

    class SomeEnum(enum.Enum):  # noqa: F811
        A = 4
        B = 5
        C = 6

    QEnum(SomeEnum)     # works even without the decorator assignment


class TestQEnumMacro(unittest.TestCase):
    meta_name = "EnumType" if sys.version_info[:2] >= (3, 11) else "EnumMeta"

    def testTopLevel(self):
        self.assertEqual(type(OuterEnum).__name__, self.meta_name)
        self.assertEqual(len(OuterEnum.__members__), 2)

    def testSomeClass(self):
        self.assertEqual(type(SomeClass.SomeEnum).__name__, self.meta_name)
        self.assertEqual(len(SomeClass.SomeEnum.__members__), 3)
        with self.assertRaises(TypeError):
            int(SomeClass.SomeEnum.C) == 6
        self.assertEqual(SomeClass.OtherEnum.C, 3)

    def testInnerClass(self):
        self.assertEqual(SomeClass.InnerClass.InnerEnum.__qualname__,
                         "SomeClass.InnerClass.InnerEnum")
        with self.assertRaises(TypeError):
            int(SomeClass.InnerClass.InnerEnum.X) == 42

    @unittest.skipUnless(HAVE_FLAG, "some older Python versions have no 'Flag'")
    def testEnumFlag(self):
        with self.assertRaises(TypeError):
            class WrongFlagForEnum(QObject):
                @QEnum
                class Bad(enum.Flag):
                    pass
        with self.assertRaises(TypeError):
            class WrongEnuForFlag(QObject):
                @QFlag
                class Bad(enum.Enum):
                    pass

    def testIsRegistered(self):
        mo = SomeClass.staticMetaObject
        self.assertEqual(mo.enumeratorCount(), 2)
        self.assertEqual(mo.enumerator(0).name(), "OtherEnum")
        self.assertEqual(mo.enumerator(0).scope(), "SomeClass")
        self.assertEqual(mo.enumerator(1).name(), "SomeEnum")
        moi = SomeClass.InnerClass.staticMetaObject
        self.assertEqual(moi.enumerator(0).name(), "InnerEnum")
        # Question: Should that scope not better be  "SomeClass.InnerClass"?
        # But we have __qualname__ already:
        self.assertEqual(moi.enumerator(0).scope(), "InnerClass")


if __name__ == '__main__':
    unittest.main()
