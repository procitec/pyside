/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt for Python.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef CPPGENERATOR_H
#define CPPGENERATOR_H

#include "shibokengenerator.h"

/**
 *   The CppGenerator generate the implementations of C++ bindings classes.
 */
class CppGenerator : public ShibokenGenerator
{
public:
    CppGenerator();

    const char *name() const override { return "Source generator"; }

protected:
    QString fileNameSuffix() const override;
    QString fileNameForContext(const GeneratorContext &context) const override;
    QVector<AbstractMetaFunctionList> filterGroupedOperatorFunctions(const AbstractMetaClass *metaClass,
                                                                     uint query);
    void generateClass(QTextStream &s, const GeneratorContext &classContext) override;
    bool finishGeneration() override;

private:
    void writeInitFunc(QTextStream &declStr, QTextStream &callStr,
                       const Indentor &indent, const QString &initFunctionName,
                       const TypeEntry *enclosingEntry = nullptr);
    void writeCacheResetNative(QTextStream &s, const GeneratorContext &classContext);
    void writeConstructorNative(QTextStream &s, const GeneratorContext &classContext,
                                const AbstractMetaFunction *func);
    void writeDestructorNative(QTextStream &s, const GeneratorContext &classContext);

    QString getVirtualFunctionReturnTypeName(const AbstractMetaFunction *func);
    void writeVirtualMethodNative(QTextStream &s, const AbstractMetaFunction *func, int cacheIndex);
    void writeVirtualMethodCppCall(QTextStream &s, const AbstractMetaFunction *func,
                                   const QString &funcName, const CodeSnipList &snips,
                                   const AbstractMetaArgument *lastArg, const TypeEntry *retType,
                                   const QString &returnStatement);
    QString virtualMethodReturn(QTextStream &s,
                                const AbstractMetaFunction *func,
                                const FunctionModificationList &functionModifications);
    void writeMetaObjectMethod(QTextStream &s, const GeneratorContext &classContext);
    void writeMetaCast(QTextStream &s, const GeneratorContext &classContext);

    void writeEnumConverterFunctions(QTextStream &s, const TypeEntry *enumType);
    void writeEnumConverterFunctions(QTextStream &s, const AbstractMetaEnum *metaEnum);
    void writeConverterFunctions(QTextStream &s, const AbstractMetaClass *metaClass,
                                 const GeneratorContext &classContext);
    void writeCustomConverterFunctions(QTextStream &s, const CustomConversion *customConversion);
    void writeConverterRegister(QTextStream &s, const AbstractMetaClass *metaClass,
                                const GeneratorContext &classContext);
    void writeCustomConverterRegister(QTextStream &s, const CustomConversion *customConversion, const QString &converterVar);

    void writeContainerConverterFunctions(QTextStream &s, const AbstractMetaType *containerType);

    void writeSmartPointerConverterFunctions(QTextStream &s, const AbstractMetaType *smartPointerType);

    void writeMethodWrapperPreamble(QTextStream &s, OverloadData &overloadData,
                                    const GeneratorContext &context);
    void writeConstructorWrapper(QTextStream &s, const AbstractMetaFunctionList &overloads,
                                 const GeneratorContext &classContext);
    void writeMethodWrapper(QTextStream &s, const AbstractMetaFunctionList &overloads,
                            const GeneratorContext &classContext);
    void writeArgumentsInitializer(QTextStream &s, OverloadData &overloadData);
    void writeCppSelfConversion(QTextStream &s, const GeneratorContext &context,
                                const QString &className, bool useWrapperClass);
    void writeCppSelfDefinition(QTextStream &s,
                                const AbstractMetaFunction *func,
                                const GeneratorContext &context,
                                bool hasStaticOverload = false);
    void writeCppSelfDefinition(QTextStream &s,
                                const GeneratorContext &context,
                                bool hasStaticOverload = false,
                                bool cppSelfAsReference = false);

    void writeErrorSection(QTextStream &s, OverloadData &overloadData);
    void writeFunctionReturnErrorCheckSection(QTextStream &s, bool hasReturnValue = true);

    /// Writes the check section for the validity of wrapped C++ objects.
    void writeInvalidPyObjectCheck(QTextStream &s, const QString &pyObj);

