// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "basewrapper.h"
#include "basewrapper_p.h"
#include "autodecref.h"
#include "pep384ext.h"
#include "sbkenum.h"
#include "sbkstring.h"
#include "sbkstaticstrings.h"
#include "sbkstaticstrings_p.h"
#include "signature.h"
#include "sbkfeature_base.h"
#include "gilstate.h"

#include <cctype>

using namespace Shiboken;

extern "C"
{

////////////////////////////////////////////////////////////////////////////
//
// Minimal __feature__ support in Shiboken
//
int currentSelectId(PyTypeObject *type)
{
    AutoDecRef tpDict(PepType_GetDict(type));
    PyObject *PyId = PyObject_GetAttr(tpDict.object(), PyName::select_id());
    if (PyId == nullptr) {
        PyErr_Clear();
        return 0x00;
    }
    int sel = PyLong_AsLong(PyId);
    Py_DECREF(PyId);
    return sel;
}

static SelectableFeatureHook SelectFeatureSet = nullptr;
static SelectableFeatureCallback featureCb = nullptr;

void setSelectableFeatureCallback(SelectableFeatureCallback func)
{
    featureCb = func;
}

SelectableFeatureHook initSelectableFeature(SelectableFeatureHook func)
{
    auto ret = SelectFeatureSet;
    SelectFeatureSet = func;
    if (featureCb)
        featureCb(SelectFeatureSet != nullptr);
    return ret;
}
//
////////////////////////////////////////////////////////////////////////////

// This useful function is for debugging
void disassembleFrame(const char *marker)
{
    Shiboken::GilState gil;
    PyObject *error_type, *error_value, *error_traceback;
    PyErr_Fetch(&error_type, &error_value, &error_traceback);
    static PyObject *dismodule = PyImport_ImportModule("dis");
    static PyObject *disco = PyObject_GetAttrString(dismodule, "disco");
    static PyObject *const _f_lasti = Shiboken::String::createStaticString("f_lasti");
    static PyObject *const _f_lineno = Shiboken::String::createStaticString("f_lineno");
    static PyObject *const _f_code = Shiboken::String::createStaticString("f_code");
    static PyObject *const _co_filename = Shiboken::String::createStaticString("co_filename");
    AutoDecRef ignore{};
    auto *frame = reinterpret_cast<PyObject *>(PyEval_GetFrame());
    if (frame == nullptr) {
        fprintf(stdout, "\n%s BEGIN no frame END\n\n", marker);
    } else {
        AutoDecRef f_lasti(PyObject_GetAttr(frame, _f_lasti));
        AutoDecRef f_lineno(PyObject_GetAttr(frame, _f_lineno));
        AutoDecRef f_code(PyObject_GetAttr(frame, _f_code));
        AutoDecRef co_filename(PyObject_GetAttr(f_code, _co_filename));
        long line = PyLong_AsLong(f_lineno);
        const char *fname = String::toCString(co_filename);
        fprintf(stdout, "\n%s BEGIN line=%ld %s\n", marker, line, fname);
        ignore.reset(PyObject_CallFunctionObjArgs(disco, f_code.object(), f_lasti.object(), nullptr));
        fprintf(stdout, "%s END line=%ld %s\n\n", marker, line, fname);
    }
#if PY_VERSION_HEX >= 0x030C0000 && !Py_LIMITED_API
    if (error_type)
        PyErr_DisplayException(error_value);
#endif
    static PyObject *stdout_file = PySys_GetObject("stdout");
    ignore.reset(PyObject_CallMethod(stdout_file, "flush", nullptr));
    PyErr_Restore(error_type, error_value, error_traceback);
}

// Python 3.13
static int const LOAD_ATTR_313 = 82;
static int const CALL_313 = 53;
static int const PUSH_NULL_313 = 34;
// Python 3.12
static int const CALL_312 = 171;
// Python 3.11
static int const PRECALL = 166;
// we have "big instructions" with gaps after them
static int const LOAD_METHOD_GAP_311 = 10 * 2;
static int const LOAD_ATTR_GAP_311 = 4 * 2;
static int const LOAD_ATTR_GAP = 9 * 2;
// Python 3.7 - 3.10
static int const LOAD_METHOD = 160;
static int const CALL_METHOD = 161;
// Python 3.6
static int const CALL_FUNCTION = 131;
static int const LOAD_ATTR_312 = 106;
// NoGil (how long will this exist in this form?)
static int const LOAD_METHOD_NOGIL = 55;
static int const CALL_METHOD_NOGIL = 72;

static bool currentOpcode_Is_CallMethNoArgs()
{
    static auto number = _PepRuntimeVersion();
    static int LOAD_ATTR = number < 0x030D00 ? LOAD_ATTR_312 : LOAD_ATTR_313;
    static int CALL = number < 0x030D00 ? CALL_312 : CALL_313;
    // PYSIDE-2221: Special case for the NoGil version:
    //              Find out if we have such a version.
    //              We could also ask the variable `Py_NOGIL`.
    static PyObject *flags = PySys_GetObject("flags");
    static bool isNoGil = PyObject_HasAttrString(flags, "nogil");
    // We look into the currently active operation if we are going to call
    // a method with zero arguments.
    auto *frame = PyEval_GetFrame();
    if (frame == nullptr) // PYSIDE-2796, has been observed to fail
        return false;
#if !Py_LIMITED_API && !defined(PYPY_VERSION)
    auto *f_code = PyFrame_GetCode(frame);
#else
    static PyObject *const _f_code = Shiboken::String::createStaticString("f_code");
    AutoDecRef dec_f_code(PyObject_GetAttr(reinterpret_cast<PyObject *>(frame), _f_code));
    auto *f_code = dec_f_code.object();
#endif
#if PY_VERSION_HEX >= 0x030B0000 && !Py_LIMITED_API
    AutoDecRef dec_co_code(PyCode_GetCode(f_code));
    Py_ssize_t f_lasti = PyFrame_GetLasti(frame);
#else
    static PyObject *const _f_lasti = Shiboken::String::createStaticString("f_lasti");
    static PyObject *const _co_code = Shiboken::String::createStaticString("co_code");
    AutoDecRef dec_co_code(PyObject_GetAttr(reinterpret_cast<PyObject *>(f_code), _co_code));
    AutoDecRef dec_f_lasti(PyObject_GetAttr(reinterpret_cast<PyObject *>(frame), _f_lasti));
    Py_ssize_t f_lasti = PyLong_AsSsize_t(dec_f_lasti);
#endif
    Py_ssize_t code_len;
    char *co_code{};
    PyBytes_AsStringAndSize(dec_co_code, &co_code, &code_len);
    uint8_t opcode1 = co_code[f_lasti];
    if (isNoGil) {
        uint8_t opcode2 = co_code[f_lasti + 4];
        uint8_t oparg2 = co_code[f_lasti + 6];
        return opcode1 == LOAD_METHOD_NOGIL && opcode2 == CALL_METHOD_NOGIL && oparg2 == 1;
    }
    uint8_t opcode2 = co_code[f_lasti + 2];
    uint8_t oparg2 = co_code[f_lasti + 3];
    if (number < 0x030B00)
        return opcode1 == LOAD_METHOD && opcode2 == CALL_METHOD && oparg2 == 0;

    if (number < 0x030C00) {
        // With Python 3.11, the opcodes get bigger and change a bit.
        // Note: The new adaptive opcodes are elegantly hidden and we
        //       don't need to take care of them.
        if (opcode1 == LOAD_METHOD)
            f_lasti += LOAD_METHOD_GAP_311;
        else if (opcode1 == LOAD_ATTR_312)
            f_lasti += LOAD_ATTR_GAP_311;
        else
            return false;

        opcode2 = co_code[f_lasti + 2];
        oparg2 = co_code[f_lasti + 3];

        return opcode2 == PRECALL && oparg2 == 0;
    }
    // With Python 3.12, the opcodes get again bigger and change a bit.
    // Note: The new adaptive opcodes are elegantly hidden and we
    //       don't need to take care of them.
    if (opcode1 == LOAD_ATTR)
        f_lasti += LOAD_ATTR_GAP;
    else
        return false;

    if (number >= 0x030D00) {
        int opcode3 = co_code[f_lasti + 2];
        if (opcode3 == PUSH_NULL_313)
            f_lasti += 2;
    }
    opcode2 = co_code[f_lasti + 2];
    oparg2 = co_code[f_lasti + 3];

    return opcode2 == CALL && oparg2 == 0;
}

void initEnumFlagsDict(PyTypeObject *type)
{
    // We create a dict for all flag enums that holds the original C++ name
    // and a dict that gives every enum/flag type name.
    static PyObject *const split = Shiboken::String::createStaticString("split");
    static PyObject *const colon = Shiboken::String::createStaticString(":");
    auto sotp = PepType_SOTP(type);
    auto **enumFlagInfo = sotp->enumFlagInfo;
    auto *dict = PyDict_New();
    auto *typeDict = PyDict_New();
    for (; *enumFlagInfo; ++enumFlagInfo) {
        AutoDecRef line(PyUnicode_FromString(*enumFlagInfo));
        AutoDecRef parts(PyObject_CallMethodObjArgs(line, split, colon, nullptr));
        auto *name = PyList_GetItem(parts, 0);
        if (PyList_Size(parts) == 3) {
            auto *key = PyList_GetItem(parts, 2);
            auto *value = name;
            PyDict_SetItem(dict, key, value);
        }
        auto *typeName = PyList_GetItem(parts, 1);
        PyDict_SetItem(typeDict, name, typeName);
    }
    sotp->enumFlagsDict = dict;
    sotp->enumTypeDict = typeDict;
}

static PyObject *replaceNoArgWithZero(PyObject *callable)
{
    static auto *partial = Pep_GetPartialFunction();
    static auto *zero = PyLong_FromLong(0);
    return PyObject_CallFunctionObjArgs(partial, callable, zero, nullptr);
}

static PyObject *lookupUnqualifiedOrOldEnum(PyTypeObject *type, PyObject *name)
{
    // MRO has been observed to be 0 in case of errors with QML decorators
    if (type == nullptr || type->tp_mro == nullptr)
        return nullptr;
    // Quick Check: Avoid "__..", "_slots", etc.
    if (std::isalpha(Shiboken::String::toCString(name)[0]) == 0)
        return nullptr;
    static PyTypeObject *const EnumMeta = getPyEnumMeta();
    static PyObject *const _member_map_ = String::createStaticString("_member_map_");
    // This is similar to `find_name_in_mro`, but instead of looking directly into
    // tp_dict, we also search for the attribute in local classes of that dict (Part 2).
    PyObject *mro = type->tp_mro;
    PyObject *result{};
    assert(PyTuple_Check(mro));
    for (Py_ssize_t idx = 0, n = PyTuple_Size(mro); idx < n; ++idx) {
        auto *base = PyTuple_GetItem(mro, idx);
        auto *type_base = reinterpret_cast<PyTypeObject *>(base);
        if (!SbkObjectType_Check(type_base))
            continue;
        auto sotp = PepType_SOTP(type_base);
        // The EnumFlagInfo structure tells us if there are Enums at all.
        const char **enumFlagInfo = sotp->enumFlagInfo;
        if (!(enumFlagInfo))
            continue;
        if (!sotp->enumFlagsDict)
            initEnumFlagsDict(type_base);
        bool useFakeRenames = !(Enum::enumOption & Enum::ENOPT_NO_FAKERENAMES);
        if (useFakeRenames) {
            auto *rename = PyDict_GetItem(sotp->enumFlagsDict, name);
            if (rename) {
                /*
                 * Part 1: Look into the enumFlagsDict if we have an old flags name.
                 * -------------------------------------------------------------
                 * We need to replace the parameterless

                    QtCore.Qt.Alignment()

                 * by the one-parameter call

                    QtCore.Qt.AlignmentFlag(0)

                 * That means: We need to bind the zero as default into a wrapper and
                 * return that to be called.
                 *
                 * Addendum:
                 * ---------
                 * We first need to look into the current opcode of the bytecode to find
                 * out if we have a call like above or just a type lookup.
                 */
                AutoDecRef tpDict(PepType_GetDict(type_base));
                auto *flagType = PyDict_GetItem(tpDict.object(), rename);
                if (currentOpcode_Is_CallMethNoArgs())
                    return replaceNoArgWithZero(flagType);
                Py_INCREF(flagType);
                return flagType;
            }
        }
        bool useFakeShortcuts = !(Enum::enumOption & Enum::ENOPT_NO_FAKESHORTCUT);
        if (useFakeShortcuts) {
            AutoDecRef tpDict(PepType_GetDict(type_base));
            auto *dict = tpDict.object();
            PyObject *key, *value;
            Py_ssize_t pos = 0;
            while (PyDict_Next(dict, &pos, &key, &value)) {
                /*
                 * Part 2: Check for a duplication into outer scope.
                 * -------------------------------------------------
                 * We need to replace the shortcut

                    QtCore.Qt.AlignLeft

                 * by the correct call

                    QtCore.Qt.AlignmentFlag.AlignLeft

                 * That means: We need to search all Enums of the class.
                 */
                if (Py_TYPE(value) == EnumMeta) {
                    auto *valtype = reinterpret_cast<PyTypeObject *>(value);
                    AutoDecRef valtypeDict(PepType_GetDict(valtype));
                    auto *member_map = PyDict_GetItem(valtypeDict.object(), _member_map_);
                    if (member_map && PyDict_Check(member_map)) {
                        result = PyDict_GetItem(member_map, name);
                        Py_XINCREF(result);
                        if (result)
                            return result;
                    }
                }
            }
        }
    }
    return nullptr;
}

PyObject *mangled_type_getattro(PyTypeObject *type, PyObject *name)
{
    /*
     * Note: This `type_getattro` version is only the default that comes
     * from `PyType_Type.tp_getattro`. This does *not* interfere in any way
     * with the complex `tp_getattro` of `QObject` and other instances.
     * What we change here is the meta class of `QObject`.
     */
    static getattrofunc const type_getattro = PepExt_Type_GetGetAttroSlot(&PyType_Type);
    static PyObject *const ignAttr1 = PyName::qtStaticMetaObject();
    static PyObject *const ignAttr2 = PyMagicName::get();
    static PyTypeObject *const EnumMeta = getPyEnumMeta();

    if (SelectFeatureSet != nullptr)
        SelectFeatureSet(type);
    auto *ret = type_getattro(reinterpret_cast<PyObject *>(type), name);

    // PYSIDE-1735: Be forgiving with strict enums and fetch the enum, silently.
    //              The PYI files now look correct, but the old duplication is
    //              emulated here. This should be removed in Qt 7, see `parser.py`.
    //
    // FIXME PYSIDE7 should remove this forgiveness:
    //
    //      The duplication of enum values into the enclosing scope, allowing to write
    //      Qt.AlignLeft instead of Qt.Alignment.AlignLeft, is still implemented but
    //      no longer advertized in PYI files or line completion.

    if (ret && Py_TYPE(ret) == EnumMeta && currentOpcode_Is_CallMethNoArgs()) {
        bool useZeroDefault = !(Enum::enumOption & Enum::ENOPT_NO_ZERODEFAULT);
        if (useZeroDefault) {
            // We provide a zero argument for compatibility if it is a call with no args.
            auto *hold = replaceNoArgWithZero(ret);
            Py_DECREF(ret);
            ret = hold;
        }
    }

    if (!ret && name != ignAttr1 && name != ignAttr2) {
        PyObject *error_type{}, *error_value{}, *error_traceback{};
        PyErr_Fetch(&error_type, &error_value, &error_traceback);
        ret = lookupUnqualifiedOrOldEnum(type, name);
        if (ret) {
            Py_DECREF(error_type);
            Py_XDECREF(error_value);
            Py_XDECREF(error_traceback);
        } else {
            PyErr_Restore(error_type, error_value, error_traceback);
        }
    }
    return ret;
}

PyObject *Sbk_TypeGet___dict__(PyTypeObject *type, void * /* context */)
{
    /*
     * This is the override for getting a dict.
     */
    AutoDecRef tpDict(PepType_GetDict(type));
    auto *dict = tpDict.object();;
    if (dict == nullptr)
        Py_RETURN_NONE;
    if (SelectFeatureSet != nullptr) {
        SelectFeatureSet(type);
        tpDict.reset(PepType_GetDict(type));
        dict = tpDict.object();
    }
    return PyDictProxy_New(dict);
}

// These functions replace the standard PyObject_Generic(Get|Set)Attr functions.
// They provide the default that "object" inherits.
// Everything else is directly handled by cppgenerator that calls `Feature::Select`.
PyObject *SbkObject_GenericGetAttr(PyObject *obj, PyObject *name)
{
    auto type = Py_TYPE(obj);
    if (SelectFeatureSet != nullptr)
        SelectFeatureSet(type);
    return PyObject_GenericGetAttr(obj, name);
}

int SbkObject_GenericSetAttr(PyObject *obj, PyObject *name, PyObject *value)
{
    auto type = Py_TYPE(obj);
    if (SelectFeatureSet != nullptr)
        SelectFeatureSet(type);
    return PyObject_GenericSetAttr(obj, name, value);
}

const char **SbkObjectType_GetPropertyStrings(PyTypeObject *type)
{
    return PepType_SOTP(type)->propertyStrings;
}

void SbkObjectType_SetPropertyStrings(PyTypeObject *type, const char **strings)
{
    PepType_SOTP(type)->propertyStrings = strings;
}

void SbkObjectType_SetEnumFlagInfo(PyTypeObject *type, const char **strings)
{
    PepType_SOTP(type)->enumFlagInfo = strings;
}

// PYSIDE-1626: Enforcing a context switch without further action.
void SbkObjectType_UpdateFeature(PyTypeObject *type)
{
    if (SelectFeatureSet != nullptr)
        SelectFeatureSet(type);
}

} // extern "C"
