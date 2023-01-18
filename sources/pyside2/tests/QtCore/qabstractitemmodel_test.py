# -*- coding: utf-8 -*-

#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of Qt for Python.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

import os
import sys
import unittest

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from init_paths import init_test_paths
init_test_paths(False)

from PySide2.QtCore import *

class MyModel (QAbstractListModel):
    pass


class TestQModelIndexInternalPointer(unittest.TestCase):

    def testInternalPointer(self):
        m = MyModel()
        foo = QObject()
        idx = m.createIndex(0,0, foo)
        check = m.checkIndex(idx, QAbstractItemModel.CheckIndexOption.IndexIsValid
                                  | QAbstractItemModel.CheckIndexOption.DoNotUseParent
                                  | QAbstractItemModel.CheckIndexOption.ParentIsInvalid)
        self.assertTrue(check)

    def testPassQPersistentModelIndexAsQModelIndex(self):
        # Related to bug #716
        m = MyModel()
        idx = QPersistentModelIndex()
        m.span(idx)

    def testQIdentityProxyModel(self):
        sourceModel = QStringListModel(['item1', 'item2'])
        sourceIndex = sourceModel.index(0, 0)
        sourceData = str(sourceModel.data(sourceIndex, Qt.DisplayRole))
        proxyModel = QIdentityProxyModel()
        proxyModel.setSourceModel(sourceModel)
        proxyIndex = proxyModel.mapFromSource(sourceIndex)
        proxyData = str(proxyModel.data(proxyIndex, Qt.DisplayRole))
        self.assertEqual(sourceData, proxyData)

if __name__ == '__main__':
    unittest.main()
