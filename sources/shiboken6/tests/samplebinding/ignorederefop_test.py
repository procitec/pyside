# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()
from sample import Reference


class TestLackOfDereferenceOperators (unittest.TestCase):
    def testIf(self):
        r = Reference()
        self.assertFalse(hasattr(r, "__mul__"))


if __name__ == '__main__':
    unittest.main()
