// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "messages.h"
#include "abstractmetaenum.h"
#include "abstractmetafield.h"
#include "abstractmetafunction.h"
#include "abstractmetalang.h"
#include "modifications.h"
#include "sourcelocation.h"
#include "typedatabase.h"
#include "functiontypeentry.h"
#include "enumtypeentry.h"
#include "smartpointertypeentry.h"
#include <codemodel.h>

#include "qtcompat.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QXmlStreamReader>

using namespace Qt::StringLiterals;

// abstractmetabuilder.cpp

static QTextStream &operator<<(QTextStream &s, Access a)
{
    switch (a) {
    case Access::Public:
        s << "public";
        break;
    case Access::Protected:
        s << "protected";
        break;
    case Access::Private:
        s << "private";
        break;
    }
    return s;
}

QString msgNoFunctionForModification(const AbstractMetaClassCPtr &klass,
                                     const QString &signature,
                                     const QString &originalSignature,
                                     const QStringList &possibleSignatures,
                                     const AbstractMetaFunctionCList &allFunctions)
{
    QString result;
    QTextStream str(&result);
    str << klass->typeEntry()->sourceLocation() << "signature '"
        << signature << '\'';
    if (!originalSignature.isEmpty() && originalSignature != signature)
        str << " (specified as '" << originalSignature << "')";
    str << " for function modification in '"
        << klass->qualifiedCppName() << "' not found.";
    if (!possibleSignatures.isEmpty()) {
        str << "\n  Possible candidates:\n";
        for (const auto &s : possibleSignatures)
            str << "    " << s << '\n';
    } else if (!allFunctions.isEmpty()) {
        str << "\n  No candidates were found. Member functions:\n";
        const auto maxCount = qMin(qsizetype(10), allFunctions.size());
        for (qsizetype f = 0; f < maxCount; ++f)
            str << "    " << allFunctions.at(f)->minimalSignature() << '\n';
        if (maxCount < allFunctions.size())
            str << "    ...\n";
    }
    return result;
}

QString msgArgumentIndexOutOfRange(const AbstractMetaFunction *func, int index)
{
    QString result;
    QTextStream str(&result);
    str <<"Index "  << index << " out of range for " << func->classQualifiedSignature() << '.';
    return result;
}

QString msgTypeModificationFailed(const QString &type, int n,
                                  const AbstractMetaFunction *func,
                                  const QString &why)
{
    QString result;
    QTextStream str(&result);
    str << "Unable to modify the ";
    if (n == 0)
        str << "return type";
    else
        str << "type of argument " << n;

    str << " of ";
    if (auto c = func->ownerClass())
        str << c->name() << "::";
    str << func->signature() << " to \"" << type << "\": " << why;
    return result;
}

QString msgInvalidArgumentModification(const AbstractMetaFunctionCPtr &func,
                                       int argIndex)
{
    QString result;
    QTextStream str(&result);
    str << "Invalid ";
    if (argIndex == 0)
        str << "return type modification";
    else
        str << "modification of argument " << argIndex;
    str << " for " << func->classQualifiedSignature();
    return result;
}

QString msgArgumentOutOfRange(int number, int minValue, int maxValue)
{
    QString result;
    QTextStream(&result) << "Argument number " << number
        << " out of range " << minValue << ".." << maxValue << '.';
    return result;
}

QString msgArgumentRemovalFailed(const AbstractMetaFunction *func, int n,
                                 const QString &why)
{
    QString result;
    QTextStream str(&result);
    str << "Unable to remove argument " << n << " of ";
    if (auto c = func->ownerClass())
        str << c->name() << "::";
    str << func->signature() << ":  " << why;
    return result;
}

