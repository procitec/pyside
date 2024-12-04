# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

import os
import sys
import time
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from init_paths import init_test_paths
init_test_paths(False)

from PySide6.QtCore import QRunnable, QThread, QThreadPool


thread_function_called = False


class thread_function():
    global thread_function_called
    thread_function_called = True


class Task(QRunnable):
    def run(self):
        QThread.msleep(100)


class QThreadPoolTest(unittest.TestCase):
    def testSlowJobs(self):
        '''This used to cause a segfault due the ownership control on
           globalInstance function'''
        for i in range(3):
            task = Task()
            QThreadPool.globalInstance().start(task)
            time.sleep(0.05)

        self.assertTrue(QThreadPool.globalInstance().waitForDone())

    def testCallable(self):
        global thread_function_called
        tp = QThreadPool.globalInstance()
        tp.start(thread_function)
        self.assertTrue(tp.waitForDone())
        self.assertTrue(thread_function_called)


if __name__ == '__main__':
    unittest.main()
