/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SBKENUM_H
#define SBKENUM_H

#include "sbkpython.h"
#include "shibokenmacros.h"

extern "C"
{

/// exposed for the signature module
LIBSHIBOKEN_API void init_enum();

extern LIBSHIBOKEN_API PyTypeObject *SbkEnumType_TypeF(void);
struct SbkObjectType;
struct SbkConverter;
struct SbkEnumType;
struct SbkEnumTypePrivate;

} // extern "C"

namespace Shiboken
{

inline bool isShibokenEnum(PyObject *pyObj)
{
    return Py_TYPE(Py_TYPE(pyObj)) == SbkEnumType_TypeF();
}

namespace Enum
{
    LIBSHIBOKEN_API bool check(PyObject *obj);
    /**
     *  Creates a new enum type (and its flags type, if any is given)
     *  and registers it to Python and adds it to \p module.
     *  \param module       Module to where the new enum type will be added.
     *  \param name         Name of the enum.
     *  \param fullName     Name of the enum that includes all scope information (e.g.: "module.Enum").
     *  \param cppName      Full qualified C++ name of the enum.
     *  \param flagsType    Optional Python type for the flags associated with the enum.
     *  \return The new enum type or NULL if it fails.
     */
    LIBSHIBOKEN_API PyTypeObject *createGlobalEnum(PyObject *module,
                                                   const char *name,
                                                   const char *fullName,
                                                   const char *cppName,
                                                   PyTypeObject *flagsType = nullptr);
    /// This function does the same as createGlobalEnum, but adds the enum to a Shiboken type or namespace.
    LIBSHIBOKEN_API PyTypeObject *createScopedEnum(SbkObjectType *scope,
                                                   const char *name,
                                                   const char *fullName,
                                                   const char *cppName,
                                                   PyTypeObject *flagsType = nullptr);

    /**
     *  Creates a new enum item for a given enum type and adds it to \p module.
     *  \param enumType  Enum type to where the new enum item will be added.
     *  \param module    Module to where the enum type of the new enum item belongs.
     *  \param itemName  Name of the enum item.
     *  \param itemValue Numerical value of the enum item.
     *  \return true if everything goes fine, false if it fails.
     */
    LIBSHIBOKEN_API bool createGlobalEnumItem(PyTypeObject *enumType, PyObject *module, const char *itemName, long itemValue);
    /// This function does the same as createGlobalEnumItem, but adds the enum to a Shiboken type or namespace.
    LIBSHIBOKEN_API bool createScopedEnumItem(PyTypeObject *enumType, PyTypeObject *scope,
                                              const char *itemName, long itemValue);
    LIBSHIBOKEN_API bool createScopedEnumItem(PyTypeObject *enumType, SbkObjectType *scope, const char *itemName, long itemValue);

    LIBSHIBOKEN_API PyObject *newItem(PyTypeObject *enumType, long itemValue, const char *itemName = nullptr);

    LIBSHIBOKEN_API PyTypeObject *newTypeWithName(const char *name, const char *cppName,
                                                  PyTypeObject *numbers_fromFlag=nullptr);
    LIBSHIBOKEN_API const char *getCppName(PyTypeObject *type);

    LIBSHIBOKEN_API long getValue(PyObject *enumItem);
    LIBSHIBOKEN_API PyObject *getEnumItemFromValue(PyTypeObject *enumType, long itemValue);

    /// Sets the enum's type converter.
    LIBSHIBOKEN_API void setTypeConverter(PyTypeObject *enumType, SbkConverter *converter);
    /// Returns the converter assigned to the enum \p type.
    LIBSHIBOKEN_API SbkConverter *getTypeConverter(PyTypeObject *enumType);

    LIBSHIBOKEN_API PyObject *unpickleEnum(PyObject *, PyObject *);
}

} // namespace Shiboken

#endif // SKB_PYENUM_H
