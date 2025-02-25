# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test cases for QImage'''

import gc
import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtGui import QImage, qRgb

from helper.usesqapplication import UsesQApplication


class SetPixelFloat(UsesQApplication):
    '''Test case for calling setPixel with float as argument'''

    def setUp(self):
        # Acquire resources
        super(SetPixelFloat, self).setUp()
        self.color = qRgb(255, 0, 0)
        self.image = QImage(200, 200, QImage.Format_RGB32)

    def tearDown(self):
        # Release resources
        del self.color
        del self.image
        # PYSIDE-535: Need to collect garbage in PyPy to trigger deletion
        gc.collect()
        super(SetPixelFloat, self).tearDown()

    def testFloat(self):
        # QImage.setPixel(float, float, color) - Implicit conversion
        self.image.setPixel(3.14, 4.2, self.color)
        self.assertEqual(self.image.pixel(3.14, 4.2), self.color)


if __name__ == '__main__':
    unittest.main()