template <class Stream>
static void msgFormatEnumType(Stream &str,
                              const EnumModelItem &enumItem,
                              const QString &className)
{
    switch (enumItem->enumKind()) {
    case CEnum:
        str << "Enum '" << enumItem->qualifiedName().join(u"::"_s) << '\'';
        break;
    case AnonymousEnum: {
        const EnumeratorList &values = enumItem->enumerators();
        str << "Anonymous enum (";
        switch (values.size()) {
        case 0:
            break;
        case 1:
            str << values.constFirst()->name();
            break;
        case 2:
            str << values.at(0)->name() << ", " << values.at(1)->name();
            break;
        default:
            str << values.at(0)->name() << ", ... , "
                << values.at(values.size() - 1)->name();
            break;
        }
        str << ')';
    }
        break;
    case EnumClass:
        str << "Scoped enum '" << enumItem->qualifiedName().join(u"::"_s) << '\'';
        break;
    }
    if (!className.isEmpty())
        str << " (class: " << className << ')';
}

static void formatAddedFuncError(const QString &addedFuncName,
                                 const AbstractMetaClassCPtr &context,
                                 QTextStream &str)
{
    if (context) {
        str << context->typeEntry()->sourceLocation()
            <<  "Unable to traverse function \"" << addedFuncName
            << "\" added to \"" << context->name() << "\": ";
    } else {
        str << "Unable to traverse added global function \""
            << addedFuncName << "\": ";
    }
}

QString msgAddedFunctionInvalidArgType(const QString &addedFuncName,
                                       const QStringList &typeName,
                                       int pos, const QString &why,
                                       const AbstractMetaClassCPtr &context)
{
    QString result;
    QTextStream str(&result);
    formatAddedFuncError(addedFuncName, context, str);
    str << "Unable to translate type \"" << typeName.join(u"::"_s)
        << "\" of argument " << pos << " of added function \""
        << addedFuncName << "\": " << why;
    return result;
}

QString msgAddedFunctionInvalidReturnType(const QString &addedFuncName,
                                          const QStringList &typeName, const QString &why,
                                          const AbstractMetaClassCPtr &context)
{
    QString result;
    QTextStream str(&result);
    formatAddedFuncError(addedFuncName, context, str);
    str << "Unable to translate return type \"" << typeName.join(u"::"_s)
        << "\" of added function \"" << addedFuncName << "\": "
        << why;
    return result;
}

QString msgUnnamedArgumentDefaultExpression(const AbstractMetaClassCPtr &context,
                                            int n, const QString &className,
                                            const AbstractMetaFunction *f)
{
    QString result;
    QTextStream str(&result);
    if (context)
        str << context->sourceLocation();
    str << "Argument " << n << " on function '" << className << "::"
        << f->minimalSignature() << "' has default expression but does not have name.";
    return result;
}

QString msgClassOfEnumNotFound(const EnumTypeEntryCPtr &entry)
{
    QString result;
    QTextStream str(&result);
    str << entry->sourceLocation() << "AbstractMeta::findEnum(), unknown class '"
        << entry->parent()->qualifiedCppName() << "' in '"
        << entry->qualifiedCppName() << '\'';
    return result;
}

QString msgNoEnumTypeEntry(const EnumModelItem &enumItem,
                           const QString &className)
{
    QString result;
    QTextStream str(&result);
    str << enumItem->sourceLocation();
    msgFormatEnumType(str, enumItem, className);
    str << " does not have a type entry (type systems: "
        << TypeDatabase::instance()->loadedTypeSystemNames() << ')';
    return result;
}

QString msgNoEnumTypeConflict(const EnumModelItem &enumItem,
                              const QString &className,
                              const TypeEntryCPtr &t)
{
    QString result;
    QDebug debug(&result); // Use the debug operator for TypeEntry::Type
    debug.noquote();
    debug.nospace();
    debug << enumItem->sourceLocation().toString();
    msgFormatEnumType(debug, enumItem, className);
    debug << " is not an enum (type: " << t->type() << ')';
    return result;
}

QString msgNamespaceNoTypeEntry(const NamespaceModelItem &item,
                                const QString &fullName)
{
    QString result;
    QTextStream str(&result);
    str << item->sourceLocation() << "namespace '" << fullName
        << "' does not have a type entry (type systems: "
        << TypeDatabase::instance()->loadedTypeSystemNames() << ')';
    return result;
}

QString msgNamespaceNotFound(const QString &name)
{
    return u"namespace '"_s + name + u"' not found."_s;
}

