// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "abstractmetafunction.h"
#include "abstractmetaargument.h"
#include "abstractmetabuilder.h"
#include "abstractmetalang.h"
#include "abstractmetalang_helpers.h"
#include "abstractmetatype.h"
#include "addedfunction.h"
#include <codemodel.h>
#include "documentation.h"
#include "exception.h"
#include "messages.h"
#include "codesnip.h"
#include "modifications.h"
#include "reporthandler.h"
#include "sourcelocation.h"
#include "typedatabase.h"
#include "complextypeentry.h"
#include "containertypeentry.h"
#include "functiontypeentry.h"
#include "primitivetypeentry.h"
#include "typesystemtypeentry.h"

#include "qtcompat.h"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

// Cache FunctionModificationList in a flat list per class (0 for global
// functions, or typically owner/implementing/declaring class.
struct ModificationCacheEntry
{
     AbstractMetaClassCPtr klass;
     FunctionModificationList modifications;
};

using ModificationCache = QList<ModificationCacheEntry>;

class AbstractMetaFunctionPrivate
{
public:
    AbstractMetaFunctionPrivate()
        : m_constant(false),
          m_reverse(false),
          m_pointerOperator(false),
          m_isCallOperator(false)
    {
    }

    QString signature() const;
    QString formatMinimalSignature(const AbstractMetaFunction *q,
                                   bool comment) const;
    QString modifiedName(const AbstractMetaFunction *q) const;
    int overloadNumber(const AbstractMetaFunction *q) const;

    const FunctionModificationList &modifications(const AbstractMetaFunction *q,
                                                  const AbstractMetaClassCPtr &implementor) const;

    bool applyTypeModification(const AbstractMetaFunction *q,
                               const QString &type, int number, QString *errorMessage);

    QString m_name;
    QString m_originalName;
    Documentation m_doc;
    mutable QString m_cachedMinimalSignature;
    mutable QString m_cachedSignature;
    mutable QString m_cachedModifiedName;
    QString m_unresolvedSignature;

    FunctionTypeEntryPtr m_typeEntry;
    AbstractMetaFunction::FunctionType m_functionType = AbstractMetaFunction::NormalFunction;
    AbstractMetaType m_type;
    QString m_modifiedTypeName;
    AbstractMetaClassCPtr m_class;
    AbstractMetaClassCPtr m_implementingClass;
    AbstractMetaClassCPtr m_declaringClass;
    mutable ModificationCache m_modificationCache;
    int m_propertySpecIndex = -1;
    AbstractMetaArgumentList m_arguments;
    AddedFunctionPtr m_addedFunction;
    SourceLocation m_sourceLocation;
    AbstractMetaFunction::Attributes m_attributes;
    FunctionAttributes m_cppAttributes;
    AbstractMetaFunction::Flags m_flags;
    uint m_constant                 : 1;
    uint m_reverse                  : 1;
    uint m_pointerOperator          : 1;
    uint m_isCallOperator           : 1;
    mutable int m_cachedOverloadNumber = TypeSystem::OverloadNumberUnset;
    Access m_access = Access::Public;
    Access m_originalAccess = Access::Public;
    ExceptionSpecification m_exceptionSpecification = ExceptionSpecification::Unknown;
    TypeSystem::AllowThread m_allowThreadModification = TypeSystem::AllowThread::Unspecified;
    TypeSystem::ExceptionHandling m_exceptionHandlingModification = TypeSystem::ExceptionHandling::Unspecified;
};

AbstractMetaFunction::AbstractMetaFunction(const QString &name) :
    AbstractMetaFunction()
{
    d->m_originalName = d->m_name = name;
}

AbstractMetaFunction::AbstractMetaFunction(const AddedFunctionPtr &addedFunc) :
    AbstractMetaFunction(addedFunc->name())
{
    d->m_addedFunction = addedFunc;
    setConstant(addedFunc->isConstant());
    switch (addedFunc->access()) {
    case AddedFunction::Protected:
        setAccess(Access::Protected);
        break;
    case AddedFunction::Public:
        setAccess(Access::Public);
        break;
    }
    AbstractMetaFunction::Attributes atts;
    if (addedFunc->isStatic())
        setCppAttribute(FunctionAttribute::Static);
    if (addedFunc->isClassMethod())
        atts |= AbstractMetaFunction::ClassMethod;
    setAttributes(atts);
}

QString AbstractMetaFunction::name() const
{
    return d->m_name;
}

void AbstractMetaFunction::setName(const QString &name)
{
    d->m_name = name;
}

QString AbstractMetaFunction::originalName() const
{
    return d->m_originalName.isEmpty() ? name() : d->m_originalName;
}

void AbstractMetaFunction::setOriginalName(const QString &name)
{
    d->m_originalName = name;
}

Access AbstractMetaFunction::access() const
{
    return d->m_access;
}

void AbstractMetaFunction::setAccess(Access a)
{
    d->m_originalAccess = d->m_access = a;
}

void AbstractMetaFunction::modifyAccess(Access a)
{
    d->m_access = a;
}

bool AbstractMetaFunction::wasPrivate() const
{
    return d->m_originalAccess == Access::Private;
}

bool AbstractMetaFunction::wasProtected() const
{
    return d->m_originalAccess == Access::Protected;
}

bool AbstractMetaFunction::wasPublic() const
{
    return d->m_originalAccess == Access::Public;
}

QStringList AbstractMetaFunction::definitionNames() const
{
    return AbstractMetaBuilder::definitionNames(d->m_name, snakeCase());
}

const Documentation &AbstractMetaFunction::documentation() const
{
    return d->m_doc;
}

void AbstractMetaFunction::setDocumentation(const Documentation &doc)
{
    d->m_doc = doc;
}

bool AbstractMetaFunction::isReverseOperator() const
{
    return d->m_reverse;
}

void AbstractMetaFunction::setReverseOperator(bool reverse)
{
    d->m_reverse = reverse;
}

bool AbstractMetaFunction::isPointerOperator() const
{
    return d->m_pointerOperator;
}

void AbstractMetaFunction::setPointerOperator(bool value)
{
    d->m_pointerOperator = value;
}

bool AbstractMetaFunction::isExplicit() const
{
    return d->m_cppAttributes.testFlag(FunctionAttribute::Explicit);
}

void AbstractMetaFunction::setExplicit(bool isExplicit)
{
    d->m_cppAttributes.setFlag(FunctionAttribute::Explicit, isExplicit);
}

bool AbstractMetaFunction::returnsBool() const
{
    if (d->m_type.typeUsagePattern() != AbstractMetaType::PrimitivePattern)
        return false;
    return basicReferencedTypeEntry(d->m_type.typeEntry())->name() == u"bool";
}

bool AbstractMetaFunction::isOperatorBool() const
{
    return d->m_functionType == AbstractMetaFunction::ConversionOperator
        && d->m_constant && returnsBool();
}

