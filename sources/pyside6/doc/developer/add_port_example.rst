.. _developer-add-port-example:

Add a new example or port one
=============================

Adding examples is a good exercise for people wanting to become familiar with
the modules and its functionality.

You can either design an example from scratch or inspired in another
application, or simply you can port an existing Qt example that does not have
a Python counterpart.

Example code should be free of `flake8 <https://pypi.org/project/flake8/>`_
warnings; this is enforced by a bot. A configuration file is provided
at the root of the repository. Offending lines can be excluded by a
``noqa`` directive if there is a good reason to do so.

Keep in mind we do allow 100 columns for line length.

Additionally, please use `isort <https://pypi.org/project/isort/>`_ to keep the
imports ordered and consistent with other examples.

For example:

.. code-block:: bash

  $ flake8 --config pyside-setup/.flake8 your_file.py
  $ isort your_file.py



Add a new example
-----------------

- Check if the topic your example covers is not in an existing example already.
- Create a new directory inside the ``examples/<module>`` you think
  is more relevant.
- Inside, place the code of the example, and also a ``.pyproject``
  file listing the files the example needs.
- If you want the example to be automatically displayed on the
  example gallery, include a ``doc`` directory that contains a ``rst``
  file and a screenshot. Check other examples for formatting questions.
- When writing the ``rst`` file, you can include code snippets using
  the ``literalinclude`` directive specifying the relative path
  as listed in the ``.pyproject`` file. The `example_gallery` tool will
  expand this (see the `pointconfiguration` example).
- For the code displayed in the tabs, you can create ``rstinc`` files
  in the ``doc`` directory containing some description explaining them
  (see the `samplebinding` example).

.. note:: The ``example_gallery`` tools automatically prepends an
          auto-generated anchor to the example documentation, please use those for
          linking to the documentation.

Port a Qt example
-----------------

- Quickly check the C++ example, fix outdated code.
- Port the sources using ``tools/tools/qtcpp2py.py`` (front-end for
  ``snippets-translate``).
- Note that our examples need to have unique names due to the doc build.
- Verify that all slots are decorated using ``@Slot``.
- Add a ``.pyproject`` file (verify later on that docs build).
- Add a ``doc`` directory and descriptive ``.rst`` file,
  and a screenshot if suitable (use ``optipng`` to reduce file size).
- Add the ``"""Port of the ... example from Qt 6"""`` doc string.
- Try to port variable and function names to snake case convention.
- Remove C++ documentation from ``sources/pyside6/doc/additionaldocs.lst``.

.. note:: Example screenshots in ``.png`` should be optimized by
          running ``optipng -o 7 -strip all``. Alternatively, the ``.webp`` format can be used.