QString msgAmbiguousVaryingTypesFound(const QString &qualifiedName, const TypeEntryCList &te)
{
    QString result = u"Ambiguous types of varying types found for \""_s + qualifiedName
        + u"\": "_s;
    QDebug(&result) << te;
    return result;
}

QString msgAmbiguousTypesFound(const QString &qualifiedName, const TypeEntryCList &te)
{
    QString result = u"Ambiguous types found for \""_s + qualifiedName
        + u"\": "_s;
    QDebug(&result) << te;
    return result;
}

QString msgUnmatchedParameterType(const ArgumentModelItem &arg, int n,
                                  const QString &why)
{
    QString result;
    QTextStream str(&result);
    str << "unmatched type '" << arg->type().toString() << "' in parameter #"
        << (n + 1);
    if (!arg->name().isEmpty())
        str << " \"" << arg->name() << '"';
    str << ": " << why;
    return result;
}

QString msgUnmatchedReturnType(const FunctionModelItem &functionItem,
                               const QString &why)
{
    return u"unmatched return type '"_s
        + functionItem->type().toString()
        + u"': "_s + why;
}

QString msgSkippingFunction(const FunctionModelItem &functionItem,
                            const QString &signature, const QString &why)
{
    QString result;
    QTextStream str(&result);
    str << functionItem->sourceLocation() << "skipping "
        << functionItem->accessPolicy() << ' ';
    const bool isAbstract = functionItem->attributes().testFlag(FunctionAttribute::Abstract);
    if (isAbstract)
        str << "abstract ";
    str << "function '" << signature << "', " << why;
    if (isAbstract) {
        str << "\nThis will lead to compilation errors due to not "
               "being able to instantiate the wrapper.";
    }
    return result;
}

QString msgShadowingFunction(const AbstractMetaFunction *f1,
                             const AbstractMetaFunction *f2)
{
    auto f2Class = f2->implementingClass();
    QString result;
    QTextStream str(&result);
    str << f2Class->sourceLocation() << "Shadowing: " << f1->classQualifiedSignature()
        << " and " << f2->classQualifiedSignature();
    return result;
}

QString msgSignalOverloaded(const AbstractMetaClassCPtr &c,
                            const AbstractMetaFunction *f)
{
    QString result;
    QTextStream str(&result);
    str << c->sourceLocation() << "signal '" << f->name() << "' in class '"
        << c->name() << "' is overloaded.";
    return result;
}

QString msgSkippingField(const VariableModelItem &field, const QString &className,
                         const QString &type)
{
    QString result;
    QTextStream str(&result);
    str << field->sourceLocation() << "skipping " << field->accessPolicy()
        << " field '" << className << "::" << field->name()
        << "' with unmatched type '" << type << '\'';
    return result;
}

static const char msgCompilationError[] =
    "This could potentially lead to compilation errors.";

QString msgTypeNotDefined(const TypeEntryCPtr &entry)
{
    QString result;
    QTextStream str(&result);
    const bool hasConfigCondition = entry->isComplex()
        && std::static_pointer_cast<const ConfigurableTypeEntry>(entry)->hasConfigCondition();
    str << entry->sourceLocation() << "type '" <<entry->qualifiedCppName()
        << "' is specified in typesystem, but not defined";
    if (hasConfigCondition)
        str << " (disabled by configuration?).";
    else
        str << ". " << msgCompilationError;
    return result;
}

QString msgGlobalFunctionNotDefined(const FunctionTypeEntryCPtr &fte,
                                    const QString &signature,
                                    const QStringList &candidates)
{
    QString result;
    QTextStream str(&result);
    str << fte->sourceLocation() << "Global function '" << signature
        << "' is specified in typesystem, but not defined.";
    if (!candidates.isEmpty())
        str << " Candidates are: " << candidates.join(u", "_s);
    str << ' ' << msgCompilationError;
    return result;
}