AbstractMetaFunction::AbstractMetaFunction() : d(new AbstractMetaFunctionPrivate)
{
}

AbstractMetaFunction::~AbstractMetaFunction() = default;

AbstractMetaFunction::Attributes AbstractMetaFunction::attributes() const
{
    return d->m_attributes;
}

void AbstractMetaFunction::setAttributes(Attributes attributes)
{
    d->m_attributes = attributes;
}

void AbstractMetaFunction::operator+=(AbstractMetaFunction::Attribute attribute)
{
    d->m_attributes.setFlag(attribute);
}

void AbstractMetaFunction::operator-=(AbstractMetaFunction::Attribute attribute)
{
    d->m_attributes.setFlag(attribute, false);
}

FunctionAttributes AbstractMetaFunction::cppAttributes() const
{
    return d->m_cppAttributes;
}

void AbstractMetaFunction::setCppAttributes(FunctionAttributes a)
{
    d->m_cppAttributes = a;
}

void AbstractMetaFunction::setCppAttribute(FunctionAttribute a, bool on)
{
    d->m_cppAttributes.setFlag(a, on);
}

AbstractMetaFunction::Flags AbstractMetaFunction::flags() const
{
    return d->m_flags;
}

void AbstractMetaFunction::setFlags(Flags f)
{
    d->m_flags = f;
}

/*******************************************************************************
 * Indicates that this function has a modification that removes it
 */

static bool fmIsRemoved(const FunctionModification &mod) { return mod.isRemoved(); }

bool AbstractMetaFunction::isModifiedRemoved(AbstractMetaClassCPtr cls) const
{
    if (!isInGlobalScope() && !cls)
        cls = d->m_implementingClass;
    const auto &mods = modifications(cls);
    return std::any_of(mods.cbegin(), mods.cend(), fmIsRemoved);
}

static bool isFinal(const FunctionModification &mod)
{
    return mod.modifiers().testFlag(FunctionModification::Final);
}

bool AbstractMetaFunction::isModifiedFinal(AbstractMetaClassCPtr cls) const
{
    if (!isInGlobalScope() && cls == nullptr)
        cls = d->m_implementingClass;
    const auto &mods = modifications(cls);
    return std::any_of(mods.cbegin(), mods.cend(), isFinal);
}

bool AbstractMetaFunction::isVoid() const
{
    return d->m_type.isVoid();
}

const AbstractMetaType &AbstractMetaFunction::type() const
{
    return d->m_type;
}

void AbstractMetaFunction::setType(const AbstractMetaType &type)
{
    d->m_type = type;
}

AbstractMetaClassCPtr AbstractMetaFunction::ownerClass() const
{
    return d->m_class;
}

void AbstractMetaFunction::setOwnerClass(const AbstractMetaClassCPtr &cls)
{
    d->m_class = cls;
}

bool AbstractMetaFunction::operator<(const AbstractMetaFunction &other) const
{
    return compareTo(&other) & NameLessThan;
}


/*!
    Returns a mask of CompareResult describing how this function is
    compares to another function
*/
AbstractMetaFunction::CompareResult AbstractMetaFunction::compareTo(const AbstractMetaFunction *other) const
{
    CompareResult result;

    // Enclosing class...
    if (ownerClass() == other->ownerClass())
        result |= EqualImplementor;

    // Attributes
    if (attributes() == other->attributes() && cppAttributes() == other->cppAttributes())
        result |= EqualAttributes;

    // Compare types
    if (type().name() == other->type().name())
        result |= EqualReturnType;

    // Compare names
    int cmp = originalName().compare(other->originalName());

    if (cmp < 0)
        result |= NameLessThan;
    else if (!cmp)
        result |= EqualName;

    // compare name after modification...
    cmp = modifiedName().compare(other->modifiedName());
    if (!cmp)
        result |= EqualModifiedName;

    // Compare arguments...
    AbstractMetaArgumentList minArguments;
    AbstractMetaArgumentList maxArguments;
    if (arguments().size() < other->arguments().size()) {
        minArguments = arguments();
        maxArguments = other->arguments();
    } else {
        minArguments = other->arguments();
        maxArguments = arguments();
    }

    const auto minCount = minArguments.size();
    const auto maxCount = maxArguments.size();
    bool same = true;
    for (qsizetype i = 0; i < maxCount; ++i) {
        if (i < minCount) {
            const AbstractMetaArgument &min_arg = minArguments.at(i);
            const AbstractMetaArgument &max_arg = maxArguments.at(i);
            if (min_arg.type().name() != max_arg.type().name()
                && (min_arg.defaultValueExpression().isEmpty() || max_arg.defaultValueExpression().isEmpty())) {
                same = false;
                break;
            }
        } else {
            if (maxArguments.at(i).defaultValueExpression().isEmpty()) {
                same = false;
                break;
            }
        }
    }

    if (same)
        result |= minCount == maxCount ? EqualArguments : EqualDefaultValueOverload;

    return result;
}

// Is this the const overload of another function of equivalent return type?
bool AbstractMetaFunction::isConstOverloadOf(const AbstractMetaFunction *other) const
{
    const auto argumentCount = d->m_arguments.size();
    if (!isConstant() || other->isConstant() || name() != other->name()
        || argumentCount != other->arguments().size()) {
        return false;
    }

    // Match "const Foo &getFoo() const" / "Foo &getFoo()" / "Foo getFoo() const"
    const auto &otherType = other->type();
    if (d->m_type.name() != otherType.name()
        || d->m_type.indirectionsV() != otherType.indirectionsV()) {
        return false;
    }

    const auto &otherArguments = other->arguments();
    for (qsizetype a = 0; a < argumentCount; ++a) {
        if (d->m_arguments.at(a).type() != otherArguments.at(a).type())
            return false;
    }
    return true;
}

AbstractMetaFunction *AbstractMetaFunction::copy() const
{
    auto *cpy = new AbstractMetaFunction;
    cpy->setAttributes(attributes());
    auto ca = cppAttributes();
    // Historical bug: explicit was not copied! (causing nontypetemplate_test.py fail)
    ca.setFlag(FunctionAttribute::Explicit, false);
    cpy->setCppAttributes(ca);
    cpy->setFlags(flags());
    cpy->setAccess(access());
    cpy->setName(name());
    cpy->setOriginalName(originalName());
    cpy->setOwnerClass(ownerClass());
    cpy->setImplementingClass(implementingClass());
    cpy->setFunctionType(functionType());
    cpy->setDeclaringClass(declaringClass());
    cpy->setType(type());
    cpy->setConstant(isConstant());
    cpy->setExceptionSpecification(d->m_exceptionSpecification);
    cpy->setAllowThreadModification(d->m_allowThreadModification);
    cpy->setExceptionHandlingModification(d->m_exceptionHandlingModification);
    cpy->d->m_modifiedTypeName = d->m_modifiedTypeName;
    cpy->d->m_addedFunction = d->m_addedFunction;
    cpy->d->m_arguments = d->m_arguments;

    return cpy;
}