    void writeTypeCheck(QTextStream &s, const AbstractMetaType *argType, const QString &argumentName,
                        bool isNumber = false, const QString &customType = QString(),
                        bool rejectNull = false);
    void writeTypeCheck(QTextStream& s, const OverloadData *overloadData, QString argumentName);

    void writeTypeDiscoveryFunction(QTextStream &s, const AbstractMetaClass *metaClass);

    void writeSetattroDefinition(QTextStream &s, const AbstractMetaClass *metaClass) const;
    void writeSetattroDefaultReturn(QTextStream &s) const;
    void writeSmartPointerSetattroFunction(QTextStream &s, const GeneratorContext &context);
    void writeSetattroFunction(QTextStream &s, AttroCheck attroCheck,
                               const GeneratorContext &context);
    static void writeGetattroDefinition(QTextStream &s, const AbstractMetaClass *metaClass);
    void writeSmartPointerGetattroFunction(QTextStream &s, const GeneratorContext &context);
    void writeGetattroFunction(QTextStream &s, AttroCheck attroCheck,
                               const GeneratorContext &context);
    QString writeSmartPointerGetterCast();
    QString qObjectGetAttroFunction() const;

    /**
     *   Writes Python to C++ conversions for arguments on Python wrappers.
     *   If implicit conversions, and thus new object allocation, are needed,
     *   code to deallocate a possible new instance is also generated.
     *   \param s                    text stream to write
     *   \param argType              a pointer to the argument type to be converted
     *   \param argName              C++ argument name
     *   \param pyArgName            Python argument name
     *   \param context              the current meta class
     *   \param defaultValue         an optional default value to be used instead of the conversion result
     *   \param castArgumentAsUnused if true the converted argument is cast as unused to avoid compiler warnings
     */
    void writeArgumentConversion(QTextStream &s, const AbstractMetaType *argType,
                                 const QString &argName, const QString &pyArgName,
                                 const AbstractMetaClass *context = nullptr,
                                 const QString &defaultValue = QString(),
                                 bool castArgumentAsUnused = false);

    /**
     *  Returns the AbstractMetaType for a function argument.
     *  If the argument type was modified in the type system, this method will
     *  try to build a new type based on the type name defined in the type system.
     *  \param  func    The function which owns the argument.
     *  \param  argPos  Argument position in the function signature.
     *                  Note that the position 0 represents the return value, and the function
     *                  parameters start counting on 1.
     *  \param  newType It is set to true if the type returned is a new object that must be deallocated.
     *  \return The type of the argument indicated by \p argPos.
     */
    const AbstractMetaType *getArgumentType(const AbstractMetaFunction *func, int argPos);

    void writePythonToCppTypeConversion(QTextStream &s,
                                        const AbstractMetaType *type,
                                        const QString &pyIn,
                                        const QString &cppOut,
                                        const AbstractMetaClass *context = nullptr,
                                        const QString &defaultValue = QString());

    /// Writes the conversion rule for arguments of regular and virtual methods.
    void writeConversionRule(QTextStream &s, const AbstractMetaFunction *func, TypeSystem::Language language);
    /// Writes the conversion rule for the return value of a method.
    void writeConversionRule(QTextStream &s, const AbstractMetaFunction *func, TypeSystem::Language language, const QString &outputVar);

    /**
     *   Set the Python method wrapper return value variable to Py_None if
     *   there are return types different from void in any of the other overloads
     *   for the function passed as parameter.
     *   \param s text stream to write
     *   \param func a pointer to the function that will possibly return Py_None
     *   \param thereIsReturnValue indicates if the return type of any of the other overloads
     *                             for this function is different from 'void'
     */
    void writeNoneReturn(QTextStream &s, const AbstractMetaFunction *func, bool thereIsReturnValue);

    /**
     *   Writes the Python function wrapper overload decisor that selects which C++
     *   method/function to call with the received Python arguments.
     *   \param s text stream to write
     *   \param overloadData the overload data describing all the possible overloads for the function/method
     */
    void writeOverloadedFunctionDecisor(QTextStream &s, const OverloadData &overloadData);
    /// Recursive auxiliar method to the other writeOverloadedFunctionDecisor.
    void writeOverloadedFunctionDecisorEngine(QTextStream &s, const OverloadData *parentOverloadData);