QString msgStrippingArgument(const FunctionModelItem &f, int i,
                             const QString &originalSignature,
                             const ArgumentModelItem &arg,
                             const QString &reason)
{
    QString result;
    QTextStream str(&result);
    str << f->sourceLocation() << "Stripping argument #" << (i + 1) << " of "
        << originalSignature << " due to unmatched type \""
        << arg->type().toString() << "\" with default expression \""
        << arg->defaultValueExpression() << "\": " << reason;
    return result;
}

QString msgStrippingQtDisambiguatedArgument(const FunctionModelItem &f,
                                            const QString &originalSignature)
{
    QString result;
    QTextStream str(&result);
    str << f->sourceLocation() << "Stripping last argument of "
        << originalSignature << " (QT6_DECL_NEW_OVERLOAD_TAIL).";
    return result;
}

QString msgEnumNotDefined(const EnumTypeEntryCPtr &t)
{
    QString result;
    QTextStream str(&result);
    str << t->sourceLocation() << "enum '" << t->qualifiedCppName()
        << "' is specified in typesystem, but not declared.";
    return result;
}

QString msgUnknownBase(const AbstractMetaClassCPtr &metaClass,
                       const QString &baseClassName)
{
    QString result;
    QTextStream str(&result);
    str << metaClass->sourceLocation() << "Base class '" << baseClassName << "' of class '"
        << metaClass->name() << "' not found in the code for setting up inheritance.";
    return result;
}

QString msgBaseNotInTypeSystem(const AbstractMetaClassCPtr &metaClass,
                               const QString &baseClassName)
{
    QString result;
    QTextStream str(&result);
    str << metaClass->sourceLocation() << "Base class '" << baseClassName << "' of class '"
        << metaClass->name() << "' not found in the type system for setting up inheritance.";
    return result;
}

QString msgArrayModificationFailed(const FunctionModelItem &functionItem,
                                   const QString &className,
                                   const QString &errorMessage)
{
    QString result;
    QTextStream str(&result);
    str << functionItem->sourceLocation() << "While traversing " << className
        << ": " << errorMessage;
    return result;
}

QString msgCannotResolveEntity(const QString &name, const QString &reason)
{
    return u"Cannot resolve entity \""_s + name + u"\": "_s + reason;
}

QString msgCannotSetArrayUsage(const QString &function, int i, const QString &reason)
{
    return function +  u": Cannot use parameter "_s
        + QString::number(i + 1) + u" as an array: "_s + reason;
}

QString msgUnableToTranslateType(const QString &t, const QString &why)
{
    return u"Unable to translate type \""_s + t + u"\": "_s + why;
}

QString msgUnableToTranslateType(const TypeInfo &typeInfo,
                                 const QString &why)
{
    return msgUnableToTranslateType(typeInfo.toString(), why);
}

QString msgCannotFindTypeEntry(const QString &t)
{
    return u"Cannot find type entry for \""_s + t + u"\"."_s;
}

QString msgCannotFindTypeEntryForSmartPointer(const QString &t, const QString &smartPointerType)
{
    return u"Cannot find type entry \""_s + t
        + u"\" for instantiation of \""_s +smartPointerType + u"\"."_s;
}

QString msgInheritTemplateIssue(const AbstractMetaClassPtr &subclass,
                                const TypeInfo &info,
                                const QString &what)
{
    return "While inheriting template "_L1 + subclass->name()
           + " from "_L1 + info.toString() + ": "_L1 + what;
}

QString msgIgnoringTemplateParameter(const QString &typeName,
                                     const char *why)
{
    return "Ignoring template parameter "_L1 + typeName +
           ": "_L1 + QLatin1StringView(why);
}

QString msgInvalidSmartPointerType(const TypeInfo &i)
{
    return u"Invalid smart pointer type \""_s +i.toString() + u"\"."_s;
}

QString msgCannotFindSmartPointerInstantion(const TypeInfo &i)
{
    return u"Cannot find instantiation of smart pointer type for \""_s
        + i.toString() + u"\"."_s;
}

QString msgCannotTranslateTemplateArgument(int i,
                                           const TypeInfo &typeInfo,
                                           const QString &why)
{
    QString result;
    QTextStream(&result) << "Unable to translate template argument "
        << (i + 1) << typeInfo.toString() << ": " << why;
    return result;
}

