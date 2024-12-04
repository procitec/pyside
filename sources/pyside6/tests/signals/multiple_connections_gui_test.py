# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtWidgets import QPushButton, QSpinBox

from helper.basicpyslotcase import BasicPySlotCase
from helper.usesqapplication import UsesQApplication


class QtGuiMultipleSlots(UsesQApplication):
    '''Multiple connections to QtGui signals'''

    def run_many(self, signal, emitter, receivers, args=None):
        """Utility method to connect a list of receivers to a signal.
        sender - QObject that will emit the signal
        signal - string with the signal signature
        emitter - the callable that will trigger the signal
        receivers - list of BasicPySlotCase instances
        args - tuple with the arguments to be sent.
        """

        if args is None:
            args = tuple()

        for rec in receivers:
            rec.setUp()
            signal.connect(rec.cb)
            rec.args = tuple(args)

        emitter(*args)

        for rec in receivers:
            self.assertTrue(rec.called)

    def testButtonClick(self):
        """Multiple connections to QPushButton.clicked()"""
        sender = QPushButton('button')
        receivers = [BasicPySlotCase() for x in range(30)]
        self.run_many(sender.clicked, sender.click, receivers)

    def testSpinBoxValueChanged(self):
        """Multiple connections to QSpinBox.valueChanged(int)"""
        sender = QSpinBox()
        # FIXME if number of receivers if higher than 50, segfaults
        receivers = [BasicPySlotCase() for x in range(10)]
        self.run_many(sender.valueChanged, sender.setValue,
                      receivers, (1,))


if __name__ == '__main__':
    unittest.main()
