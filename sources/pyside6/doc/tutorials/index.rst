.. _pyside6_tutorials:

Tutorials
=========

A collection of tutorials with walkthrough guides are
provided with |project| to help new users get started.

Some of these documents were ported from C++ to Python and cover a range of
topics, from basic use of widgets to step-by-step tutorials that show how an
application is put together.

Qt Widgets: Basic tutorials
---------------------------

If you want to see the available widgets in action, you can check the
`Qt Widget Gallery <https://doc.qt.io/qt-6/gallery.html>`_ to learn their
names and how they look like.

.. grid:: 1 3 3 3
    :gutter: 2

    .. grid-item-card:: Basic Widget
        :class-item: cover-img
        :link: tutorial_widgets
        :link-type: ref
        :img-top: basictutorial/widgets.png

        Your first QtWidgets Application

    .. grid-item-card:: Basic Button
        :class-item: cover-img
        :link: tutorial_clickablebutton
        :link-type: ref
        :img-top: basictutorial/clickablebutton.png

        Using a Simple Button

    .. grid-item-card:: Basic Connections
        :class-item: cover-img
        :link: tutorial_signals_and_slots
        :link-type: ref
        :img-top: basictutorial/signals_slots.png

        Signals and Slots

    .. grid-item-card:: Basic Dialog
        :class-item: cover-img
        :link: tutorial_dialog
        :link-type: ref
        :img-top: basictutorial/dialog.png

        Creating a Dialog Application

    .. grid-item-card:: Basic Table
        :class-item: cover-img
        :link: tutorial_tablewidget
        :link-type: ref
        :img-top: basictutorial/tablewidget.png

        Displaying Data Using a Table Widget

    .. grid-item-card:: Basic Tree
        :class-item: cover-img
        :link: tutorial_treewidget
        :link-type: ref
        :img-top: basictutorial/treewidget.png

        Displaying Data Using a Tree Widget

    .. grid-item-card:: Basic ``ui`` files
        :class-item: cover-img
        :link: tutorial_uifiles
        :link-type: ref
        :img-top: basictutorial/uifiles.png

        Using .ui files from Designer or QtCreator with QUiLoader and pyside6-uic

    .. grid-item-card:: Basic ``qrc`` files
        :class-item: cover-img
        :link: tutorial_qrcfiles
        :link-type: ref
        :img-top: basictutorial/player-new.png

        Using .qrc Files (pyside6-rcc)

    .. grid-item-card:: Basic Translations
        :class-item: cover-img
        :link: tutorial_translations
        :link-type: ref
        :img-top: basictutorial/translations.png

        Translating Applications

    .. grid-item-card:: Basic Widget Style
        :class-item: cover-img
        :link: tutorial_widgetstyling
        :link-type: ref
        :img-top: basictutorial/widgetstyling-yes.png

        Styling the Widgets Application

.. toctree::
    :hidden:

    basictutorial/widgets.rst
    basictutorial/clickablebutton.rst
    basictutorial/signals_and_slots.rst
    basictutorial/dialog.rst
    basictutorial/tablewidget.rst
    basictutorial/treewidget.rst
    basictutorial/uifiles.rst
    basictutorial/qrcfiles.rst
    basictutorial/translations.rst
    basictutorial/widgetstyling.rst


Quick/QML: Basic tutorials
--------------------------

.. grid:: 1 3 3 3
    :gutter: 2

    .. grid-item-card:: Basic Quick
        :class-item: cover-img
        :link: tutorial_qml
        :link-type: ref
        :img-top: basictutorial/greenapplication.png

        Your First QtQuick/QML Application

    .. grid-item-card:: Basic QML Integration
        :class-item: cover-img
        :link: tutorial_qmlintegration
        :link-type: ref
        :img-top: qmlintegration/textproperties_material.png

        Python-QML integration

    .. grid-item-card:: QML Application
        :class-item: cover-img
        :link: tutorial_qmlapplication
        :link-type: ref
        :img-top: qmlapp/qmlapplication.png

        QML Application Tutorial (QtCreator)

    .. grid-item-card:: Advanced QML Integration
        :class-item: cover-img
        :link: tutorial_qmlsqlintegration
        :link-type: ref
        :img-top: qmlsqlintegration/example_list_view.png

        QML, SQL and PySide Integration Tutorial

    .. grid-item-card:: Extended Explorer
        :class-item: cover-img
        :link: tutorial_extendedexplorer
        :link-type: ref
        :img-top: extendedexplorer/resources/extendedexplorer.webp

        Extending an Qt Quick Controls example

    .. grid-item-card:: Finance Manager Tutorial
        :class-item: cover-img
        :link: tutorial_financemanager
        :link-type: ref
        :img-top: finance_manager/part1/resources/finance_manager.webp

        Finance Manager Tutorial using QtQuick, SQLAlchemy, and FastAPI demonstrating how
        PySide6 can be used to interact with other popular packages in the Python ecosystem.

.. toctree::
    :maxdepth: 1
    :hidden:

    basictutorial/qml.rst
    qmlintegration/qmlintegration.rstsourc
    qmlapp/qmlapplication.rst
    qmlsqlintegration/qmlsqlintegration.rst
    extendedexplorer/extendedexplorer.md
    finance_manager/index.md

General Applications
--------------------

.. grid:: 1 3 3 3
    :gutter: 2

    .. grid-item-card:: Data Visualization
        :class-item: cover-img
        :link: datavisualize_index
        :link-type: ref
        :img-top: datavisualize/images/datavisualization_app.png

        Data Visualization Tool

    .. grid-item-card:: Expenses Application
        :class-item: cover-img
        :link: tutorial_expenses
        :link-type: ref
        :img-top: expenses/expenses_tool.png

        Expenses administration tool

.. toctree::
    :hidden:

    datavisualize/index.rst
    expenses/expenses.rst

Qt Overviews
------------

.. toctree::
    :maxdepth: 1

    ../overviews/overviews-main.rst

C++ and Python
--------------

.. toctree::
    :maxdepth: 1

    portingguide/index.rst

Debug a PySide6 Application
---------------------------
.. toctree::
    :maxdepth: 1

    debugging/mixed_debugging.rst
    debugging/qml_debugging.rst