QString msgDisallowThread(const AbstractMetaFunction *f)
{
    QString result;
    QTextStream str(&result);
    str <<"Disallowing threads for ";
    if (auto c = f->declaringClass())
        str << c->name() << "::";
    str << f->name() << "().";
    return result;
}

QString msgNamespaceToBeExtendedNotFound(const QString &namespaceName, const QString &packageName)
{
    return u"The namespace '"_s + namespaceName
        + u"' to be extended cannot be found in package "_s
        + packageName + u'.';
}

QString msgPropertyTypeParsingFailed(const QString &name, const QString &typeName,
                                     const QString &why)
{
    QString result;
    QTextStream str(&result);
    str << "Unable to decide type of property: \"" << name << "\" (" << typeName
        << "): " << why;
    return result;
}

QString msgPropertyExists(const QString &className, const QString &name)
{
    return u"class "_s + className + u" already has a property \""_s
           + name + u"\" (defined by Q_PROPERTY)."_s;
}

QString msgFunctionVisibilityModified(const AbstractMetaClassCPtr &c,
                                      const AbstractMetaFunction *f)
{
    QString result;
    QTextStream str(&result);
    str << c->sourceLocation() << "Visibility of function '" << f->name()
        << "' modified in class '"<< c->name() << '\'';
    return result;
}

QString msgUsingMemberClassNotFound(const AbstractMetaClassCPtr &c,
                                    const QString &baseClassName,
                                    const QString &memberName)
{
    QString result;
    QTextStream str(&result);
    str << c->sourceLocation() << "base class \"" << baseClassName
        << "\" of \"" << c->qualifiedCppName() << "\" for using member \""
        << memberName << "\" not found.";
     return result;
}
// docparser.cpp

QString msgCannotFindDocumentation(const QString &fileName,
                                   const char *what, const QString &name,
                                   const QString &query)
{
    QString result;
    QTextStream str(&result);
    str << "Cannot find documentation for " << what
        << ' ' << name << " in:\n    " << QDir::toNativeSeparators(fileName);
    if (!query.isEmpty())
        str << "\n  using query:\n    " << query;
    return result;
}

QString msgFallbackForDocumentation(const QString &fileName,
                                    const char *what, const QString &name,
                                    const QString &query)
{
    QString result;
    QTextStream str(&result);
    str << "Fallback used while trying to find documentation for " << what
        << ' ' << name << " in:\n    " << QDir::toNativeSeparators(fileName);
    if (!query.isEmpty())
        str << "\n  using query:\n    " << query;
    return result;
}

static QString functionDescription(const AbstractMetaFunction *function)
{
    QString result = u'"' + function->classQualifiedSignature() + u'"';
    if (function->flags().testFlag(AbstractMetaFunction::Flag::HiddenFriend))
        result += u" (hidden friend)"_s;
    if (function->flags().testFlag(AbstractMetaFunction::Flag::InheritedFromTemplate))
        result += u" (inherited from template)"_s;
    return result;
}

QString msgCannotFindDocumentation(const QString &fileName,
                                   const AbstractMetaFunction *function,
                                   const QString &query)
{
    return msgCannotFindDocumentation(fileName, "function",
                                      functionDescription(function), query);
}

QString msgFallbackForDocumentation(const QString &fileName,
                                    const AbstractMetaFunction *function,
                                    const QString &query)
{
    return msgFallbackForDocumentation(fileName, "function",
                                       functionDescription(function), query);
}

QString msgCannotFindDocumentation(const QString &fileName,
                                   const AbstractMetaClassCPtr &metaClass,
                                   const AbstractMetaEnum &e,
                                   const QString &query)
{
    QString name = e.name();
    if (metaClass != nullptr)
        name.prepend(metaClass->name() + "::"_L1);
    return msgCannotFindDocumentation(fileName, "enum", name, query);
}

