# Copyright (C) 2013 Riverbank Computing Limited.
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
from __future__ import annotations

import sys

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QApplication, QGridLayout,
                               QLabel, QLineEdit,
                               QMessageBox, QPushButton, QTextEdit,
                               QVBoxLayout, QWidget)


class SortedDict(dict):
    class Iterator(object):
        def __init__(self, sorted_dict):
            self._dict = sorted_dict
            self._keys = sorted(self._dict.keys())
            self._nr_items = len(self._keys)
            self._idx = 0

        def __iter__(self):
            return self

        def next(self):
            if self._idx >= self._nr_items:
                raise StopIteration

            key = self._keys[self._idx]
            value = self._dict[key]
            self._idx += 1

            return key, value

        __next__ = next

    def __iter__(self):
        return SortedDict.Iterator(self)

    iterkeys = __iter__


class AddressBook(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.contacts = SortedDict()
        self._old_name = ''
        self._old_address = ''

        name_label = QLabel("Name:")
        self._name_line = QLineEdit()
        self._name_line.setReadOnly(True)

        address_label = QLabel("Address:")
        self._address_text = QTextEdit()
        self._address_text.setReadOnly(True)

        self._add_button = QPushButton("&Add")
        self._submit_button = QPushButton("&Submit")
        self._submit_button.hide()
        self._cancel_button = QPushButton("&Cancel")
        self._cancel_button.hide()

        self._add_button.clicked.connect(self.add_contact)
        self._submit_button.clicked.connect(self.submit_contact)
        self._cancel_button.clicked.connect(self.cancel)

        button_layout_1 = QVBoxLayout()
        button_layout_1.addWidget(self._add_button, Qt.AlignTop)
        button_layout_1.addWidget(self._submit_button)
        button_layout_1.addWidget(self._cancel_button)
        button_layout_1.addStretch()

        main_layout = QGridLayout()
        main_layout.addWidget(name_label, 0, 0)
        main_layout.addWidget(self._name_line, 0, 1)
        main_layout.addWidget(address_label, 1, 0, Qt.AlignTop)
        main_layout.addWidget(self._address_text, 1, 1)
        main_layout.addLayout(button_layout_1, 1, 2)

        self.setLayout(main_layout)
        self.setWindowTitle("Simple Address Book")

    def add_contact(self):
        self._old_name = self._name_line.text()
        self._old_address = self._address_text.toPlainText()

        self._name_line.clear()
        self._address_text.clear()

        self._name_line.setReadOnly(False)
        self._name_line.setFocus(Qt.OtherFocusReason)
        self._address_text.setReadOnly(False)

        self._add_button.setEnabled(False)
        self._submit_button.show()
        self._cancel_button.show()

    def submit_contact(self):
        name = self._name_line.text()
        address = self._address_text.toPlainText()

        if name == "" or address == "":
            QMessageBox.information(self, "Empty Field", "Please enter a name and address.")
            return

        if name not in self.contacts:
            self.contacts[name] = address
            QMessageBox.information(self, "Add Successful",
                                    f'"{name}" has been added to your address book.')
        else:
            QMessageBox.information(self, "Add Unsuccessful",
                                    f'Sorry, "{name}" is already in your address book.')
            return

        if not self.contacts:
            self._name_line.clear()
            self._address_text.clear()

        self._name_line.setReadOnly(True)
        self._address_text.setReadOnly(True)
        self._add_button.setEnabled(True)
        self._submit_button.hide()
        self._cancel_button.hide()

    def cancel(self):
        self._name_line.setText(self._old_name)
        self._name_line.setReadOnly(True)

        self._address_text.setText(self._old_address)
        self._address_text.setReadOnly(True)

        self._add_button.setEnabled(True)
        self._submit_button.hide()
        self._cancel_button.hide()


if __name__ == '__main__':
    app = QApplication(sys.argv)

    address_book = AddressBook()
    address_book.show()

    sys.exit(app.exec())
