# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test case for signal signature received by QObject::connectNotify().'''

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import QObject, Signal, SIGNAL, SLOT
from helper.usesqapplication import UsesQApplication


called = False
name = "Old"


class Sender(QObject):
    dummySignal = Signal()


class Obj(QObject):
    dummySignalArgs = Signal(str)
    numberSignal = Signal(int)

    def __init__(self):
        super().__init__()
        self.signal = ''

    def connectNotify(self, signal):
        self.signal = signal

    @staticmethod
    def static_method():
        global called
        called = True

    @staticmethod
    def static_method_args(arg="default"):
        global name
        name = arg


def callback(arg=None):
    pass


def callback_empty():
    pass


class TestConnectNotifyWithNewStyleSignals(UsesQApplication):
    '''Test case for signal signature received by QObject::connectNotify().'''

    def testOldStyle(self):
        sender = Obj()
        receiver = QObject()
        sender.connect(SIGNAL('destroyed()'), receiver, SLOT('deleteLater()'))
        self.assertEqual(sender.signal.methodSignature(), 'destroyed(QObject*)')

    def testOldStyleWithPythonCallback(self):
        sender = Obj()
        sender.connect(SIGNAL('destroyed()'), callback)
        self.assertEqual(sender.signal.methodSignature(), 'destroyed()')

    def testNewStyle(self):
        sender = Obj()

        sender.destroyed.connect(callback_empty)
        self.assertEqual(sender.signal.methodSignature(), 'destroyed()')

        sender.destroyed[QObject].connect(callback)
        self.assertEqual(sender.signal.methodSignature(), 'destroyed(QObject*)')

    def testStaticSlot(self):
        global called
        sender = Sender()
        sender.dummySignal.connect(Obj.static_method)
        sender.dummySignal.emit()
        self.assertTrue(called)

    def testStaticSlotArgs(self):
        global name
        sender = Obj()
        sender.dummySignalArgs.connect(Obj.static_method_args)
        sender.dummySignalArgs[str].emit("New")
        self.assertEqual(name, "New")

    def testLambdaSlot(self):
        sender = Obj()
        sender.numberSignal[int].connect(lambda x: 42)
        with self.assertRaises(IndexError):
            sender.numberSignal[str].emit("test")


if __name__ == '__main__':
    unittest.main()