static bool usesRValueReference(const AbstractMetaArgument &a)
{
    return a.type().referenceType() == RValueReference;
}

bool AbstractMetaFunction::usesRValueReferences() const
{
    return d->m_functionType == MoveConstructorFunction
        || d->m_functionType == MoveAssignmentOperatorFunction
        || d->m_type.referenceType() == RValueReference
        || std::any_of(d->m_arguments.cbegin(), d->m_arguments.cend(), usesRValueReference);
}

bool AbstractMetaFunction::generateBinding() const
{
    switch (d->m_functionType) {
    case ConversionOperator:
        if (d->m_name != u"operator int" && d->m_name != u"operator double")
            return false;
        break;
    case AssignmentOperatorFunction:
    case MoveAssignmentOperatorFunction:
    case AbstractMetaFunction::MoveConstructorFunction:
        return false;
    default:
        if (!isWhiteListed())
            return false;
        break;
    }
    // Can we access the wrapper in case of a protected method? If not,
    // disable for consistency regardless of avoidProtectedHack.
    if (isProtected()) {
        const auto typeFlags = ownerClass()->typeEntry()->typeFlags();
        if (typeFlags.testFlag(ComplexTypeEntry::DisableWrapper))
            return false;
    }
    if (isPrivate() && d->m_functionType != EmptyFunction)
        return false;
    // RValue references only for user-specified
    // functions (<add-function>/<declare-function>/<function>)
    return d->m_name != u"qt_metacall" &&
           (!usesRValueReferences() || d->m_addedFunction || d->m_typeEntry)
           && !isModifiedRemoved();
}

bool AbstractMetaFunction::isWhiteListed() const
{
    switch (d->m_functionType) {
    case NormalFunction:
    case SignalFunction:
    case SlotFunction:
        if (auto dc = declaringClass()) {
            const QSet<QString> &whiteList = dc->typeEntry()->generateFunctions();
            return whiteList.isEmpty() || whiteList.contains(d->m_name)
                   || whiteList.contains(minimalSignature());
        }
        break;
    default:
        break;
    }
    return true;
}

QString AbstractMetaFunctionPrivate::signature() const
{
    if (m_cachedSignature.isEmpty()) {
        m_cachedSignature = m_originalName;

        m_cachedSignature += u'(';

        for (qsizetype i = 0; i < m_arguments.size(); ++i) {
            const AbstractMetaArgument &a = m_arguments.at(i);
            const AbstractMetaType &t = a.type();
            if (i > 0)
                m_cachedSignature += u", "_s;
            m_cachedSignature += t.cppSignature();
            // We need to have the argument names in the qdoc files
            if (!m_cachedSignature.endsWith(u'*') && !m_cachedSignature.endsWith(u'&'))
                m_cachedSignature += u' ';
            m_cachedSignature += a.name();
        }
        m_cachedSignature += u')';

        if (m_constant)
            m_cachedSignature += u" const"_s;
    }
    return m_cachedSignature;
}

QString AbstractMetaFunction::signature() const
{
    return d->signature();
}

QString AbstractMetaFunction::classQualifiedSignature() const
{
    QString result;
    if (d->m_implementingClass)
        result += d->m_implementingClass->qualifiedCppName() + u"::"_s;
    result += signature();
    return result;
}

QString AbstractMetaFunction::unresolvedSignature() const
{
    return d->m_unresolvedSignature;
}

void AbstractMetaFunction::setUnresolvedSignature(const QString &s)
{
    d->m_unresolvedSignature = s;
}

bool AbstractMetaFunction::isConstant() const
{
    return d->m_constant;
}

void AbstractMetaFunction::setConstant(bool constant)
{
    d->m_constant = constant;
}

bool AbstractMetaFunction::isUserAdded() const
{
    return d->m_addedFunction && !d->m_addedFunction->isDeclaration();
}

bool AbstractMetaFunction::isUserAddedPythonOverride() const
{
    return d->m_addedFunction && d->m_addedFunction->isPythonOverride();
}

bool AbstractMetaFunction::isUserDeclared() const
{
    return d->m_addedFunction && d->m_addedFunction->isDeclaration();
}

int AbstractMetaFunction::actualMinimumArgumentCount() const
{
    int count = 0;
    for (qsizetype i = 0, size = d->m_arguments.size(); i < size; ++i && ++count) {
        const auto &arg = d->m_arguments.at(i);
        if (arg.isModifiedRemoved())
            --count;
        else if (!arg.defaultValueExpression().isEmpty())
            break;
    }

    return count;
}

int AbstractMetaFunction::actualArgumentIndex(int index) const
{
    if (index < 0 || index >= int(d->m_arguments.size()))
        throw Exception(msgArgumentIndexOutOfRange(this, index));
    int result = 0;
    for (int i = 0; i < index; ++i) {
        if (!d->m_arguments.at(i).isModifiedRemoved())
            ++result;
    }
    return result;
}

// Returns reference counts for argument at idx, or all arguments if idx == -2
QList<ReferenceCount> AbstractMetaFunction::referenceCounts(const AbstractMetaClassCPtr &cls, int idx) const
{
    QList<ReferenceCount> returned;

    for (const auto &mod : modifications(cls)) {
        for (const ArgumentModification &argumentMod : mod.argument_mods()) {
            if (argumentMod.index() != idx && idx != -2)
                continue;
            returned += argumentMod.referenceCounts();
        }
    }

    return returned;
}

ArgumentOwner AbstractMetaFunction::argumentOwner(const AbstractMetaClassCPtr &cls, int idx) const
{
    for (const auto &mod : modifications(cls)) {
        for (const ArgumentModification &argumentMod : mod.argument_mods()) {
            if (argumentMod.index() != idx)
                continue;
            return argumentMod.owner();
        }
    }
    return {};
}

QString AbstractMetaFunction::conversionRule(TypeSystem::Language language, int key) const
{
    for (const auto &modification : modifications(declaringClass())) {
        for (const ArgumentModification &argumentModification : modification.argument_mods()) {
            if (argumentModification.index() != key)
                continue;

            for (const CodeSnip &snip : argumentModification.conversionRules()) {
                if (snip.language == language && !snip.code().isEmpty())
                    return snip.code();
            }
        }
    }

    return {};
}

bool AbstractMetaFunction::hasConversionRule(TypeSystem::Language language, int idx) const
{
    return !conversionRule(language, idx).isEmpty();
}

// FIXME If we remove a arg. in the method at the base class, it will not reflect here.
bool AbstractMetaFunction::argumentRemoved(int key) const
{
    for (const auto &modification : modifications(declaringClass())) {
        for (const ArgumentModification &argumentModification : modification.argument_mods()) {
            if (argumentModification.index() == key) {
                if (argumentModification.isRemoved())
                    return true;
            }
        }
    }

    return false;
}

