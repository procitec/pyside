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
init_test_paths(True)

from PySide2.QtCore import Qt
from testbinding import Enum1, TestObjectWithoutNamespace

class ListConnectionTest(unittest.TestCase):

    def testEnumVisibility(self):
        self.assertEqual(Enum1.Option1, 1)
        self.assertEqual(Enum1.Option2, 2)
        self.assertEqual(TestObjectWithoutNamespace.Enum2.Option3, 3)
        self.assertEqual(TestObjectWithoutNamespace.Enum2.Option4, 4)

    def testFlagComparisonOperators(self):  # PYSIDE-1696, compare to self
        f1 = Qt.AlignHCenter | Qt.AlignBottom
        f2 = Qt.AlignHCenter | Qt.AlignBottom
        self.assertTrue(f1 == f1)
        self.assertTrue(f1 <= f1)
        self.assertTrue(f1 >= f1)
        self.assertFalse(f1 != f1)
        self.assertFalse(f1 < f1)
        self.assertFalse(f1 > f1)

        self.assertTrue(f1 == f2)
        self.assertTrue(f1 <= f2)
        self.assertTrue(f1 >= f2)
        self.assertFalse(f1 != f2)
        self.assertFalse(f1 < f2)
        self.assertFalse(f1 > f2)

        self.assertTrue(Qt.AlignHCenter < Qt.AlignBottom)
        self.assertFalse(Qt.AlignHCenter > Qt.AlignBottom)
        self.assertFalse(Qt.AlignBottom < Qt.AlignHCenter)
        self.assertTrue(Qt.AlignBottom > Qt.AlignHCenter)


if __name__ == '__main__':
    unittest.main()

