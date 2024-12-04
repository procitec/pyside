// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"
#include "basewrapper_p.h"
#include "sbkstring.h"
#include "sbkstaticstrings.h"
#include "sbkstaticstrings.h"
#include "pep384impl.h"

#include <algorithm>
#include <optional>

#include <iomanip>
#include <iostream>
#include <climits>
#include <cstring>
#include <cstdarg>
#include <cctype>

#ifdef _WIN32
#  include <sbkwindows.h>
#else
#  include <pthread.h>
#endif

static std::optional<std::string> getStringAttr(PyObject *obj, const char *what)
{
    if (PyObject_HasAttrString(obj, what) != 0) { // Check first to suppress error.
        Shiboken::AutoDecRef result(PyObject_GetAttrString(obj, what));
        if (PyUnicode_Check(result.object()) != 0)
            return _PepUnicode_AsString(result.object());
    }
    return std::nullopt;
}

static std::optional<int> getIntAttr(PyObject *obj, const char *what)
{
    if (PyObject_HasAttrString(obj, what) != 0) { // Check first to suppress error.
        Shiboken::AutoDecRef result(PyObject_GetAttrString(obj, what));
        if (PyLong_Check(result.object()) != 0)
            return PyLong_AsLong(result.object());
    }
    return std::nullopt;
}

static bool verbose = false;

static void formatTypeTuple(PyObject *t, const char *what, std::ostream &str);

static void formatPyTypeObject(const PyTypeObject *obj, std::ostream &str, bool verbose)
{
    if (obj == nullptr) {
        str << '0';
        return;
    }

    str << '"' << obj->tp_name << '"';
    if (verbose) {
        bool immutableType = false;
        str << ", 0x" << std::hex << obj->tp_flags << std::dec;
        if (obj->tp_flags & Py_TPFLAGS_HEAPTYPE)
            str << " [heaptype]";
        if (obj->tp_flags & Py_TPFLAGS_BASETYPE)
            str << " [base]";
        if (obj->tp_flags & Py_TPFLAGS_HAVE_GC)
            str << " [gc]";
        if (obj->tp_flags & Py_TPFLAGS_LONG_SUBCLASS)
            str << " [long]";
        if (obj->tp_flags & Py_TPFLAGS_LIST_SUBCLASS)
            str << " [list]";
        if (obj->tp_flags & Py_TPFLAGS_TUPLE_SUBCLASS)
            str << " [tuple]";
        if (obj->tp_flags & Py_TPFLAGS_BYTES_SUBCLASS)
            str << " [bytes]";
        if (obj->tp_flags & Py_TPFLAGS_UNICODE_SUBCLASS)
            str << " [unicode]";
        if (obj->tp_flags & Py_TPFLAGS_DICT_SUBCLASS)
            str << " [dict]";
        if (obj->tp_flags & Py_TPFLAGS_TYPE_SUBCLASS)
            str << " [type]";
        if (obj->tp_flags & Py_TPFLAGS_IS_ABSTRACT)
            str << " [abstract]";
        if (obj->tp_flags & Py_TPFLAGS_READY)
            str << " [ready]";
        if (obj->tp_flags & Py_TPFLAGS_READYING)
            str << " [readying]";
        if (obj->tp_flags & Py_TPFLAGS_METHOD_DESCRIPTOR)
            str << " [method_descriptor]";
#  ifndef Py_LIMITED_API
        if (obj->tp_flags & Py_TPFLAGS_HAVE_VECTORCALL)
            str << " [vectorcall]";
#  endif // !Py_LIMITED_API
#  if PY_VERSION_HEX >= 0x030A0000
        immutableType = (obj->tp_flags & Py_TPFLAGS_IMMUTABLETYPE) != 0;
        if (immutableType)
            str << " [immutabletype]";
        if (obj->tp_flags & Py_TPFLAGS_DISALLOW_INSTANTIATION)
            str << " [disallow_instantiation]";
#  ifndef Py_LIMITED_API
        if (obj->tp_flags & Py_TPFLAGS_MAPPING)
            str << " [mapping]";
        if (obj->tp_flags & Py_TPFLAGS_SEQUENCE)
            str << " [sequence]";
#       endif // !Py_LIMITED_API
#  endif // 3.10
        if (obj->tp_basicsize != 0)
            str << ", basicsize=" << obj->tp_basicsize;
        if (verbose) {
            formatTypeTuple(obj->tp_bases, "bases", str);
            formatTypeTuple(obj->tp_mro, "mro", str);
            if (!immutableType) {
                auto *underlying = reinterpret_cast<const PyObject *>(obj)->ob_type;
                if (underlying != nullptr && underlying != obj) {
                    str << ", underlying=\"" << underlying->tp_name << '"';
                }
            }
        }
    }
}

