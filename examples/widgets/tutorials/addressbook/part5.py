# Copyright (C) 2013 Riverbank Computing Limited.
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
from __future__ import annotations

import sys

from PySide6.QtCore import Qt, Slot
from PySide6.QtWidgets import (QApplication, QDialog, QGridLayout,
                               QHBoxLayout, QLabel, QLineEdit,
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
    NavigationMode, AddingMode, EditingMode = range(3)

    def __init__(self, parent=None):
        super().__init__(parent)

        self.contacts = SortedDict()
        self._old_name = ''
        self._old_address = ''
        self._current_mode = self.NavigationMode

        name_label = QLabel("Name:")
        self._name_line = QLineEdit()
        self._name_line.setReadOnly(True)

        address_label = QLabel("Address:")
        self._address_text = QTextEdit()
        self._address_text.setReadOnly(True)

        self._add_button = QPushButton("&Add")
        self._edit_button = QPushButton("&Edit")
        self._edit_button.setEnabled(False)
        self._remove_button = QPushButton("&Remove")
        self._remove_button.setEnabled(False)
        self._find_button = QPushButton("&Find")
        self._find_button.setEnabled(False)
        self._submit_button = QPushButton("&Submit")
        self._submit_button.hide()
        self._cancel_button = QPushButton("&Cancel")
        self._cancel_button.hide()

        self._next_button = QPushButton("&Next")
        self._next_button.setEnabled(False)
        self._previous_button = QPushButton("&Previous")
        self._previous_button.setEnabled(False)

        self.dialog = FindDialog()

        self._add_button.clicked.connect(self.add_contact)
        self._submit_button.clicked.connect(self.submit_contact)
        self._edit_button.clicked.connect(self.edit_contact)
        self._remove_button.clicked.connect(self.remove_contact)
        self._find_button.clicked.connect(self.find_contact)
        self._cancel_button.clicked.connect(self.cancel)
        self._next_button.clicked.connect(self.next)
        self._previous_button.clicked.connect(self.previous)

        button_layout_1 = QVBoxLayout()
        button_layout_1.addWidget(self._add_button)
        button_layout_1.addWidget(self._edit_button)
        button_layout_1.addWidget(self._remove_button)
        button_layout_1.addWidget(self._find_button)
        button_layout_1.addWidget(self._submit_button)
        button_layout_1.addWidget(self._cancel_button)
        button_layout_1.addStretch()

        button_layout_2 = QHBoxLayout()
        button_layout_2.addWidget(self._previous_button)
        button_layout_2.addWidget(self._next_button)

        main_layout = QGridLayout()
        main_layout.addWidget(name_label, 0, 0)
        main_layout.addWidget(self._name_line, 0, 1)
        main_layout.addWidget(address_label, 1, 0, Qt.AlignTop)
        main_layout.addWidget(self._address_text, 1, 1)
        main_layout.addLayout(button_layout_1, 1, 2)
        main_layout.addLayout(button_layout_2, 2, 1)

        self.setLayout(main_layout)
        self.setWindowTitle("Simple Address Book")

    @Slot()
    def add_contact(self):
        self._old_name = self._name_line.text()
        self._old_address = self._address_text.toPlainText()

        self._name_line.clear()
        self._address_text.clear()

        self.update_interface(self.AddingMode)

    @Slot()
    def edit_contact(self):
        self._old_name = self._name_line.text()
        self._old_address = self._address_text.toPlainText()

        self.update_interface(self.EditingMode)

    @Slot()
    def submit_contact(self):
        name = self._name_line.text()
        address = self._address_text.toPlainText()

        if name == "" or address == "":
            QMessageBox.information(self, "Empty Field", "Please enter a name and address.")
            return

        if self._current_mode == self.AddingMode:
            if name not in self.contacts:
                self.contacts[name] = address
                QMessageBox.information(self, "Add Successful",
                                        f'"{name}" has been added to your address book.')
            else:
                QMessageBox.information(self, "Add Unsuccessful",
                                        f'Sorry, "{name}" is already in your address book.')
                return

        elif self._current_mode == self.EditingMode:
            if self._old_name != name:
                if name not in self.contacts:
                    QMessageBox.information(self, "Edit Successful",
                                            f'"{self.oldName}" has been edited in your '
                                            'address book.')
                    del self.contacts[self._old_name]
                    self.contacts[name] = address
                else:
                    QMessageBox.information(self, "Edit Unsuccessful",
                                            f'Sorry, "{name}" is already in your address book.')
                    return
            elif self._old_address != address:
                QMessageBox.information(self, "Edit Successful",
                                        f'"{name}" has been edited in your address book.')
                self.contacts[name] = address

        self.update_interface(self.NavigationMode)

    @Slot()
    def cancel(self):
        self._name_line.setText(self._old_name)
        self._address_text.setText(self._old_address)
        self.update_interface(self.NavigationMode)

    @Slot()
    def remove_contact(self):
        name = self._name_line.text()

        if name in self.contacts:
            button = QMessageBox.question(self, "Confirm Remove",
                                          f'Are you sure you want to remove "{name}"?',
                                          QMessageBox.Yes | QMessageBox.No)

            if button == QMessageBox.Yes:
                self.previous()
                del self.contacts[name]

                QMessageBox.information(self, "Remove Successful",
                                        f'"{name}" has been removed from your address book.')

        self.update_interface(self.NavigationMode)

    @Slot()
    def next(self):
        name = self._name_line.text()
        it = iter(self.contacts)

        try:
            while True:
                this_name, _ = it.next()

                if this_name == name:
                    next_name, next_address = it.next()
                    break
        except StopIteration:
            next_name, next_address = iter(self.contacts).next()

        self._name_line.setText(next_name)
        self._address_text.setText(next_address)

    @Slot()
    def previous(self):
        name = self._name_line.text()

        prev_name = prev_address = None
        for this_name, this_address in self.contacts:
            if this_name == name:
                break

            prev_name = this_name
            prev_address = this_address
        else:
            self._name_line.clear()
            self._address_text.clear()
            return

        if prev_name is None:
            for prev_name, prev_address in self.contacts:
                pass

        self._name_line.setText(prev_name)
        self._address_text.setText(prev_address)

    def find_contact(self):
        self.dialog.show()

        if self.dialog.exec() == QDialog.Accepted:
            contact_name = self.dialog.get_find_text()

            if contact_name in self.contacts:
                self._name_line.setText(contact_name)
                self._address_text.setText(self.contacts[contact_name])
            else:
                QMessageBox.information(self, "Contact Not Found",
                                        f'Sorry, "{contact_name}" is not in your address book.')
                return

        self.update_interface(self.NavigationMode)

    def update_interface(self, mode):
        self._current_mode = mode

        if self._current_mode in (self.AddingMode, self.EditingMode):
            self._name_line.setReadOnly(False)
            self._name_line.setFocus(Qt.OtherFocusReason)
            self._address_text.setReadOnly(False)

            self._add_button.setEnabled(False)
            self._edit_button.setEnabled(False)
            self._remove_button.setEnabled(False)

            self._next_button.setEnabled(False)
            self._previous_button.setEnabled(False)

            self._submit_button.show()
            self._cancel_button.show()

        elif self._current_mode == self.NavigationMode:
            if not self.contacts:
                self._name_line.clear()
                self._address_text.clear()

            self._name_line.setReadOnly(True)
            self._address_text.setReadOnly(True)
            self._add_button.setEnabled(True)

            number = len(self.contacts)
            self._edit_button.setEnabled(number >= 1)
            self._remove_button.setEnabled(number >= 1)
            self._find_button.setEnabled(number > 2)
            self._next_button.setEnabled(number > 1)
            self._previous_button.setEnabled(number > 1)

            self._submit_button.hide()
            self._cancel_button.hide()


class FindDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)

        find_label = QLabel("Enter the name of a contact:")
        self._line_edit = QLineEdit()

        self._find_button = QPushButton("&Find")
        self._find_text = ''

        layout = QHBoxLayout()
        layout.addWidget(find_label)
        layout.addWidget(self._line_edit)
        layout.addWidget(self._find_button)

        self.setLayout(layout)
        self.setWindowTitle("Find a Contact")

        self._find_button.clicked.connect(self.find_clicked)
        self._find_button.clicked.connect(self.accept)

    def find_clicked(self):
        text = self._line_edit.text()

        if not text:
            QMessageBox.information(self, "Empty Field", "Please enter a name.")
            return
        else:
            self._find_text = text
            self._line_edit.clear()
            self.hide()

    def get_find_text(self):
        return self._find_text


if __name__ == '__main__':
    app = QApplication(sys.argv)

    address_book = AddressBook()
    address_book.show()

    sys.exit(app.exec())
