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

from PySide6.QtCore import QEvent, Qt
import PySide6


TEST_EVENT_TYPE = QEvent.Type(QEvent.registerEventType())


class TestEvent(QEvent):
    TestEventType = QEvent.Type(TEST_EVENT_TYPE)

    def __init__(self, rand=0):
        super().__init__(TestEvent.TestEventType)
        self._rand = rand

    def getRand(self):
        return self._rand


class TestEnums(unittest.TestCase):
    def testUserTypesValues(self):
        self.assertTrue(QEvent.User <= TestEvent.TestEventType <= QEvent.MaxUser)
        self.assertTrue(QEvent.User <= TEST_EVENT_TYPE <= QEvent.MaxUser)


if __name__ == '__main__':
    unittest.main()