static void formatTypeTuple(PyObject *t, const char *what, std::ostream &str)
{
    const Py_ssize_t size = t != nullptr && PyTuple_Check(t) != 0 ? PyTuple_Size(t) : 0;
    if (size > 0) {
        str << ", " << what << "=[" << size << "]{";
        for (Py_ssize_t i = 0; i < size; ++i) {
            if (i != 0)
                str << ", ";
            Shiboken::AutoDecRef item(PyTuple_GetItem(t, i));
            if (item.isNull())
                str << '0'; // Observed with non-ready types
            else
                str << '"' << reinterpret_cast<PyTypeObject *>(item.object())->tp_name << '"';
        }
        str << '}';
    }
}

static void formatPyObject(PyObject *obj, std::ostream &str);

static void formatPySequence(PyObject *obj, std::ostream &str)
{
    const Py_ssize_t size = PySequence_Size(obj);
    const Py_ssize_t printSize = std::min(size, Py_ssize_t(5));
    str << size << " <";
    for (Py_ssize_t i = 0; i < printSize; ++i) {
        if (i)
            str << ", ";
        str << '(';
        Shiboken::AutoDecRef item(PySequence_GetItem(obj, i));
        formatPyObject(item, str);
        str << ')';
    }
    if (printSize < size)
        str << ",...";
    str << '>';
}

static void formatPyTuple(PyObject *obj, std::ostream &str)
{

    const Py_ssize_t size = PyTuple_Size(obj);
    str << size << " <";
    for (Py_ssize_t i = 0; i < size; ++i) {
        if (i)
            str << ", ";
        str << '(';
        PyObject *item = PyTuple_GetItem(obj, i);
        formatPyObject(item, str);
        str << ')';
        Py_XDECREF(item);
    }
    str << '>';
}

static void formatPyDict(PyObject *obj, std::ostream &str)
{
    PyObject *key;
    PyObject *value;
    Py_ssize_t pos = 0;
    str << '{';
    while (PyDict_Next(obj, &pos, &key, &value) != 0) {
        if (pos)
            str << ", ";
        str << Shiboken::debugPyObject(key) << '=' << Shiboken::debugPyObject(value);
    }
    str << '}';
}

// Helper to format a 0-terminated character sequence
template <class Char>
static void formatCharSequence(const Char *s, std::ostream &str)
{
    str << '"';
    const auto oldFillC = str.fill('0');
    str << std::hex;
    for (; *s; ++s) {
        const unsigned c = *s;
        if (c < 127)
            str << char(c);
        else
            str << "0x" << std::right << std::setw(sizeof(Char) * 2) << c << std::left;
    }
    str << std::dec;
    str.fill(oldFillC);
    str << '"';
}

