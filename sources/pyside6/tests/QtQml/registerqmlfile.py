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

from PySide6.QtCore import QDir, QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import qmlRegisterType


class TestQmlSupport(unittest.TestCase):

    def testIt(self):
        app = QGuiApplication([])

        file = os.fspath(Path(__file__).resolve().parent / 'ModuleType.qml')
        url = QUrl.fromLocalFile(QDir.fromNativeSeparators(file))
        result = qmlRegisterType(url, "CustomModule", 1, 0, "ModuleType")
        self.assertTrue(result != -1)


if __name__ == '__main__':
    unittest.main()
