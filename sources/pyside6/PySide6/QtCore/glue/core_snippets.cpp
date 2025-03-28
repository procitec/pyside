// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core_snippets_p.h"
#include "qtcorehelper.h"
#include "pysideqobject.h"

#include "shiboken.h"
#ifndef Py_LIMITED_API
#  include <datetime.h>
#endif
#include "basewrapper.h"
#include "autodecref.h"
#include "pysideutils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QStack>
#include <QtCore/QVariant>

// Helpers for QVariant conversion

QMetaType QVariant_resolveMetaType(PyTypeObject *type)
{
    if (!PyObject_TypeCheck(type, SbkObjectType_TypeF()))
        return {};
    const char *typeName = Shiboken::ObjectType::getOriginalName(type);
    if (!typeName)
        return {};
    const bool valueType = '*' != typeName[qstrlen(typeName) - 1];
    // Do not convert user type of value
    if (valueType && Shiboken::ObjectType::isUserType(type))
        return {};
    QMetaType metaType = QMetaType::fromName(typeName);
    if (metaType.isValid())
        return metaType;
    // Do not resolve types to value type
    if (valueType)
        return {};
    // Find in base types. First check tp_bases, and only after check tp_base, because
    // tp_base does not always point to the first base class, but rather to the first
    // that has added any python fields or slots to its object layout.
    // See https://mail.python.org/pipermail/python-list/2009-January/520733.html
    if (type->tp_bases) {
        for (Py_ssize_t i = 0, size = PyTuple_Size(type->tp_bases); i < size; ++i) {
            auto baseType = reinterpret_cast<PyTypeObject *>(PyTuple_GetItem(type->tp_bases, i));
            const QMetaType derived = QVariant_resolveMetaType(baseType);
            if (derived.isValid())
                return derived;
        }
    } else if (type->tp_base) {
        return QVariant_resolveMetaType(type->tp_base);
    }
    return {};
}

QVariant QVariant_convertToValueList(PyObject *list)
{
    if (PySequence_Size(list) < 0) {
        // clear the error if < 0 which means no length at all
        PyErr_Clear();
        return {};
    }

    Shiboken::AutoDecRef element(PySequence_GetItem(list, 0));

    auto *type = reinterpret_cast<PyTypeObject *>(element.object());
    QMetaType metaType = QVariant_resolveMetaType(type);
    if (!metaType.isValid())
        return {};

    const QByteArray listTypeName = QByteArrayLiteral("QList<") + metaType.name() + '>';
    metaType = QMetaType::fromName(listTypeName);
    if (!metaType.isValid())
        return {};

    Shiboken::Conversions::SpecificConverter converter(listTypeName);
    if (!converter) {
        qWarning("Type converter for: %s not registered.", listTypeName.constData());
        return {};
    }

    QVariant var(metaType);
    converter.toCpp(list, &var);
    return var;
}

bool QVariant_isStringList(PyObject *list)
{
    if (!PySequence_Check(list)) {
        // If it is not a list or a derived list class
        // we assume that will not be a String list neither.
        return false;
    }

    if (PySequence_Size(list) < 0) {
        // clear the error if < 0 which means no length at all
        PyErr_Clear();
        return false;
    }

    Shiboken::AutoDecRef fast(PySequence_Fast(list, "Failed to convert QVariantList"));
    const Py_ssize_t size = PySequence_Size(fast.object());
    for (Py_ssize_t i = 0; i < size; ++i) {
        Shiboken::AutoDecRef item(PySequence_GetItem(fast.object(), i));
        if (PyUnicode_Check(item) == 0)
            return false;
    }
    return true;
}

// Helpers for qAddPostRoutine

namespace PySide {

static QStack<PyObject *> globalPostRoutineFunctions;

void globalPostRoutineCallback()
{
    Shiboken::GilState state;
    for (auto *callback : globalPostRoutineFunctions) {
        Shiboken::AutoDecRef result(PyObject_CallObject(callback, nullptr));
        Py_DECREF(callback);
    }
    globalPostRoutineFunctions.clear();
}

void addPostRoutine(PyObject *callback)
{
    if (PyCallable_Check(callback)) {
        globalPostRoutineFunctions << callback;
        Py_INCREF(callback);
    } else {
        PyErr_SetString(PyExc_TypeError, "qAddPostRoutine: The argument must be a callable object.");
    }
}
} // namespace PySide