static void formatPyUnicode(PyObject *obj, std::ostream &str)
{
    // Note: The below call create the PyCompactUnicodeObject.utf8 representation
    str << '"' << _PepUnicode_AsString(obj) << '"';
    if (!verbose)
        return;

    str << " (" << PyUnicode_GetLength(obj) << ')';
    const auto kind = _PepUnicode_KIND(obj);
    switch (kind) {
#if PY_VERSION_HEX < 0x030C0000
    case PepUnicode_WCHAR_KIND:
        str << " [wchar]";
        break;
#endif
    case PepUnicode_1BYTE_KIND:
        str << " [1byte]";
        break;
    case PepUnicode_2BYTE_KIND:
        str << " [2byte]";
        break;
    case PepUnicode_4BYTE_KIND:
        str << " [4byte]";
        break;
    }

    const bool ascii = _PepUnicode_IS_ASCII(obj);
    if (ascii)
        str << " [ascii]";
    const bool compact = _PepUnicode_IS_COMPACT(obj);
    if (compact)
        str << " [compact]";
    void *data =_PepUnicode_DATA(obj);
    str << ", data=";
    switch (kind) {
#if PY_VERSION_HEX < 0x030C0000
    case PepUnicode_WCHAR_KIND:
        formatCharSequence(reinterpret_cast<const wchar_t *>(data), str);
#endif
        break;
    case PepUnicode_1BYTE_KIND:
        formatCharSequence(reinterpret_cast<const Py_UCS1 *>(data), str);
        break;
    case PepUnicode_2BYTE_KIND:
        formatCharSequence(reinterpret_cast<const Py_UCS2 *>(data), str);
        break;
    case PepUnicode_4BYTE_KIND:
        formatCharSequence(reinterpret_cast<const Py_UCS4 *>(data), str);
        break;
    }

#ifndef Py_LIMITED_API
    const char *utf8 = nullptr;
    if (!ascii && compact && kind == PepUnicode_1BYTE_KIND) {
        const auto *compactObj = reinterpret_cast<const PyCompactUnicodeObject *>(obj);
        if (compactObj->utf8_length)
            utf8 = compactObj->utf8;
    }
    if (utf8) {
        str << ", utf8=";
        formatCharSequence(reinterpret_cast<const Py_UCS1 *>(utf8), str);
    } else {
        str << ", no-utf8";
    }
#endif // !Py_LIMITED_API
}

static std::string getQualName(PyObject *obj)
{
    Shiboken::AutoDecRef result(PyObject_GetAttr(obj, Shiboken::PyMagicName::qualname()));
    return result.object() != nullptr
        ? _PepUnicode_AsString(result.object()) : std::string{};
}

static void formatPyFunction(PyObject *obj, std::ostream &str)
{
    str << '"' << getQualName(obj) << "()\"";
}

static void formatPyMethod(PyObject *obj, std::ostream &str)
{
    if (auto *func = PyMethod_Function(obj))
        formatPyFunction(func, str);
    str << ", instance=" << PyMethod_Self(obj);
}

static void formatPyCodeObject(PyObject *obj, std::ostream &str)
{
    if (auto name = getStringAttr(obj, "co_name"))
        str << '"' << name.value() << '"';
    if (auto qualName = getStringAttr(obj, "co_qualname"))
        str << ", co_qualname=\"" << qualName.value() << '"';
    if (auto flags = getIntAttr(obj, "co_flags"))
        str << ", flags=0x" << std::hex << flags.value() << std::dec;
    if (auto c = getIntAttr(obj, "co_argcount"))
        str << ", co_argcounts=" << c.value();
    if (auto c = getIntAttr(obj, "co_posonlyargcount"))
        str << ", co_posonlyargcount=" << c.value();
    if (auto c = getIntAttr(obj, "co_kwonlyargcount"))
        str << ", co_kwonlyargcount=" << c.value();
    if (auto fileName = getStringAttr(obj, "co_filename")) {
        str << " @" << fileName.value();
        if (auto l = getIntAttr(obj, "co_firstlineno"))
            str << ':'<< l.value();
    }
}

static void formatPyObjectHelper(PyObject *obj, std::ostream &str)
{
    str << ", ";
    if (obj == Py_None) {
        str << "None";
        return;
    }
    if (obj == Py_True) {
        str << "True";
        return;
    }
    if (obj == Py_False) {
        str << "False";
        return;
    }
    const auto refs = Py_REFCNT(obj);
    if (refs == UINT_MAX) // _Py_IMMORTAL_REFCNT
        str << "immortal, ";
    else
        str << "refs=" << refs << ", ";
    if (PyType_Check(obj)) {
        str << "type: ";
        formatPyTypeObject(reinterpret_cast<PyTypeObject *>(obj), str, true);
        return;
    }
    formatPyTypeObject(obj->ob_type, str, false);
    str << ", ";
    if (PyLong_Check(obj)) {
        const auto llv = PyLong_AsLongLong(obj);
        if (PyErr_Occurred() != PyExc_OverflowError) {
            str << llv;
        } else {
            PyErr_Clear();
            str << "0x" << std::hex << PyLong_AsUnsignedLongLong(obj) << std::dec;
        }
    }
    else if (PyFloat_Check(obj))
        str << PyFloat_AsDouble(obj);
    else if (PyUnicode_Check(obj))
        formatPyUnicode(obj, str);
    else if (PyFunction_Check(obj) != 0)
        formatPyFunction(obj, str);
    else if (PyMethod_Check(obj) != 0)
        formatPyMethod(obj, str);
    else if (PepCode_Check(obj) != 0)
        formatPyCodeObject(obj, str);
    else if (PySequence_Check(obj))
        formatPySequence(obj, str);
    else if (PyDict_Check(obj))
        formatPyDict(obj, str);
    else if (PyTuple_CheckExact(obj))
        formatPyTuple(obj, str);
    else
        str << "<unknown>";
}