AbstractMetaClassCPtr AbstractMetaFunction::targetLangOwner() const
{
    return d->m_class && d->m_class->isInvisibleNamespace()
        ?  d->m_class->targetLangEnclosingClass() : d->m_class;
}

AbstractMetaClassCPtr AbstractMetaFunction::declaringClass() const
{
    return d->m_declaringClass;
}

void AbstractMetaFunction::setDeclaringClass(const AbstractMetaClassCPtr &cls)
{
    d->m_declaringClass = cls;
}

AbstractMetaClassCPtr AbstractMetaFunction::implementingClass() const
{
    return d->m_implementingClass;
}

void AbstractMetaFunction::setImplementingClass(const AbstractMetaClassCPtr &cls)
{
    d->m_implementingClass = cls;
}

const AbstractMetaArgumentList &AbstractMetaFunction::arguments() const
{
    return d->m_arguments;
}

AbstractMetaArgumentList &AbstractMetaFunction::arguments()
{
    return d->m_arguments;
}

void AbstractMetaFunction::setArguments(const AbstractMetaArgumentList &arguments)
{
    d->m_arguments = arguments;
}

void AbstractMetaFunction::addArgument(const AbstractMetaArgument &argument)
{
    d->m_arguments << argument;
}

static bool modifiedDeprecated(const FunctionModification &mod)
{
    return mod.modifiers().testFlag(FunctionModification::Deprecated);
}

static bool modifiedUndeprecated(const FunctionModification &mod)
{
    return mod.modifiers().testFlag(FunctionModification::Undeprecated);
}

bool AbstractMetaFunction::isDeprecated() const
{
    const auto &mods = modifications(declaringClass());

    return d->m_cppAttributes.testFlag(FunctionAttribute::Deprecated)
           ? std::none_of(mods.cbegin(), mods.cend(), modifiedUndeprecated)
           : std::any_of(mods.cbegin(), mods.cend(), modifiedDeprecated);
}

bool AbstractMetaFunction::isConstructor() const
{
    return d->m_functionType == ConstructorFunction || d->m_functionType == CopyConstructorFunction
            || d->m_functionType == MoveConstructorFunction;
}

bool AbstractMetaFunction::isDefaultConstructor() const
{
    return d->m_functionType == ConstructorFunction
        && (d->m_arguments.isEmpty()
            || d->m_arguments.constFirst().hasDefaultValueExpression());
}

bool AbstractMetaFunction::needsReturnType() const
{
    switch (d->m_functionType) {
    case AbstractMetaFunction::ConstructorFunction:
    case AbstractMetaFunction::CopyConstructorFunction:
    case AbstractMetaFunction::MoveConstructorFunction:
    case AbstractMetaFunction::DestructorFunction:
        return false;
    default:
        break;
    }
    return true;
}

bool AbstractMetaFunction::isInGlobalScope() const
{
    return d->m_class == nullptr;
}

AbstractMetaFunction::FunctionType AbstractMetaFunction::functionType() const
{
    return d->m_functionType;
}

void AbstractMetaFunction::setFunctionType(AbstractMetaFunction::FunctionType type)
{
    d->m_functionType = type;
}

std::optional<AbstractMetaFunction::ComparisonOperatorType>
AbstractMetaFunction::comparisonOperatorType() const
{
    if (d->m_functionType != ComparisonOperator)
        return {};
    static const QHash<QString, ComparisonOperatorType> mapping = {
        {u"operator=="_s, OperatorEqual},
        {u"operator!="_s, OperatorNotEqual},
        {u"operator<"_s, OperatorLess},
        {u"operator<="_s, OperatorLessEqual},
        {u"operator>"_s, OperatorGreater},
        {u"operator>="_s, OperatorGreaterEqual}
    };
    const auto it = mapping.constFind(originalName());
    Q_ASSERT(it != mapping.constEnd());
    return it.value();
}

// Auto-detect whether a function should be wrapped into
// Py_BEGIN_ALLOW_THREADS/Py_END_ALLOW_THREADS, that is, temporarily release
// the GIL (global interpreter lock). Doing so is required for any thread-wait
// functions, anything that might call a virtual function (potentially
// reimplemented in Python), and recommended for lengthy I/O or similar.
// It has performance costs, though.
bool AbstractMetaFunction::autoDetectAllowThread() const
{
    // Disallow for simple getter functions.
    return !maybeAccessor();
}

bool AbstractMetaFunction::maybeAccessor() const
{
     return d->m_functionType == NormalFunction && d->m_class != nullptr
        && d->m_constant != 0 && !isVoid() && d->m_arguments.isEmpty();
}

SourceLocation AbstractMetaFunction::sourceLocation() const
{
    return d->m_sourceLocation;
}

void AbstractMetaFunction::setSourceLocation(const SourceLocation &sourceLocation)
{
    d->m_sourceLocation = sourceLocation;
}

static inline TypeSystem::AllowThread allowThreadMod(const AbstractMetaClassCPtr &klass)
{
    return klass->typeEntry()->allowThread();
}

static inline bool hasAllowThreadMod(const AbstractMetaClassCPtr &klass)
{
    return allowThreadMod(klass) != TypeSystem::AllowThread::Unspecified;
}

bool AbstractMetaFunction::allowThread() const
{
    auto allowThreadModification = d->m_allowThreadModification;
    // If there is no modification on the function, check for a base class.
    if (d->m_class && allowThreadModification == TypeSystem::AllowThread::Unspecified) {
        if (auto base = recurseClassHierarchy(d->m_class, hasAllowThreadMod))
            allowThreadModification = allowThreadMod(base);
    }

    bool result = true;
    switch (allowThreadModification) {
    case TypeSystem::AllowThread::Disallow:
        result = false;
        break;
    case TypeSystem::AllowThread::Allow:
        break;
    case TypeSystem::AllowThread::Auto:
        result = autoDetectAllowThread();
        break;
    case TypeSystem::AllowThread::Unspecified:
        result = false;
        break;
    }
    if (!result && ReportHandler::isDebug(ReportHandler::MediumDebug))
        qCInfo(lcShiboken).noquote() << msgDisallowThread(this);
    return result;
}

TypeSystem::Ownership AbstractMetaFunction::argumentTargetOwnership(const AbstractMetaClassCPtr &cls, int idx) const
{
    for (const auto &modification : modifications(cls)) {
        for (const ArgumentModification &argumentModification : modification.argument_mods()) {
            if (argumentModification.index() == idx)
                return argumentModification.targetOwnerShip();
        }
    }

    return TypeSystem::UnspecifiedOwnership;
}

const QString &AbstractMetaFunction::modifiedTypeName() const
{
    return d->m_modifiedTypeName;
}