// Helpers for QObject::findChild(ren)()

static bool _findChildTypeMatch(const QObject *child, PyTypeObject *desiredType)
{
    auto *pyChildType = PySide::getTypeForQObject(child);
    return pyChildType != nullptr && PyType_IsSubtype(pyChildType, desiredType);
}

static inline bool _findChildrenComparator(const QObject *child,
                                           const QRegularExpression &name)
{
    return name.match(child->objectName()).hasMatch();
}

static inline bool _findChildrenComparator(const QObject *child,
                                           const QString &name)
{
    return name.isNull() || name == child->objectName();
}

QObject *qObjectFindChild(const QObject *parent, const QString &name,
                          PyTypeObject *desiredType, Qt::FindChildOptions options)
{
    for (auto *child : parent->children()) {
        if (_findChildrenComparator(child, name)
            && _findChildTypeMatch(child, desiredType)) {
            return child;
        }
    }

    if (options.testFlag(Qt::FindChildrenRecursively)) {
        for (auto *child : parent->children()) {
            if (auto *obj = qObjectFindChild(child, name, desiredType, options))
                return obj;
        }
    }
    return nullptr;
}

template<typename T> // QString/QRegularExpression
static void _findChildrenHelper(const QObject *parent, const T& name, PyTypeObject *desiredType,
                                Qt::FindChildOptions options, FindChildHandler handler)
{
    for (auto *child : parent->children()) {
        if (_findChildrenComparator(child, name) && _findChildTypeMatch(child, desiredType))
            handler(child);
        if (options.testFlag(Qt::FindChildrenRecursively))
            _findChildrenHelper(child, name, desiredType, options, handler);
    }
}

void qObjectFindChildren(const QObject *parent, const QString &name,
                         PyTypeObject *desiredType, Qt::FindChildOptions options,
                         FindChildHandler handler)
{
    _findChildrenHelper(parent, name, desiredType, options, handler);
}

void qObjectFindChildren(const QObject *parent, const QRegularExpression &pattern,
                         PyTypeObject *desiredType, Qt::FindChildOptions options,
                         FindChildHandler handler)
{
    _findChildrenHelper(parent, pattern, desiredType, options, handler);
}

//////////////////////////////////////////////////////////////////////////////
// Helpers for translation:
// PYSIDE-131: Use the class name as context where the calling function is
//             living. Derived Python classes have the wrong context.
//
// The original patch uses Python introspection to look up the current
// function (from the frame stack) in the class __dict__ along the mro.
//
// The problem is that looking into the frame stack works for Python
// functions, only. For including builtin function callers, the following
// approach turned out to be much simpler:
//
// Walk the __mro__
// - translate the string
// - if the translated string is changed:
//   - return the translation.

QString qObjectTr(PyTypeObject *type, const char *sourceText, const char *disambiguation, int n)
{
    PyObject *mro = type->tp_mro;
    auto len = PyTuple_Size(mro);
    QString result = QString::fromUtf8(sourceText);
    QString oldResult = result;
    static auto *sbkObjectType = reinterpret_cast<PyTypeObject *>(SbkObject_TypeF());
    for (Py_ssize_t idx = 0; idx < len - 1; ++idx) {
        // Skip the last class which is `object`.
        auto *type = reinterpret_cast<PyTypeObject *>(PyTuple_GetItem(mro, idx));
        if (type == sbkObjectType)
            continue;
        const char *context = type->tp_name;
        const char *dotpos = strrchr(context, '.');
        if (dotpos != nullptr)
            context = dotpos + 1;
        result = QCoreApplication::translate(context, sourceText, disambiguation, n);
        if (result != oldResult)
            break;
    }
    return result;
}

bool PyDate_ImportAndCheck(PyObject *pyIn)
{
    if (!PyDateTimeAPI)
        PyDateTime_IMPORT;
    return PyDate_Check(pyIn);
}

bool PyDateTime_ImportAndCheck(PyObject *pyIn)
{
    if (!PyDateTimeAPI)
        PyDateTime_IMPORT;
    return PyDateTime_Check(pyIn);
}

bool PyTime_ImportAndCheck(PyObject *pyIn)
{
    if (!PyDateTimeAPI)
        PyDateTime_IMPORT;
    return PyTime_Check(pyIn);
}