QString msgCannotFindDocumentation(const QString &fileName,
                                   const AbstractMetaClassCPtr &metaClass,
                                   const AbstractMetaField &f,
                                   const QString &query)
{
    QString name = f.name();
    if (metaClass != nullptr)
        name.prepend(metaClass->name() + "::"_L1);
    return msgCannotFindDocumentation(fileName, "field", name, query);
}

QString msgXpathDocModificationError(const DocModificationList& mods,
                                     const QString &what)
{
    QString result;
    QTextStream str(&result);
    str << "Error when applying modifications (";
    for (const DocModification &mod : mods) {
        if (mod.mode() == TypeSystem::DocModificationXPathReplace) {
            str << '"' << mod.xpath() << "\" -> \"";
            const QString simplified = mod.code().simplified();
            if (simplified.size() > 20)
                str << QStringView{simplified}.left(20) << "...";
            else
                str << simplified;
            str << '"';
        }
    }
    str << "): " << what;
    return result;
}

// fileout.cpp

QString msgCannotOpenForReading(const QFile &f)
{
    return QString::fromLatin1("Failed to open file '%1' for reading: %2")
           .arg(QDir::toNativeSeparators(f.fileName()), f.errorString());
}

QString msgCannotOpenForWriting(const QFile &f)
{
    return QString::fromLatin1("Failed to open file '%1' for writing: %2")
           .arg(QDir::toNativeSeparators(f.fileName()), f.errorString());
}

QString msgWriteFailed(const QFile &f, qsizetype size)
{
    QString result;
    QTextStream(&result) << "Failed to write " << size << "bytes to '"
        << QDir::toNativeSeparators(f.fileName()) << "': "
        << f.errorString();
    return result;
}

// generator.cpp

QString msgCannotUseEnumAsInt(const QString &name)
{
    return u"Cannot convert the protected scoped enum \""_s + name
           + u"\" to type int when generating wrappers for the protected hack. "
           "Compilation errors may occur when used as a function argument."_s;
}

QString msgCannotFindSmartPointerGetter(const SmartPointerTypeEntryCPtr &te)
{
     return u"Getter \""_s + te->getter() +  u"()\" of smart pointer \""_s
         + te->name() + u"\" not found."_s;
}

QString msgCannotFindSmartPointerMethod(const SmartPointerTypeEntryCPtr &te, const QString &m)
{
    return u"Method \""_s + m +  u"()\" of smart pointer \""_s
        + te->name() + u"\" not found."_s;
}

QString msgMethodNotFound(const AbstractMetaClassCPtr &klass, const QString &name)
{
     return u"Method \""_s + name + u"\" not found in class "_s
            + klass->name() + u'.';
}

// main.cpp

QString msgLeftOverArguments(const QString &remainingArgs, const QStringList &argV)
{
    QString message;
    QTextStream str(&message);
    str << "shiboken: Unprocessed arguments: " << remainingArgs
        << "\nCommand line: " << argV.join(u' ');
    return message;
}

QString msgInvalidVersion(const QString &package, const QString &version)
{
    return u"Invalid version \""_s + version
        + u"\" specified for package "_s + package + u'.';
}

QString msgCyclicDependency(const QString &funcName, const QString &graphName,
                            const AbstractMetaFunctionCList &cyclic,
                            const AbstractMetaFunctionCList &involvedConversions)
{
    QString result;
    QTextStream str(&result);
    str << "Cyclic dependency found on overloaddata for \"" << funcName
         << "\" method! The graph boy saved the graph at \"" << QDir::toNativeSeparators(graphName)
         << "\". Cyclic functions:";
    for (const auto &c : cyclic)
        str << ' ' << c->signature();
    if (const auto count = involvedConversions.size()) {
        str << " Implicit conversions (" << count << "): ";
        for (qsizetype i = 0; i < count; ++i) {
            if (i)
                str << ", \"";
            str << involvedConversions.at(i)->signature() << '"';
            if (const auto c = involvedConversions.at(i)->implementingClass())
                str << '(' << c->name() << ')';
        }
    }
    return result;
}

// shibokengenerator.cpp