bool AbstractMetaFunction::generateOpaqueContainerReturn() const
{
    if (!isTypeModified() || d->m_type.typeUsagePattern() != AbstractMetaType::ContainerPattern)
        return false;
    // Needs to be a reference to a container, allow by value only for spans
    if (d->m_type.referenceType() != LValueReference) {
        auto cte = std::static_pointer_cast<const ContainerTypeEntry>(d->m_type.typeEntry());
        if (cte->containerKind() != ContainerTypeEntry::SpanContainer)
            return false;
    }
    return d->m_type.generateOpaqueContainerForGetter(d->m_modifiedTypeName);
}

bool AbstractMetaFunction::isModifiedToArray(int argumentIndex) const
{
    for (const auto &modification : modifications(declaringClass())) {
        for (const ArgumentModification &argumentModification : modification.argument_mods()) {
            if (argumentModification.index() == argumentIndex && argumentModification.isArray())
                return true;
        }
    }
    return false;
}

// Note: The declaring class must be correctly set for this to work.
bool AbstractMetaFunctionPrivate::applyTypeModification(const AbstractMetaFunction *q,
                                                        const QString &type,
                                                        int number, QString *errorMessage)
{
    if (number < 0 || number > m_arguments.size()) {
        *errorMessage =
            msgTypeModificationFailed(type, number, q,
                                      msgArgumentOutOfRange(number, 0, m_arguments.size()));
        return false;
    }

    // Modified return types may have unparseable types like Python tuples
    if (number == 0) {
        m_modifiedTypeName = type;
        return true;
    }

    auto typeOpt = AbstractMetaType::fromString(type, errorMessage);
    if (!typeOpt.has_value()) {
        *errorMessage = msgTypeModificationFailed(type, number, q, *errorMessage);
        return false;
    }
    m_arguments[number - 1].setModifiedType(typeOpt.value());
    return true;
}

void AbstractMetaFunction::applyTypeModifications()
{
    QString errorMessage;
    for (const auto &modification : modifications(declaringClass())) {
        for (const ArgumentModification &am : modification.argument_mods()) {
            const  int n = am.index();
            if (am.isTypeModified()
                && !d->applyTypeModification(this, am.modifiedType(),
                                             n, &errorMessage)) {
                throw Exception(errorMessage);
            }
            if (am.isRemoved() && n != 0) {
                if (n < 1 || n > d->m_arguments.size()) {
                    errorMessage =
                        msgArgumentRemovalFailed(this, n,
                                                 msgArgumentOutOfRange(n, 1, d->m_arguments.size()));
                    throw Exception(errorMessage);
                }
                d->m_arguments[n - 1].setModifiedRemoved(true);
            }
        }
    }
}

QString AbstractMetaFunction::pyiTypeReplaced(int argumentIndex) const
{
    for (const auto &modification : modifications(declaringClass())) {
        for (const ArgumentModification &argumentModification : modification.argument_mods()) {
            if (argumentModification.index() == argumentIndex) {
                QString type = argumentModification.pyiType();
                if (!type.isEmpty())
                    return type;
                type = argumentModification.modifiedType();
                if (!type.isEmpty())
                    return type;
            }
        }
    }

    return {};
}

// Parameter 'comment' indicates usage as a code comment of the overload decisor
QString AbstractMetaFunctionPrivate::formatMinimalSignature(const AbstractMetaFunction *q,
                                                            bool comment) const
{
    QString result = m_originalName + u'(';
    for (qsizetype i = 0; i < m_arguments.size(); ++i) {
        const auto &argument = m_arguments.at(i);
        if (i > 0)
            result += u',';

        const auto &type = comment ? argument.modifiedType() : argument.type();
        result += type.minimalSignature();
        if (comment && argument.hasDefaultValueExpression())
            result += u'=';
    }
    result += u')';
    if (m_constant)
        result += u"const"_s;
    result = TypeDatabase::normalizedSignature(result);

    if (comment && !q->isVoid()) {
        result += u"->"_s;
        result += q->isTypeModified()
                  ? q->modifiedTypeName() : q->type().minimalSignature();
    }
    return result;
}

QString AbstractMetaFunction::minimalSignature() const
{
    if (d->m_cachedMinimalSignature.isEmpty())
        d->m_cachedMinimalSignature = d->formatMinimalSignature(this, false);
    return d->m_cachedMinimalSignature;
}

QStringList AbstractMetaFunction::modificationSignatures() const
{
    QStringList result{minimalSignature()};
    if (d->m_unresolvedSignature != result.constFirst())
        result.append(d->m_unresolvedSignature);
    return result;
}

QString AbstractMetaFunction::signatureComment() const
{
    return d->formatMinimalSignature(this, true);
}

QString AbstractMetaFunction::debugSignature() const
{
    QString result;
    const auto attributes = cppAttributes();
    const bool isOverride = attributes.testFlag(FunctionAttribute::Override);
    const bool isFinal = attributes.testFlag(FunctionAttribute::Final);
    if (!isOverride && !isFinal && (attributes.testFlag(FunctionAttribute::Virtual)))
        result += u"virtual "_s;
    if (d->m_implementingClass)
        result += d->m_implementingClass->qualifiedCppName() + u"::"_s;
    result += minimalSignature();
    if (isOverride)
        result += u" override"_s;
    if (isFinal)
        result += u" final"_s;
    return result;
}

FunctionModificationList AbstractMetaFunction::findClassModifications(const AbstractMetaFunction *f,
                                                                      AbstractMetaClassCPtr implementor)
{
    const auto signatures = f->modificationSignatures();
    FunctionModificationList mods;
    while (implementor) {
        mods += implementor->typeEntry()->functionModifications(signatures);
        if ((implementor == implementor->baseClass()) ||
            (implementor == f->implementingClass() && !mods.isEmpty())) {
                break;
        }
        implementor = implementor->baseClass();
    }
    return mods;
}

FunctionModificationList AbstractMetaFunction::findGlobalModifications(const AbstractMetaFunction *f)
{
    auto *td = TypeDatabase::instance();
    return td->globalFunctionModifications(f->modificationSignatures());
}

const FunctionModificationList &
    AbstractMetaFunctionPrivate::modifications(const AbstractMetaFunction *q,
                                               const AbstractMetaClassCPtr &implementor) const
{
    if (m_addedFunction)
        return m_addedFunction->modifications();
    for (const auto &ce : m_modificationCache) {
        if (ce.klass == implementor)
            return ce.modifications;
    }
    auto modifications = m_class == nullptr
        ? AbstractMetaFunction::findGlobalModifications(q)
        : AbstractMetaFunction::findClassModifications(q, implementor);

    m_modificationCache.append({implementor, modifications});
    return m_modificationCache.constLast().modifications;
}

const FunctionModificationList &
    AbstractMetaFunction::modifications(AbstractMetaClassCPtr implementor) const
{
    if (!implementor)
        implementor = d->m_class;
    return d->modifications(this, implementor);
}

void AbstractMetaFunction::clearModificationsCache()
{
    d->m_modificationCache.clear();
}

