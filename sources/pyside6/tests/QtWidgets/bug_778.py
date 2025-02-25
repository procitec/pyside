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

from helper.usesqapplication import UsesQApplication

from PySide6.QtWidgets import QTreeWidget, QTreeWidgetItem, QTreeWidgetItemIterator


class QTreeWidgetItemIteratorTest(UsesQApplication):
    def testWidgetIterator(self):
        treeWidget = QTreeWidget()
        treeWidget.setColumnCount(1)
        items = []
        for i in range(10):
            items.append(QTreeWidgetItem(None, [f'item: {i}']))
        treeWidget.insertTopLevelItems(0, items)

        index = 0
        for it in QTreeWidgetItemIterator(treeWidget):
            self.assertEqual(it.value().text(0), f'item: {index}')
            index += 1


if __name__ == '__main__':
    unittest.main()