static void formatPyObject(PyObject *obj, std::ostream &str)
{
    str << obj;
    if (obj)
        formatPyObjectHelper(obj, str);
}

namespace Shiboken
{

debugPyObject::debugPyObject(PyObject *o) : m_object(o)
{
}

debugSbkObject::debugSbkObject(SbkObject *o) : m_object(o)
{
}

debugPyTypeObject::debugPyTypeObject(const PyTypeObject *o) : m_object(o)
{
}

debugPyBuffer::debugPyBuffer(const Py_buffer &b) : m_buffer(b)
{
}

std::ostream &operator<<(std::ostream &str, const debugPyTypeObject &o)
{
    str << "PyTypeObject(";
    formatPyTypeObject(o.m_object, str, true);
    str << ')';
    return str;
}

std::ostream &operator<<(std::ostream &str, const debugSbkObject &o)
{
    str << "SbkObject(" << o.m_object;
    if (o.m_object) {
        Shiboken::Object::_debugFormat(str, o.m_object);
        formatPyObjectHelper(reinterpret_cast<PyObject *>(o.m_object), str);
    }
    str << ')';
    return str;
}

std::ostream &operator<<(std::ostream &str, const debugPyObject &o)
{
    str << "PyObject(";
    formatPyObject(o.m_object, str);
    str << ')';
    return str;
}

std::ostream &operator<<(std::ostream &str, const debugPyBuffer &b)
{
    str << "PyBuffer(buf=" << b.m_buffer.buf << ", len="
        << b.m_buffer.len << ", itemsize=" << b.m_buffer.itemsize
        << ", readonly=" << b.m_buffer.readonly << ", ndim=" << b.m_buffer.ndim;
    if (b.m_buffer.format)
        str << ", format=\"" << b.m_buffer.format << '"';
    str << ", shape=" << b.m_buffer.shape << ", strides=" << b.m_buffer.strides
        << ", suboffsets=" << b.m_buffer.suboffsets << ')';
    return str;
}

std::ios_base &debugVerbose(std::ios_base &s)
{
    verbose = true;
    return s;
}

std::ios_base &debugBrief(std::ios_base &s)
{
    verbose = false;
    return s;
}

#ifdef _WIN32
// Converts a Unicode string to a string encoded in the Windows console's
// code page via wchar_t for use with argv (PYSIDE-1425).
// FIXME: Remove once Windows console uses UTF-8
static char *toWindowsConsoleEncoding(PyObject *unicode)
{
    wchar_t *buf =  PyUnicode_AsWideCharString(unicode, nullptr);
    if (buf == nullptr)
        return nullptr;
    const int required = WideCharToMultiByte(CP_ACP, 0, buf, -1,
                                             nullptr, 0, nullptr, nullptr);
    if (required == 0) {
        PyMem_Free(buf);
        return nullptr;
    }
    char *result = new char[required];
    WideCharToMultiByte(CP_ACP, 0, buf, -1,
                        result, required, nullptr, nullptr);
    PyMem_Free(buf);
    return result;
}
#endif // _WIN32

// PySide-510: Changed from PySequence to PyList, which is correct.
bool listToArgcArgv(PyObject *argList, int *argc, char ***argv, const char *defaultAppName)
{
    if (!PyList_Check(argList))
        return false;

    if (!defaultAppName)
        defaultAppName = "PySideApplication";

    // Check all items
    Shiboken::AutoDecRef args(PySequence_Fast(argList, nullptr));
    Py_ssize_t numArgs = PySequence_Size(argList);
    for (Py_ssize_t i = 0; i < numArgs; ++i) {
        PyObject *item = PyList_GET_ITEM(args.object(), i);
        if (!PyBytes_Check(item) && !PyUnicode_Check(item))
            return false;
    }

    bool hasEmptyArgList = numArgs == 0;
    if (hasEmptyArgList)
        numArgs = 1;

    *argc = numArgs;
    *argv = new char *[*argc];

    if (hasEmptyArgList) {
        // Try to get the script name
        PyObject *globals = PyEval_GetGlobals();
        PyObject *appName = PyDict_GetItem(globals, Shiboken::PyMagicName::file());
        (*argv)[0] = strdup(appName ? Shiboken::String::toCString(appName) : defaultAppName);
    } else {
        for (int i = 0; i < numArgs; ++i) {
            PyObject *item = PyList_GET_ITEM(args.object(), i);
            char *string = nullptr;
            if (Shiboken::String::check(item)) {
#ifdef _WIN32
                string = toWindowsConsoleEncoding(item);
#else
                string = strdup(Shiboken::String::toCString(item));
#endif
            }
            (*argv)[i] = string;
        }
    }

    return true;
}

int *sequenceToIntArray(PyObject *obj, bool zeroTerminated)
{
    AutoDecRef seq(PySequence_Fast(obj, "Sequence of ints expected"));
    if (seq.isNull())
        return nullptr;

    Py_ssize_t size = PySequence_Size(seq.object());
    int *array = new int[size + (zeroTerminated ? 1 : 0)];

    for (int i = 0; i < size; i++) {
        Shiboken::AutoDecRef item(PySequence_GetItem(seq.object(), i));
        if (!PyLong_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "Sequence of ints expected");
            delete[] array;
            return nullptr;
        }
        array[i] = PyLong_AsLong(item);
    }