QString msgClassNotFound(const TypeEntryCPtr &t)
{
    return u"Could not find class \""_s
           + t->qualifiedCppName()
           + u"\" in the code model. Maybe it is forward declared?"_s;
}

QString msgEnclosingClassNotFound(const TypeEntryCPtr &t)
{
    QString result;
    QTextStream str(&result);
    str << "Warning: Enclosing class \"" << t->parent()->name()
        << "\" of class \"" << t->name() << "\" not found.";
    return result;
}

QString msgUnknownOperator(const AbstractMetaFunction *func)
{
    QString result = u"Unknown operator: \""_s + func->originalName()
                     + u'"';
    if (const auto c = func->implementingClass())
        result += u" in class: "_s + c->name();
    return result;
}

QString msgWrongIndex(const char *varName, const QString &capture,
                      const AbstractMetaFunction *func)
{
    QString result;
    QTextStream str(&result);
    str << "Wrong index for " << varName << " variable (" << capture << ") on ";
    if (const auto c = func->implementingClass())
        str << c->name() << "::";
    str << func->signature();
    return  result;
}

QString msgCannotFindType(const QString &type, const QString &variable,
                          const QString &why)
{
    QString result;
    QTextStream(&result) << "Could not find type '"
        << type << "' for use in '" << variable << "' conversion: " << why
        << "\nMake sure to use the full C++ name, e.g. 'Namespace::Class'.";
    return result;
}

QString msgCannotBuildMetaType(const QString &s)
{
    return u"Unable to build meta type for \""_s + s + u"\": "_s;
}

QString msgCouldNotFindMinimalConstructor(const QString &where, const QString &type, const QString &why)
{
    QString result;
    QTextStream str(&result);
    str << where << ": Could not find a minimal constructor for type '" << type << '\'';
    if (why.isEmpty())
        str << '.';
    else
        str << ": " << why << ' ';
    str << "This will result in a compilation error.";
    return result;
}

// typedatabase.cpp

QString msgRejectReason(const TypeRejection &r, const QString &needle)
{
    QString result;
    QTextStream str(&result);
    switch (r.matchType) {
    case TypeRejection::ExcludeClass:
        str << "matches class exclusion \"" << r.className.pattern() << '"';
        break;
    case TypeRejection::Function:
    case TypeRejection::Field:
    case TypeRejection::Enum:
        str << "matches class \"" << r.className.pattern() << "\" and \""
            << r.pattern.pattern() << '"';
        break;
    case TypeRejection::ArgumentType:
    case TypeRejection::ReturnType:
        str << "matches class \"" << r.className.pattern() << "\" and \""
            << needle << "\" matches \"" << r.pattern.pattern() << '"';
        break;
    }
    return result;
}

// typesystem.cpp

QString msgCannotFindNamespaceToExtend(const QString &name,
                                       const QString &extendsPackage)
{
    return u"Cannot find namespace "_s + name
        + u" in package "_s + extendsPackage;
}

QString msgExtendingNamespaceRequiresPattern(const QString &name)
{
    return u"Namespace "_s + name
        + u" requires a file pattern since it extends another namespace."_s;
}

QString msgInvalidRegularExpression(const QString &pattern, const QString &why)
{
    return u"Invalid pattern \""_s + pattern + u"\": "_s + why;
}

QString msgNoRootTypeSystemEntry()
{
    return u"Type system entry appears out of order, there does not seem to be a root type system element."_s;
}

QString msgIncorrectlyNestedName(const QString &name)
{
    return u"Nesting types by specifying '::' is no longer supported ("_s
           + name + u")."_s;
}

QString msgCannotFindView(const QString &viewedName, const QString &name)
{
    return u"Unable to find viewed type "_s + viewedName
        + u" for "_s + name;
}

QString msgCannotFindSnippet(const QString &file, const QString &snippetLabel)
{
    QString result;
    QTextStream str(&result);
    str << "Cannot find snippet \"" << snippetLabel << "\" in "
        << QDir::toNativeSeparators(file) << '.';
    return result;
}