DocModificationList AbstractMetaFunction::addedFunctionDocModifications() const
{
    return d->m_addedFunction
        ? d->m_addedFunction->docModifications() : DocModificationList{};
}

QString AbstractMetaFunction::argumentName(int index,
                                           bool /* create */,
                                           const AbstractMetaClassCPtr & /* implementor */) const
{
    return d->m_arguments[--index].name();
}

int AbstractMetaFunction::propertySpecIndex() const
{
    return d->m_propertySpecIndex;
}

void AbstractMetaFunction::setPropertySpecIndex(int i)
{
    d->m_propertySpecIndex = i;
}

FunctionTypeEntryPtr AbstractMetaFunction::typeEntry() const
{
    return d->m_typeEntry;
}

void AbstractMetaFunction::setTypeEntry(const FunctionTypeEntryPtr &typeEntry)
{
    d->m_typeEntry = typeEntry;
}

QString AbstractMetaFunction::targetLangPackage() const
{
    if (d->m_addedFunction != nullptr)
        return d->m_addedFunction->targetLangPackage();
    if (d->m_class != nullptr)
        return d->m_class->typeEntry()->targetLangPackage();
    if (d->m_typeEntry != nullptr)
        return d->m_typeEntry->targetLangPackage();
    return {};
}

bool AbstractMetaFunction::isCallOperator() const
{
    return d->m_name == u"operator()";
}

static bool isCodeInjection(const FunctionModification &mod)
{
    return mod.isCodeInjection();
}

bool AbstractMetaFunction::hasInjectedCode() const
{
    const FunctionModificationList &mods = modifications(ownerClass());
    return std::any_of(mods.cbegin(), mods.cend(), isCodeInjection);
}

// Traverse the code snippets, return true if predicate returns true
template <class Predicate>
bool AbstractMetaFunction::traverseCodeSnips(Predicate predicate,
                                             TypeSystem::CodeSnipPosition position,
                                             TypeSystem::Language language) const
{
    for (const FunctionModification &mod : modifications(ownerClass())) {
        if (mod.isCodeInjection()) {
            for (const CodeSnip &snip : mod.snips()) {
                if ((snip.language & language) != 0
                    && (snip.position == position || position == TypeSystem::CodeSnipPositionAny)
                    && predicate(snip)) {
                    return true;
                }
            }
        }
    }
    return false;
}

CodeSnipList AbstractMetaFunction::injectedCodeSnips(TypeSystem::CodeSnipPosition position,
                                                     TypeSystem::Language language) const
{
    CodeSnipList result;
    traverseCodeSnips([&result] (const CodeSnip &s) {
                           result.append(s);
                           return false;
                      }, position, language);
    return result;
}

bool AbstractMetaFunction::injectedCodeContains(const QRegularExpression &pattern,
                                                TypeSystem::CodeSnipPosition position,
                                                TypeSystem::Language language) const
{
    return traverseCodeSnips([pattern] (const CodeSnip &s) {
                                 return s.code().contains(pattern);
                             }, position, language);
}

bool AbstractMetaFunction::injectedCodeContains(QStringView pattern,
                                                TypeSystem::CodeSnipPosition position,
                                                TypeSystem::Language language) const
{
    return traverseCodeSnips([pattern] (const CodeSnip &s) {
                                 return s.code().contains(pattern);
                             }, position, language);
}

bool AbstractMetaFunction::hasSignatureModifications() const
{
    const FunctionModificationList &mods = modifications();
    for (const FunctionModification &mod : mods) {
        if (mod.isRenameModifier())
            return true;
        for (const ArgumentModification &argmod : mod.argument_mods()) {
            // since zero represents the return type and we're
            // interested only in checking the function arguments,
            // it will be ignored.
            if (argmod.index() > 0)
                return true;
        }
    }
    return false;
}

bool AbstractMetaFunction::isConversionOperator(const QString &funcName)
{
    return funcName.startsWith(u"operator ");
}

ExceptionSpecification AbstractMetaFunction::exceptionSpecification() const
{
    return d->m_exceptionSpecification;
}

void AbstractMetaFunction::setExceptionSpecification(ExceptionSpecification e)
{
    d->m_exceptionSpecification = e;
}

static inline TypeSystem::ExceptionHandling exceptionMod(const AbstractMetaClassCPtr &klass)
{
    return klass->typeEntry()->exceptionHandling();
}

static inline bool hasExceptionMod(const AbstractMetaClassCPtr &klass)
{
    return exceptionMod(klass) != TypeSystem::ExceptionHandling::Unspecified;
}

bool AbstractMetaFunction::generateExceptionHandling() const
{
    switch (d->m_functionType) {
    case AbstractMetaFunction::CopyConstructorFunction:
    case AbstractMetaFunction::MoveConstructorFunction:
    case AbstractMetaFunction::AssignmentOperatorFunction:
    case AbstractMetaFunction::MoveAssignmentOperatorFunction:
    case AbstractMetaFunction::DestructorFunction:
        return false;
    default:
        break;
    }

    auto exceptionHandlingModification = d->m_exceptionHandlingModification;
    // If there is no modification on the function, check for a base class.
    if (d->m_class && exceptionHandlingModification == TypeSystem::ExceptionHandling::Unspecified) {
        if (auto base = recurseClassHierarchy(d->m_class, hasExceptionMod))
            exceptionHandlingModification = exceptionMod(base);
    }

    bool result = false;
    switch (exceptionHandlingModification) {
    case TypeSystem::ExceptionHandling::On:
        result = true;
        break;
    case TypeSystem::ExceptionHandling::AutoDefaultToOn:
        result = d->m_exceptionSpecification != ExceptionSpecification::NoExcept;
        break;
    case TypeSystem::ExceptionHandling::AutoDefaultToOff:
        result = d->m_exceptionSpecification == ExceptionSpecification::Throws;
        break;
    case TypeSystem::ExceptionHandling::Unspecified:
    case TypeSystem::ExceptionHandling::Off:
        break;
    }
    return result;
}

bool AbstractMetaFunction::isConversionOperator() const
{
    return d->m_functionType == ConversionOperator;
}

bool AbstractMetaFunction::isOperatorOverload(const QString &funcName)
{
    if (isConversionOperator(funcName))
        return true;

    static const QRegularExpression opRegEx(u"^operator([+\\-\\*/%=&\\|\\^\\<>!][=]?"
                    "|\\+\\+|\\-\\-|&&|\\|\\||<<[=]?|>>[=]?|~"
                    "|\\[\\]|\\s+delete\\[?\\]?"
                    "|\\(\\)"
                    "|\\s+new\\[?\\]?)$"_s);
    Q_ASSERT(opRegEx.isValid());
    return opRegEx.match(funcName).hasMatch();
}

bool AbstractMetaFunction::isOperatorOverload() const
{
    return d->m_functionType == AssignmentOperatorFunction
        || (d->m_functionType >= FirstOperator && d->m_functionType <= LastOperator);
}