    /// Writes calls to all the possible method/function overloads.
    void writeFunctionCalls(QTextStream &s,
                            const OverloadData &overloadData,
                            const GeneratorContext &context);

    /// Writes the call to a single function usually from a collection of overloads.
    void writeSingleFunctionCall(QTextStream &s,
                                 const OverloadData &overloadData,
                                 const AbstractMetaFunction *func,
                                 const GeneratorContext &context);

    /// Returns the name of a C++ to Python conversion function.
    static QString cppToPythonFunctionName(const QString &sourceTypeName, QString targetTypeName = QString());

    /// Returns the name of a Python to C++ conversion function.
    static QString pythonToCppFunctionName(const QString &sourceTypeName, const QString &targetTypeName);
    static QString pythonToCppFunctionName(const AbstractMetaType *sourceType, const AbstractMetaType *targetType);
    static QString pythonToCppFunctionName(const CustomConversion::TargetToNativeConversion *toNative, const TypeEntry *targetType);

    /// Returns the name of a Python to C++ convertible check function.
    static QString convertibleToCppFunctionName(const QString &sourceTypeName, const QString &targetTypeName);
    static QString convertibleToCppFunctionName(const AbstractMetaType *sourceType, const AbstractMetaType *targetType);
    static QString convertibleToCppFunctionName(const CustomConversion::TargetToNativeConversion *toNative, const TypeEntry *targetType);

    /// Writes a C++ to Python conversion function.
    void writeCppToPythonFunction(QTextStream &s, const QString &code, const QString &sourceTypeName, QString targetTypeName = QString());
    void writeCppToPythonFunction(QTextStream &s, const CustomConversion *customConversion);
    void writeCppToPythonFunction(QTextStream &s, const AbstractMetaType *containerType);

    /// Writes a Python to C++ conversion function.
    void writePythonToCppFunction(QTextStream &s, const QString &code, const QString &sourceTypeName, const QString &targetTypeName);

    /// Writes a Python to C++ convertible check function.
    void writeIsPythonConvertibleToCppFunction(QTextStream &s,
                                               const QString &sourceTypeName,
                                               const QString &targetTypeName,
                                               const QString &condition,
                                               QString pythonToCppFuncName = QString(),
                                               bool acceptNoneAsCppNull = false);

    /// Writes a pair of Python to C++ conversion and check functions.
    void writePythonToCppConversionFunctions(QTextStream &s,
                                             const AbstractMetaType *sourceType,
                                             const AbstractMetaType *targetType,
                                             QString typeCheck = QString(),
                                             QString conversion = QString(),
                                             const QString &preConversion = QString());
    /// Writes a pair of Python to C++ conversion and check functions for implicit conversions.
    void writePythonToCppConversionFunctions(QTextStream &s,
                                             const CustomConversion::TargetToNativeConversion *toNative,
                                             const TypeEntry *targetType);

    /// Writes a pair of Python to C++ conversion and check functions for instantiated container types.
    void writePythonToCppConversionFunctions(QTextStream &s, const AbstractMetaType *containerType);

    void writeAddPythonToCppConversion(QTextStream &s, const QString &converterVar, const QString &pythonToCppFunc, const QString &isConvertibleFunc);

    void writeNamedArgumentResolution(QTextStream &s, const AbstractMetaFunction *func,
                                      bool usePyArgs, const OverloadData &overloadData);

    /// Returns a string containing the name of an argument for the given function and argument index.
    QString argumentNameFromIndex(const AbstractMetaFunction *func, int argIndex, const AbstractMetaClass **wrappedClass);
    void writeMethodCall(QTextStream &s, const AbstractMetaFunction *func,
                         const GeneratorContext &context, int maxArgs = 0);

    QString getInitFunctionName(const GeneratorContext &context) const;
    QString getSimpleClassInitFunctionName(const AbstractMetaClass *metaClass) const;
    QString getSimpleClassStaticFieldsInitFunctionName(const AbstractMetaClass *metaClass) const;