QString msgSnippetError(const QString &context, const char *what)
{
    return "Error processing code snippet of "_L1 + context
           + ": "_L1 + QString::fromUtf8(what);
}

QString msgUnableToResolveTypedef(const QString &sourceType, const QString &sourceName)
{
    QString result;
    QTextStream(&result) << "Unable to resolve typedef \"" << sourceType
        << "\": Could not find a value, container, object or smart pointer type named \""
        << sourceName << "\".";
    return result;
}

// cppgenerator.cpp

QString msgPureVirtualFunctionRemoved(const AbstractMetaFunction *f)
{
    QString result;
    auto klass = f->ownerClass();
    QTextStream str(&result);
    str << klass->sourceLocation() << "Pure virtual method '" << klass->name()
        << "::"<<  f->minimalSignature()
        << "' must be implemented but was completely removed on type system.";
    return result;
}

QString msgUnknownTypeInArgumentTypeReplacement(const QString &typeReplaced,
                                                const AbstractMetaFunction *f)
{
    QString result;
    QTextStream str(&result);
    if (auto klass = f->ownerClass())
        str << klass->sourceLocation();
    str << "Unknown type '" << typeReplaced
        << "' used as argument type replacement in function '" << f->signature()
        << "', the generated code may be broken.";
    return result;
}

QString msgDuplicateBuiltInTypeEntry(const QString &name)
{
    return u"A type entry duplicating the built-in type \""_s
           + name + u"\" was found. It is ignored."_s;
}

QString msgDuplicateTypeEntry(const QString &name)
{
    return u"Duplicate type entry: '"_s + name + u"'."_s;
}

QString msgInvalidTargetLanguageApiName(const QString &name)
{
    return u"Invalid target language API name \""_s
           + name + u"\"."_s;
}

QString msgUnknownCheckFunction(const TypeEntryCPtr &t)
{
     return u"Unknown check function for type: '"_s
            + t->qualifiedCppName() + u"'."_s;
}

QString msgArgumentClassNotFound(const AbstractMetaFunctionCPtr &func,
                                 const TypeEntryCPtr &t)
{
    QString result;
    QTextStream(&result) << "Internal Error: Class \"" <<  t->qualifiedCppName()
        << "\" for \"" << func->classQualifiedSignature() << "\" not found!";
    return result;
}

QString msgMissingCustomConversion(const TypeEntryCPtr &t)
{
    QString result;
    QTextStream(&result) << "Entry \"" << t->qualifiedCppName()
        << "\" is missing a custom conversion.";
    return result;
}

QString msgUnknownArrayPointerConversion(const QString &s)
{
    return u"Warning: Falling back to pointer conversion for unknown array type \""_s
        + s + u"\""_s;
}

QString msgMissingProjectFileMarker(const QString &name, const QByteArray &startMarker)
{
    return u"First line of project file \""_s + QDir::toNativeSeparators(name)
        + u"\" must be the string \""_s + QString::fromLatin1(startMarker) + u"\"."_s;
}

QString msgInvalidLanguageLevel(const QString &l)
{
    return u"Invalid argument for language level: \""_s + l + u"\"."_s;
}

QString msgCannotFindImage(const QString &href, const QString &context,
                           const QStringList &candidates)
{
    QString result = "Cannot resolve image "_L1 + href + " for "_L1 + context
                     + " (tried "_L1;
    for (const auto &candidate : candidates)
        result += QDir::toNativeSeparators(candidate) + u' ';
    result += ")."_L1;
    return result;
}

QString msgNotRelative(const QString &path, const QString &dir)
{
    return QDir::toNativeSeparators(path)
           + " is not under "_L1 + QDir::toNativeSeparators(dir) + u'.';
}

QString msgCannotCreateDir(const QString &dir)
{
    return "Cannot create "_L1 + QDir::toNativeSeparators(dir) + u'.';
}

QString msgCannotCopy(const QFile &source, const QString &target)
{
    return "Cannot copy "_L1 + QDir::toNativeSeparators(source.fileName())
        + " to "_L1 + QDir::toNativeSeparators(target)
        + ": "_L1 + source.errorString();
}