PyObject *invokeMetaMethod(const InvokeMetaMethodFunc &f,
                           const QtCoreHelper::QGenericArgumentHolder &a0,
                           const QtCoreHelper::QGenericArgumentHolder &a1,
                           const QtCoreHelper::QGenericArgumentHolder &a2,
                           const QtCoreHelper::QGenericArgumentHolder &a3,
                           const QtCoreHelper::QGenericArgumentHolder &a4,
                           const QtCoreHelper::QGenericArgumentHolder &a5,
                           const QtCoreHelper::QGenericArgumentHolder &a6,
                           const QtCoreHelper::QGenericArgumentHolder &a7,
                           const QtCoreHelper::QGenericArgumentHolder &a8,
                           const QtCoreHelper::QGenericArgumentHolder &a9)
{
    PyThreadState *_save = PyEval_SaveThread(); // Py_BEGIN_ALLOW_THREADS
    const bool resultB = f(a0.toGenericArgument(), a1.toGenericArgument(), a2.toGenericArgument(),
                           a3.toGenericArgument(), a4.toGenericArgument(), a5.toGenericArgument(),
                           a6.toGenericArgument(), a7.toGenericArgument(), a8.toGenericArgument(),
                           a9.toGenericArgument());
    PyEval_RestoreThread(_save); // Py_END_ALLOW_THREADS
    PyObject *result = resultB ? Py_True : Py_False;
    Py_INCREF(result);
    return result;
}

// Convert a QGenericReturnArgument to Python for QMetaObject::invokeMethod
static PyObject *convertGenericReturnArgument(const void *retData, QMetaType metaType)
{
    PyObject *result = nullptr;
    switch (metaType.id()) {
    case QMetaType::Bool:
        result = *reinterpret_cast<const bool *>(retData) ? Py_True : Py_False;
        Py_INCREF(result);
        break;
    case QMetaType::Int:
        result = PyLong_FromLong(*reinterpret_cast<const int *>(retData));
        break;
    case QMetaType::Double:
        result = PyFloat_FromDouble(*reinterpret_cast<const double *>(retData));
        break;
    case QMetaType::QString:
        result = PySide::qStringToPyUnicode(*reinterpret_cast<const QString *>(retData));
        break;
    default: {
        Shiboken::Conversions::SpecificConverter converter(metaType.name());
        const auto type = converter.conversionType();
        if (type == Shiboken::Conversions::SpecificConverter::InvalidConversion) {
            PyErr_Format(PyExc_RuntimeError, "%s: Unable to find converter for \"%s\".",
                         __FUNCTION__, metaType.name());
            return nullptr;
        }
        result = converter.toPython(retData);
    }
    }
    return result;
}

PyObject *invokeMetaMethodWithReturn(const InvokeMetaMethodFuncWithReturn &f,
                                     const QtCoreHelper::QGenericReturnArgumentHolder &r,
                                     const QtCoreHelper::QGenericArgumentHolder &a0,
                                     const QtCoreHelper::QGenericArgumentHolder &a1,
                                     const QtCoreHelper::QGenericArgumentHolder &a2,
                                     const QtCoreHelper::QGenericArgumentHolder &a3,
                                     const QtCoreHelper::QGenericArgumentHolder &a4,
                                     const QtCoreHelper::QGenericArgumentHolder &a5,
                                     const QtCoreHelper::QGenericArgumentHolder &a6,
                                     const QtCoreHelper::QGenericArgumentHolder &a7,
                                     const QtCoreHelper::QGenericArgumentHolder &a8,
                                     const QtCoreHelper::QGenericArgumentHolder &a9)
{
    PyThreadState *_save = PyEval_SaveThread(); // Py_BEGIN_ALLOW_THREADS
    const bool callResult = f(r.toGenericReturnArgument(),
                              a0.toGenericArgument(), a1.toGenericArgument(), a2.toGenericArgument(),
                              a3.toGenericArgument(), a4.toGenericArgument(), a5.toGenericArgument(),
                              a6.toGenericArgument(), a7.toGenericArgument(), a8.toGenericArgument(),
                              a9.toGenericArgument());
    PyEval_RestoreThread(_save); // Py_END_ALLOW_THREADS
    if (!callResult)
        return PyErr_Format(PyExc_RuntimeError, "QMetaMethod invocation failed.");
    return convertGenericReturnArgument(r.data(), r.metaType());
}