    void writeSignatureStrings(QTextStream &s, QTextStream &signatureStream,
                               const QString &arrayName,
                               const char *comment) const;
    void writeClassRegister(QTextStream &s,
                            const AbstractMetaClass *metaClass,
                            const GeneratorContext &classContext,
                            QTextStream &signatureStream);
    void writeStaticFieldInitialization(QTextStream &s,
                                        const AbstractMetaClass *metaClass);
    void writeClassDefinition(QTextStream &s,
                              const AbstractMetaClass *metaClass,
                              const GeneratorContext &classContext);
    void writeMethodDefinitionEntry(QTextStream &s, const AbstractMetaFunctionList &overloads);
    void writeMethodDefinition(QTextStream &s, const AbstractMetaFunctionList &overloads);
    void writeSignatureInfo(QTextStream &s, const AbstractMetaFunctionList &overloads);
    /// Writes the implementation of all methods part of python sequence protocol
    void writeSequenceMethods(QTextStream &s,
                              const AbstractMetaClass *metaClass,
                              const GeneratorContext &context);
    void writeTypeAsSequenceDefinition(QTextStream &s, const AbstractMetaClass *metaClass);

    /// Writes the PyMappingMethods structure for types that supports the python mapping protocol.
    void writeTypeAsMappingDefinition(QTextStream &s, const AbstractMetaClass *metaClass);
    void writeMappingMethods(QTextStream &s,
                             const AbstractMetaClass *metaClass,
                             const GeneratorContext &context);

    void writeTypeAsNumberDefinition(QTextStream &s, const AbstractMetaClass *metaClass);

    void writeTpTraverseFunction(QTextStream &s, const AbstractMetaClass *metaClass);
    void writeTpClearFunction(QTextStream &s, const AbstractMetaClass *metaClass);

    void writeCopyFunction(QTextStream &s, const GeneratorContext &context);

    void writeGetterFunction(QTextStream &s,
                             const AbstractMetaField *metaField,
                             const GeneratorContext &context);
    void writeGetterFunction(QTextStream &s,
                             const QPropertySpec *property,
                             const GeneratorContext &context);
    void writeSetterFunctionPreamble(QTextStream &s,
                                     const QString &name,
                                     const QString &funcName,
                                     const AbstractMetaType *type,
                                     const GeneratorContext &context);
    void writeSetterFunction(QTextStream &s,
                             const AbstractMetaField *metaField,
                             const GeneratorContext &context);
    void writeSetterFunction(QTextStream &s,
                             const QPropertySpec *property,
                             const GeneratorContext &context);

    void writeRichCompareFunction(QTextStream &s, const GeneratorContext &context);

    void writeEnumsInitialization(QTextStream &s, AbstractMetaEnumList &enums);
    void writeEnumInitialization(QTextStream &s, const AbstractMetaEnum *metaEnum);

    void writeSignalInitialization(QTextStream &s, const AbstractMetaClass *metaClass);

    void writeFlagsMethods(QTextStream &s, const AbstractMetaEnum *cppEnum);
    void writeFlagsToLong(QTextStream &s, const AbstractMetaEnum *cppEnum);
    void writeFlagsNonZero(QTextStream &s, const AbstractMetaEnum *cppEnum);
    void writeFlagsNumberMethodsDefinition(QTextStream &s, const AbstractMetaEnum *cppEnum);
    void writeFlagsNumberMethodsDefinitions(QTextStream &s, const AbstractMetaEnumList &enums);
    void writeFlagsBinaryOperator(QTextStream &s, const AbstractMetaEnum *cppEnum,
                                  const QString &pyOpName, const QString &cppOpName);
    void writeFlagsUnaryOperator(QTextStream &s, const AbstractMetaEnum *cppEnum,
                                 const QString &pyOpName, const QString &cppOpName,
                                 bool boolResult = false);

    /// Writes the function that registers the multiple inheritance information for the classes that need it.
    void writeMultipleInheritanceInitializerFunction(QTextStream &s, const AbstractMetaClass *metaClass);
    /// Writes the implementation of special cast functions, used when we need to cast a class with multiple inheritance.
    void writeSpecialCastFunction(QTextStream &s, const AbstractMetaClass *metaClass);