bool AbstractMetaFunction::isArithmeticOperator() const
{
    return d->m_functionType == ArithmeticOperator;
}

bool AbstractMetaFunction::isBitwiseOperator() const
{
    return d->m_functionType == BitwiseOperator
        || d->m_functionType == ShiftOperator;
}

bool AbstractMetaFunction::isComparisonOperator() const
{
    return d->m_functionType == ComparisonOperator;
}

bool AbstractMetaFunction::isSymmetricalComparisonOperator() const
{
    if (d->m_functionType != ComparisonOperator || d->m_class == nullptr)
        return false;
    AbstractMetaType classType(d->m_class->typeEntry());
    classType.decideUsagePattern();
    return std::all_of(d->m_arguments.constBegin(), d->m_arguments.constEnd(),
                       [classType](const AbstractMetaArgument &a) {
                           return a.type().isEquivalent(classType);});
}

bool AbstractMetaFunction::isIncDecrementOperator() const
{
    return d->m_functionType == IncrementOperator
        || d->m_functionType == DecrementOperator;
}
bool AbstractMetaFunction::isLogicalOperator() const
{
    return d->m_functionType == LogicalOperator;
}

bool AbstractMetaFunction::isAssignmentOperator() const
{
    return d->m_functionType == AssignmentOperatorFunction
        || d->m_functionType == MoveAssignmentOperatorFunction;
}

bool AbstractMetaFunction::isGetter() const
{
    return d->m_functionType == NormalFunction && !isVoid()
        && d->m_constant && d->m_access == Access::Public
        && d->m_arguments.isEmpty();
}

bool AbstractMetaFunction::isQtIsNullMethod() const
{
    return isGetter() && d->m_name == u"isNull" && returnsBool();
}

int AbstractMetaFunction::arityOfOperator() const
{
    if (!isOperatorOverload() || isCallOperator())
        return -1;

    int arity = d->m_arguments.size();

    // Operator overloads that are class members
    // implicitly includes the instance and have
    // one parameter less than their arity,
    // so we increment it.
    if (ownerClass() && arity < 2)
        arity++;

    return arity;
}

bool AbstractMetaFunction::isInplaceOperator() const
{
    static const QSet<QStringView> inplaceOperators =
    {
        u"operator+=", u"operator&=", u"operator-=", u"operator|=",
        u"operator*=", u"operator^=", u"operator/=", u"operator<<=",
        u"operator%=", u"operator>>="
    };

    return isOperatorOverload() && inplaceOperators.contains(originalName());
}

bool AbstractMetaFunction::isVirtual() const
{
    return d->m_cppAttributes.testFlag(FunctionAttribute::Virtual);
}

QString AbstractMetaFunctionPrivate::modifiedName(const AbstractMetaFunction *q) const
{
    if (m_cachedModifiedName.isEmpty()) {
        for (const auto &mod : q->modifications(q->implementingClass())) {
            if (mod.isRenameModifier()) {
                m_cachedModifiedName = mod.renamedToName();
                break;
            }
        }
        if (m_cachedModifiedName.isEmpty())
            m_cachedModifiedName = m_name;
    }
    return m_cachedModifiedName;
}

QString AbstractMetaFunction::modifiedName() const
{
    return d->modifiedName(this);
}

AbstractMetaFunctionCPtr
AbstractMetaFunction::find(const AbstractMetaFunctionCList &haystack,
                           QAnyStringView needle)
{
    for (const auto &f : haystack) {
        if (f->name() == needle)
            return f;
    }
    return {};
}

bool AbstractMetaFunction::matches(OperatorQueryOptions query) const
{
    bool result = false;
    switch (d->m_functionType) {
    case AbstractMetaFunction::AssignmentOperatorFunction:
        result = query.testFlag(OperatorQueryOption::AssignmentOp);
        break;
    case AbstractMetaFunction::ConversionOperator:
        result = query.testFlag(OperatorQueryOption::ConversionOp);
        break;
    case AbstractMetaFunction::ArithmeticOperator:
        result = query.testFlag(OperatorQueryOption::ArithmeticOp);
        break;
    case AbstractMetaFunction::IncrementOperator:
    case AbstractMetaFunction::DecrementOperator:
        result = query.testFlag(OperatorQueryOption::IncDecrementOp);
        break;
    case AbstractMetaFunction::BitwiseOperator:
    case AbstractMetaFunction::ShiftOperator:
        result = query.testFlag(OperatorQueryOption::BitwiseOp);
        break;
    case AbstractMetaFunction::LogicalOperator:
        result = query.testFlag(OperatorQueryOption::LogicalOp);
        break;
    case AbstractMetaFunction::SubscriptOperator:
        result = query.testFlag(OperatorQueryOption::SubscriptionOp);
        break;
    case AbstractMetaFunction::ComparisonOperator:
        result = query.testFlag(OperatorQueryOption::ComparisonOp);
        if (!result && query.testFlag(OperatorQueryOption::SymmetricalComparisonOp))
            result = isSymmetricalComparisonOperator();
        break;
    default:
        break;
    }
    return result;
}

void AbstractMetaFunction::setAllowThreadModification(TypeSystem::AllowThread am)
{
    d->m_allowThreadModification = am;
}

void AbstractMetaFunction::setExceptionHandlingModification(TypeSystem::ExceptionHandling em)
{
    d->m_exceptionHandlingModification = em;
}

int AbstractMetaFunctionPrivate::overloadNumber(const AbstractMetaFunction *q) const
{
    if (m_cachedOverloadNumber == TypeSystem::OverloadNumberUnset) {
        m_cachedOverloadNumber = TypeSystem::OverloadNumberDefault;
        for (const auto &mod : q->modifications(q->implementingClass())) {
            if (mod.overloadNumber() != TypeSystem::OverloadNumberUnset) {
                m_cachedOverloadNumber = mod.overloadNumber();
                break;
            }
        }
    }
    return m_cachedOverloadNumber;
}

int AbstractMetaFunction::overloadNumber() const
{
    return d->overloadNumber(this);
}

TypeSystem::SnakeCase AbstractMetaFunction::snakeCase() const
{
    if (isUserAdded())
        return TypeSystem::SnakeCase::Disabled;
    // Renamed?
    if (!d->m_originalName.isEmpty() && d->m_originalName != d->m_name)
        return TypeSystem::SnakeCase::Disabled;
    switch (d->m_functionType) {
    case AbstractMetaFunction::NormalFunction:
    case AbstractMetaFunction::SignalFunction:
    case AbstractMetaFunction::EmptyFunction:
    case AbstractMetaFunction::SlotFunction:
        break;
    default:
        return TypeSystem::SnakeCase::Disabled;
    }

    for (const auto &mod : modifications()) {
        if (mod.snakeCase() != TypeSystem::SnakeCase::Unspecified)
            return mod.snakeCase();
    }

    if (d->m_typeEntry) // Global function
        return typeSystemTypeEntry(d->m_typeEntry)->snakeCase();

    if (d->m_class) {
        auto typeEntry = d->m_class->typeEntry();
        const auto snakeCase = typeEntry->snakeCase();
        return snakeCase != TypeSystem::SnakeCase::Unspecified
            ? snakeCase : typeSystemTypeEntry(typeEntry)->snakeCase();
    }
    return TypeSystem::SnakeCase::Disabled;
}

