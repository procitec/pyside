.. _user-defined-type-conversion:

****************************
User Defined Type Conversion
****************************

In the process of creating Python bindings of a C++ library, most of the C++
classes will have wrappers representing them in Python land.
But there may be other classes that are very simple and/or have a Python type
as a direct counter part. (Example: a "Complex" class, that represents complex
numbers, has a Python equivalent in the "complex" type.) Such classes, instead
of getting a Python wrapper, normally have conversions rules, from Python to
C++ and vice-versa.

.. code-block:: c++

    // C++ class
    struct Complex {
        Complex(double real, double imag);
        double real() const;
        double imag() const;
    };

    // Converting from C++ to Python using the CPython API:
    PyObject* pyCpxObj = PyComplex_FromDoubles(complex.real(), complex.imag());

    // Converting from Python to C++:
    double real = PyComplex_RealAsDouble(pyCpxObj);
    double imag = PyComplex_ImagAsDouble(pyCpxObj);
    Complex cpx(real, imag);


For the user defined conversion code to be inserted in the proper places,
the :ref:`conversion-rule` tag must be used.

.. code-block:: xml

  <primitive-type name="Complex" target-lang-api-name="PyComplex">
    <include file-name="complex.h" location="global"/>

    <conversion-rule>

      <native-to-target>
      return PyComplex_FromDoubles(%in.real(), %in.imag());
      </native-to-target>

      <target-to-native>
        <!-- The 'check' attribute can be derived from the 'type' attribute,
             it is defined here to test the CHECKTYPE type system variable. -->
        <add-conversion type="PyComplex" check="%CHECKTYPE[Complex](%in)">
        double real = PyComplex_RealAsDouble(%in);
        double imag = PyComplex_ImagAsDouble(%in);
        %out = %OUTTYPE(real, imag);
        </add-conversion>
      </target-to-native>

    </conversion-rule>

  </primitive-type>


The details will be given later, but the gist of it are the tags
:ref:`native-to-target <native-to-target>`, which has only one conversion from C++ to Python, and
:ref:`native-to-native <target-to-native>`, that may define the conversion of multiple Python types
to C++'s "Complex" type.

.. image:: images/converter.png
    :height: 240px
    :align: center

|project| expects the code for :ref:`native-to-target <native-to-target>`, to directly return the
Python result of the conversion, and the added conversions inside the
:ref:`target-to-native <target-to-native>` must attribute the Python to C++ conversion result to
the :ref:`%out <out>` variable.

Expanding on the last example, if the binding developer want a Python 2-tuple
of numbers to be accepted by wrapped C++ functions with "Complex" arguments,
an :ref:`add-conversion <add-conversion>` tag and a custom check must be added.
Here's how to do it:

.. code-block:: xml

  <!-- Code injection at module level. -->
  <inject-code class="native" position="beginning">
  static bool Check2TupleOfNumbers(PyObject* pyIn) {
      if (!PySequence_Check(pyIn) || !(PySequence_Size(pyIn) == 2))
          return false;
      Shiboken::AutoDecRef pyReal(PySequence_GetItem(pyIn, 0));
      if (!PyNumber_Check(pyReal))
          return false;
      Shiboken::AutoDecRef pyImag(PySequence_GetItem(pyIn, 1));
      if (!PyNumber_Check(pyImag))
          return false;
      return true;
  }
  </inject-code>

  <primitive-type name="Complex" target-lang-api-name="PyComplex">
    <include file-name="complex.h" location="global"/>

    <conversion-rule>

      <native-to-target>
      return PyComplex_FromDoubles(%in.real(), %in.imag());
      </native-to-target>

      <target-to-native>

        <add-conversion type="PyComplex">
        double real = PyComplex_RealAsDouble(%in);
        double imag = PyComplex_ImagAsDouble(%in);
        %out = %OUTTYPE(real, imag);
        </add-conversion>

        <add-conversion type="PySequence" check="Check2TupleOfNumbers(%in)">
        Shiboken::AutoDecRef pyReal(PySequence_GetItem(%in, 0));
        Shiboken::AutoDecRef pyImag(PySequence_GetItem(%in, 1));
        double real = %CONVERTTOCPP[double](pyReal);
        double imag  = %CONVERTTOCPP[double](pyImag);
        %out = %OUTTYPE(real, imag);
        </add-conversion>

      </target-to-native>

    </conversion-rule>

  </primitive-type>


.. _container_conversions:

Container Conversions
=====================

Converters for :ref:`container-type <container-type>` are pretty much the same as for other type,
except that they make use of the type system variables
:ref:`%INTYPE_# <intype_n>` and :ref:`%OUTTYPE_# <outtype_n>`.
|project| combines the conversion code for containers with the conversion
defined (or automatically generated) for the containers.

.. code-block:: xml

      <container-type name="std::map" type="map">
        <include file-name="map" location="global"/>

        <conversion-rule>

          <native-to-target>
          PyObject* %out = PyDict_New();
          %INTYPE::const_iterator it = %in.begin();
          for (; it != %in.end(); ++it) {
            %INTYPE_0 key = it->first;
            %INTYPE_1 value = it->second;
                    PyDict_SetItem(%out,
                           %CONVERTTOPYTHON[%INTYPE_0](key),
                   %CONVERTTOPYTHON[%INTYPE_1](value));
          }
          return %out;
          </native-to-target>

          <target-to-native>

            <add-conversion type="PyDict">
            PyObject* key;
            PyObject* value;
            Py_ssize_t pos = 0;
            while (PyDict_Next(%in, &amp;pos, &amp;key, &amp;value)) {
                %OUTTYPE_0 cppKey = %CONVERTTOCPP[%OUTTYPE_0](key);
                %OUTTYPE_1 cppValue = %CONVERTTOCPP[%OUTTYPE_1](value);
                %out.insert(%OUTTYPE::value_type(cppKey, cppValue));
            }
            </add-conversion>

          </target-to-native>
        </conversion-rule>
      </container-type>

.. note:: The C++ containers ``std::list``\, ``std::vector``\,
          ``std::pair``\, ``std::map``\, ``std::span`` and ``std::unordered_map`` are
          built-in.
          To specify :ref:`opaque-containers`, use the :ref:`opaque-container` element.
          :ref:`container-type` can still be specified to modify the built-in behavior.
          For this case, a number of pre-defined conversion templates
          are provided (see :ref:`predefined_templates`).

.. _variables_and_functions:

Variables & Functions
=====================


.. _in:

**%in**
  Variable replaced by the C++ input variable.


.. _out:

**%out**
  Variable replaced by the C++ output variable. Needed to convey the
  result of a Python to C++ conversion.


.. _intype:

**%INTYPE**
  Used in Python to C++ conversions. It is replaced by the name of type for
  which the conversion is being defined. Don't use the type's name directly.


.. _intype_n:

**%INTYPE_#**
  Replaced by the name of the #th type used in a container.


.. _outtype:

**%OUTTYPE**
  Used in Python to C++ conversions. It is replaced by the name of type for
  which the conversion is being defined. Don't use the type's name directly.


.. _outtype_n:

**%OUTTYPE_#**
  Replaced by the name of the #th type used in a container.


.. _checktype:

**%CHECKTYPE[CPPTYPE]**
  Replaced by a |project| type checking function for a Python variable.
  The C++ type is indicated by ``CPPTYPE``.