    void writePrimitiveConverterInitialization(QTextStream &s, const CustomConversion *customConversion);
    void writeEnumConverterInitialization(QTextStream &s, const TypeEntry *enumType);
    void writeEnumConverterInitialization(QTextStream &s, const AbstractMetaEnum *metaEnum);
    void writeContainerConverterInitialization(QTextStream &s, const AbstractMetaType *type);
    void writeSmartPointerConverterInitialization(QTextStream &s, const AbstractMetaType *type);
    void writeExtendedConverterInitialization(QTextStream &s, const TypeEntry *externalType, const QVector<const AbstractMetaClass *>& conversions);

    void writeParentChildManagement(QTextStream &s, const AbstractMetaFunction *func, bool userHeuristicForReturn);
    bool writeParentChildManagement(QTextStream &s, const AbstractMetaFunction *func, int argIndex, bool userHeuristicPolicy);
    void writeReturnValueHeuristics(QTextStream &s, const AbstractMetaFunction *func);
    void writeInitQtMetaTypeFunctionBody(QTextStream &s, const GeneratorContext &context) const;

    /**
     *   Returns the multiple inheritance initializer function for the given class.
     *   \param metaClass the class for whom the function name must be generated.
     *   \return name of the multiple inheritance information initializer function or
     *           an empty string if there is no multiple inheritance in its ancestry.
     */
    QString multipleInheritanceInitializerFunctionName(const AbstractMetaClass *metaClass);

    /// Returns a list of all classes to which the given class could be cast.
    QStringList getAncestorMultipleInheritance(const AbstractMetaClass *metaClass);

    /// Returns true if the given class supports the python number protocol
    bool supportsNumberProtocol(const AbstractMetaClass *metaClass);

    /// Returns true if the given class supports the python sequence protocol
    bool supportsSequenceProtocol(const AbstractMetaClass *metaClass);

    /// Returns true if the given class supports the python mapping protocol
    bool supportsMappingProtocol(const AbstractMetaClass *metaClass);

    /// Returns true if generator should produce getters and setters for the given class.
    bool shouldGenerateGetSetList(const AbstractMetaClass *metaClass);

    void writeHashFunction(QTextStream &s, const GeneratorContext &context);

    /// Write default implementations for sequence protocol
    void writeDefaultSequenceMethods(QTextStream &s, const GeneratorContext &context);
    /// Helper function for writeStdListWrapperMethods.
    void writeIndexError(QTextStream &s, const QString &errorMsg);

    QString writeReprFunction(QTextStream &s, const GeneratorContext &context, uint indirections);

    const AbstractMetaFunction *boolCast(const AbstractMetaClass *metaClass) const;
    bool hasBoolCast(const AbstractMetaClass *metaClass) const
    { return boolCast(metaClass) != nullptr; }

    const AbstractMetaType *findSmartPointerInstantiation(const TypeEntry *entry) const;

    // Number protocol structure members names.
    static QHash<QString, QString> m_nbFuncs;

    // Maps special function names to function parameters and return types
    // used by CPython API in the sequence protocol.
    QHash<QString, QPair<QString, QString> > m_sequenceProtocol;
    // Sequence protocol structure members names.
    static QHash<QString, QString> m_sqFuncs;

    // Maps special function names to function parameters and return types
    // used by CPython API in the mapping protocol.
    QHash<QString, QPair<QString, QString> > m_mappingProtocol;
    // Mapping protocol structure members names.
    static QHash<QString, QString> m_mpFuncs;

    static QString m_currentErrorCode;

    /// Helper class to set and restore the current error code.
    class ErrorCode {
    public:
        explicit ErrorCode(QString errorCode) {
            m_savedErrorCode = CppGenerator::m_currentErrorCode;
            CppGenerator::m_currentErrorCode = errorCode;
        }
        explicit ErrorCode(int errorCode) {
            m_savedErrorCode = CppGenerator::m_currentErrorCode;
            CppGenerator::m_currentErrorCode = QString::number(errorCode);
        }
        ~ErrorCode() {
            CppGenerator::m_currentErrorCode = m_savedErrorCode;
        }
    private:
        QString m_savedErrorCode;
    };
};

#endif // CPPGENERATOR_H