// Query functions for generators
bool AbstractMetaFunction::injectedCodeUsesPySelf() const
{
    return injectedCodeContains(u"%PYSELF", TypeSystem::CodeSnipPositionAny, TypeSystem::NativeCode);
}

bool AbstractMetaFunction::injectedCodeCallsPythonOverride() const
{
    static const QRegularExpression
        overrideCallRegexCheck(R"(PyObject_Call\s*\(\s*%PYTHON_METHOD_OVERRIDE\s*,)"_L1);
    Q_ASSERT(overrideCallRegexCheck.isValid());
    return injectedCodeContains(overrideCallRegexCheck, TypeSystem::CodeSnipPositionAny,
                                TypeSystem::NativeCode);
}

bool AbstractMetaFunction::injectedCodeHasReturnValueAttribution(TypeSystem::Language language) const
{
    if (language == TypeSystem::TargetLangCode) {
        static const QRegularExpression
            retValAttributionRegexCheck_target(R"(%PYARG_0\s*=[^=]\s*.+)"_L1);
        Q_ASSERT(retValAttributionRegexCheck_target.isValid());
        return injectedCodeContains(retValAttributionRegexCheck_target, TypeSystem::CodeSnipPositionAny, language);
    }

    static const QRegularExpression
        retValAttributionRegexCheck_native(R"(%0\s*=[^=]\s*.+)"_L1);
    Q_ASSERT(retValAttributionRegexCheck_native.isValid());
    return injectedCodeContains(retValAttributionRegexCheck_native, TypeSystem::CodeSnipPositionAny, language);
}

bool AbstractMetaFunction::injectedCodeUsesArgument(int argumentIndex) const
{
    const QRegularExpression argRegEx = CodeSnipAbstract::placeHolderRegex(argumentIndex + 1);

    return traverseCodeSnips([argRegEx](const CodeSnip &s) {
                                 const QString code = s.code();
                                 return code.contains(u"%ARGUMENT_NAMES") || code.contains(argRegEx);
                             }, TypeSystem::CodeSnipPositionAny);
}

static bool isModifiedToPrivate(const FunctionModification &mod)
{
    return mod.modifiers().testFlag(FunctionModification::Private);
}

bool AbstractMetaFunction::isVisibilityModifiedToPrivate() const
{
    const auto &mods = modifications();
    return std::any_of(mods.cbegin(), mods.cend(), isModifiedToPrivate);
}

struct ComparisonOperator
{
    const char *cppOperator;
    const char *pythonOpCode;
};

using ComparisonOperatorMapping =
    QHash<AbstractMetaFunction::ComparisonOperatorType, ComparisonOperator>;

static const ComparisonOperatorMapping &comparisonOperatorMapping()
{
    static const ComparisonOperatorMapping result = {
        {AbstractMetaFunction::OperatorEqual, {"==", "Py_EQ"}},
        {AbstractMetaFunction::OperatorNotEqual, {"!=", "Py_NE"}},
        {AbstractMetaFunction::OperatorLess, {"<", "Py_LT"}},
        {AbstractMetaFunction::OperatorLessEqual, {"<=", "Py_LE"}},
        {AbstractMetaFunction::OperatorGreater, {">", "Py_GT"}},
        {AbstractMetaFunction::OperatorGreaterEqual, {">=", "Py_GE"}}
    };
    return result;
}

const char * AbstractMetaFunction::pythonRichCompareOpCode(ComparisonOperatorType ct)
{
    return comparisonOperatorMapping().value(ct).pythonOpCode;
}

const char * AbstractMetaFunction::cppComparisonOperator(ComparisonOperatorType ct)
{
    return comparisonOperatorMapping().value(ct).cppOperator;
}

#ifndef QT_NO_DEBUG_STREAM
void AbstractMetaFunction::formatDebugBrief(QDebug &debug) const
{
    debug << '"' << debugSignature() << '"';
}

void AbstractMetaFunction::formatDebugVerbose(QDebug &debug) const
{
    debug << d->m_functionType << ' ';
    if (d->m_class)
        debug << d->m_access << ' ';
    debug << d->m_type << ' ' << d->m_name;
    switch (d->m_exceptionSpecification) {
    case ExceptionSpecification::Unknown:
        break;
    case ExceptionSpecification::NoExcept:
        debug << " noexcept";
        break;
    case ExceptionSpecification::Throws:
        debug << " throw(...)";
        break;
    }
    if (d->m_exceptionHandlingModification != TypeSystem::ExceptionHandling::Unspecified)
        debug << " exeption-mod " << int(d->m_exceptionHandlingModification);
    debug << '(';
    for (qsizetype i = 0, count = d->m_arguments.size(); i < count; ++i) {
        if (i)
            debug << ", ";
        debug <<  d->m_arguments.at(i);
    }
    const QString signature = minimalSignature();
    debug << "), signature=\"" << signature << '"';
    if (signature != d->m_unresolvedSignature)
        debug << ", unresolvedSignature=\"" << d->m_unresolvedSignature << '"';
    if (d->m_constant)
        debug << " [const]";
    if (d->m_reverse)
        debug << " [reverse]";
    if (isUserAdded())
        debug << " [userAdded]";
    if (isUserDeclared())
        debug << " [userDeclared]";
    if (d->m_cppAttributes.testFlag(FunctionAttribute::Explicit))
        debug << " [explicit]";
    if (d->m_cppAttributes.testFlag(FunctionAttribute::Deprecated))
        debug << " [deprecated]";
    if (d->m_pointerOperator)
        debug << " [operator->]";
    if (d->m_isCallOperator)
        debug << " [operator()]";
    if (d->m_class)
        debug << " class: " << d->m_class->name();
    if (d->m_implementingClass)
        debug << " implementing class: " << d->m_implementingClass->name();
    if (d->m_declaringClass)
        debug << " declaring class: " << d->m_declaringClass->name();
}

QDebug operator<<(QDebug debug, const AbstractMetaFunction *af)
{
    QDebugStateSaver saver(debug);
    debug.noquote();
    debug.nospace();
    debug << "AbstractMetaFunction(";
    if (af) {
        if (debug.verbosity() > 2) {
            af->formatDebugVerbose(debug);
        } else {
            debug << "signature=";
            af->formatDebugBrief(debug);
        }
    } else {
        debug << '0';
    }
    debug << ')';
    return debug;
}
#endif // !QT_NO_DEBUG_STREAM