    if (zeroTerminated)
        array[size] = 0;

    return array;
}


int warning(PyObject *category, int stacklevel, const char *format, ...)
{
    va_list args;
    va_start(args, format);
#ifdef _WIN32
    va_list args2 = args;
#else
    va_list args2;
    va_copy(args2, args);
#endif

    // check the necessary memory
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    auto message = new char[size];
    int result = 0;
    if (message) {
        // format the message
        vsnprintf(message, size, format, args2);
        result = PyErr_WarnEx(category, message, stacklevel);
        delete [] message;
    }
    va_end(args2);
    va_end(args);
    return result;
}

ThreadId currentThreadId()
{
#if defined(_WIN32)
    return GetCurrentThreadId();
#elif defined(__APPLE_CC__)
    return reinterpret_cast<ThreadId>(pthread_self());
#else
    return pthread_self();
#endif
}

// Internal, used by init() from main thread
static ThreadId _mainThreadId{0};
void _initMainThreadId() { _mainThreadId =  currentThreadId(); }

ThreadId mainThreadId()
{
    return _mainThreadId;
}

const char *typeNameOf(const char *typeIdName)
{
    auto size = std::strlen(typeIdName);
#if defined(Q_CC_MSVC) // MSVC: "class QPaintDevice * __ptr64"
    if (auto *lastStar = strchr(typeName, '*')) {
        // MSVC: "class QPaintDevice * __ptr64"
        while (*--lastStar == ' ') {
        }
        size = lastStar - typeName + 1;
    }
#else // g++, Clang: "QPaintDevice *" -> "P12QPaintDevice"
    if (size > 2 && typeIdName[0] == 'P' && std::isdigit(typeIdName[1])) {
        ++typeIdName;
        --size;
    }
#endif
    char *result = new char[size + 1];
    result[size] = '\0';
    std::memcpy(result, typeIdName, size);
    return result;
}

#if !defined(Py_LIMITED_API) && PY_VERSION_HEX >= 0x030A0000 && !defined(PYPY_VERSION)
static int _getPyVerbose()
{
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    return config.verbose;
}
#endif // !Py_LIMITED_API >= 3.10

int pyVerbose()
{
#ifdef Py_LIMITED_API
    return Pep_GetVerboseFlag();
#elif PY_VERSION_HEX >= 0x030A0000 && !defined(PYPY_VERSION)
    static const int result = _getPyVerbose();
    return result;
#else
    return Py_VerboseFlag;
#endif
}

} // namespace Shiboken
