// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "apiextractor.h"
#include "apiextractorresult.h"
#include "abstractmetaargument.h"
#include "abstractmetabuilder.h"
#include "abstractmetaenum.h"
#include "abstractmetafield.h"
#include "abstractmetafunction.h"
#include "abstractmetalang.h"
#include "codesnip.h"
#include "exception.h"
#include "messages.h"
#include "modifications.h"
#include "optionsparser.h"
#include "reporthandler.h"
#include "typedatabase.h"
#include "customconversion.h"
#include "containertypeentry.h"
#include "primitivetypeentry.h"
#include "smartpointertypeentry.h"
#include "typedefentry.h"
#include "namespacetypeentry.h"
#include "typesystemtypeentry.h"

#include "qtcompat.h"

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryFile>

#include <algorithm>
#include <iostream>
#include <iterator>

using namespace Qt::StringLiterals;

struct InstantiationCollectContext
{
    AbstractMetaTypeList instantiatedContainers;
    InstantiatedSmartPointers instantiatedSmartPointers;
    QStringList instantiatedContainersNames;
};

struct ApiExtractorOptions
{
    QString m_typeSystemFileName;
    QFileInfoList m_cppFileNames;
    HeaderPaths m_includePaths;
    QStringList m_clangOptions;
    QString m_logDirectory;
    LanguageLevel m_languageLevel = LanguageLevel::Default;
    bool m_skipDeprecated = false;
};

static inline QString languageLevelDescription()
{
    return u"C++ Language level (c++11..c++17, default="_s
           + QLatin1StringView(clang::languageLevelOption(clang::emulatedCompilerLanguageLevel()))
           + u')';
}

QList<OptionDescription> ApiExtractor::options()
{
    return {
        {u"use-global-header"_s,
         u"Use the global headers in generated code."_s},
        {u"clang-option"_s,
         u"Option to be passed to clang"_s},
        {u"clang-options"_s,
         u"A comma-separated list of options to be passed to clang"_s},
        {u"skip-deprecated"_s,
         u"Skip deprecated functions"_s},
        {u"-F<path>"_s, {} },
        {u"framework-include-paths="_s + OptionsParser::pathSyntax(),
         u"Framework include paths used by the C++ parser"_s},
        {u"-isystem<path>"_s, {} },
        {u"system-include-paths="_s + OptionsParser::pathSyntax(),
         u"System include paths used by the C++ parser"_s},
        {u"language-level=, -std=<level>"_s,
         languageLevelDescription()},
    };
}

class ApiExtractorOptionsParser : public OptionsParser
{
public:
    explicit ApiExtractorOptionsParser(ApiExtractorOptions *o) : m_options(o) {}

    bool handleBoolOption(const QString &key, OptionSource source) override;
    bool handleOption(const QString &key, const QString &value,
                      OptionSource source) override;

private:
    void parseIncludePathOption(const QString &value, HeaderType headerType);
    void parseIncludePathOption(const QStringList &values, HeaderType headerType);
    void setLanguageLevel(const QString &value);

    ApiExtractorOptions *m_options;
};

void ApiExtractorOptionsParser::parseIncludePathOption(const QString &value,
                                                       HeaderType headerType)
{
    if (value.isEmpty())
        throw Exception(u"Empty value passed to include path option"_s);
    const auto path = QFile::encodeName(QDir::cleanPath(value));
    m_options->m_includePaths.append(HeaderPath{path, headerType});
}

void ApiExtractorOptionsParser::parseIncludePathOption(const QStringList &values,
                                                       HeaderType headerType)
{
    for (const auto &value : values)
        parseIncludePathOption(value, headerType);
}

void ApiExtractorOptionsParser::setLanguageLevel(const QString &value)
{
    const QByteArray languageLevelBA = value.toLatin1();
    const LanguageLevel level = clang::languageLevelFromOption(languageLevelBA.constData());
    if (level == LanguageLevel::Default)
        throw Exception(msgInvalidLanguageLevel(value));
    m_options->m_languageLevel = level;
}

bool ApiExtractorOptionsParser::handleBoolOption(const QString &key, OptionSource source)
{
    static const auto isystemOption = "isystem"_L1;

    switch (source) {
    case OptionSource::CommandLine:
    case OptionSource::ProjectFile:
        if (key == u"use-global-header") {
            AbstractMetaBuilder::setUseGlobalHeader(true);
            return true;
        }
        if (key == u"skip-deprecated") {
            m_options->m_skipDeprecated = true;
            return true;
        }
        break;

    case OptionSource::CommandLineSingleDash:
        if (key.startsWith(u'I')) { // Shorthand path arguments -I/usr/include...
            parseIncludePathOption(key.sliced(1), HeaderType::Standard);
            return true;
        }
        if (key.startsWith(u'F')) {
            parseIncludePathOption(key.sliced(1), HeaderType::Framework);
            return true;
        }
        if (key.startsWith(isystemOption)) {
            parseIncludePathOption(key.sliced(isystemOption.size()), HeaderType::System);
            return true;
        }
        break;
    }
    return false;
}

bool ApiExtractorOptionsParser::handleOption(const QString &key, const QString &value,
                                             OptionSource source)
{
    if (source == OptionSource::CommandLineSingleDash) {
        if (key == u"std") {
            setLanguageLevel(value);
            return true;
        }
        return false;
    }

    if (key == u"clang-option") {
        m_options->m_clangOptions.append(value);
        return true;
    }
    if (key == u"clang-options") {
        m_options->m_clangOptions.append(value.split(u',', Qt::SkipEmptyParts));
        return true;
    }
    if (key == u"include-paths") {
        parseIncludePathOption(value.split(QDir::listSeparator(), Qt::SkipEmptyParts),
                               HeaderType::Standard);
        return true;
    }
    if (key == u"framework-include-paths") {
        parseIncludePathOption(value.split(QDir::listSeparator(), Qt::SkipEmptyParts),
                               HeaderType::Framework);
        return true;
    }
    if (key == u"system-include-paths") {
        parseIncludePathOption(value.split(QDir::listSeparator(), Qt::SkipEmptyParts),
                               HeaderType::System);
        return true;
    }
    if (key == u"language-level") {
        setLanguageLevel(value);
        return true;
    }

    if (source == OptionSource::ProjectFile) {
        if (key == u"include-path") {
            parseIncludePathOption(value, HeaderType::Standard);
            return true;
        }
        if (key == u"framework-include-path") {
            parseIncludePathOption(value, HeaderType::Framework);
            return true;
        }
        if (key == u"system-include-path") {
            parseIncludePathOption(value, HeaderType::System);
            return true;
        }
    }

    return false;
}

std::shared_ptr<OptionsParser> ApiExtractor::createOptionsParser()
{
    return std::make_shared<ApiExtractorOptionsParser>(d);
}

struct ApiExtractorPrivate : public ApiExtractorOptions
{
    bool runHelper(ApiExtractorFlags flags);

    static QString getSimplifiedContainerTypeName(const AbstractMetaType &type);
    void addInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context,
                                                   const AbstractMetaType &type,
                                                   const QString &contextName);
    void collectInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context,
                                                       const AbstractMetaFunctionCPtr &func);
    void collectInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context,
                                                       const AbstractMetaClassCPtr &metaClass);
    void collectInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context);
    void collectInstantiatedOpqaqueContainers(InstantiationCollectContext &context);
    void collectContainerTypesFromSnippets(InstantiationCollectContext &context);
    void collectContainerTypesFromConverterMacros(InstantiationCollectContext &context,
                                                  const QString &code,
                                                  bool toPythonMacro);
    void addInstantiatedSmartPointer(InstantiationCollectContext &context,
                                     const AbstractMetaType &type) const;

    AbstractMetaBuilder *m_builder = nullptr;
};

ApiExtractor::ApiExtractor() :
      d(new ApiExtractorPrivate)
{
}

ApiExtractor::~ApiExtractor()
{
    delete d->m_builder;
    delete d;
}

HeaderPaths ApiExtractor::includePaths() const
{
    return d->m_includePaths;
}

void ApiExtractor::setLogDirectory(const QString& logDir)
{
    d->m_logDirectory = logDir;
}

void ApiExtractor::setCppFileNames(const QFileInfoList &cppFileName)
{
    d->m_cppFileNames = cppFileName;
}

QFileInfoList ApiExtractor::cppFileNames() const
{
    return d->m_cppFileNames;
}

void ApiExtractor::setTypeSystem(const QString& typeSystemFileName)
{
    d->m_typeSystemFileName = typeSystemFileName;
}

QString ApiExtractor::typeSystem() const
{
    return d->m_typeSystemFileName;
}

const AbstractMetaEnumList &ApiExtractor::globalEnums() const
{
    Q_ASSERT(d->m_builder);
    return d->m_builder->globalEnums();
}

const AbstractMetaFunctionCList &ApiExtractor::globalFunctions() const
{
    Q_ASSERT(d->m_builder);
    return d->m_builder->globalFunctions();
}

const AbstractMetaClassList &ApiExtractor::classes() const
{
    Q_ASSERT(d->m_builder);
    return d->m_builder->classes();
}

const AbstractMetaClassList &ApiExtractor::smartPointers() const
{
    Q_ASSERT(d->m_builder);
    return d->m_builder->smartPointers();
}

// Add defines required for parsing Qt code headers
static void addPySideExtensions(QByteArrayList *a)
{
    // Make "signals:", "slots:" visible as access specifiers
    a->append(QByteArrayLiteral("-DQT_ANNOTATE_ACCESS_SPECIFIER(a)=__attribute__((annotate(#a)))"));

    // Q_PROPERTY is defined as class annotation which does not work since a
    // sequence of properties will to expand to a sequence of annotations
    // annotating nothing, causing clang to complain. Instead, define it away in a
    // static assert with the stringified argument in a ','-operator (cf qdoc).
    a->append(QByteArrayLiteral("-DQT_ANNOTATE_CLASS(type,...)=static_assert(sizeof(#__VA_ARGS__),#type);"));

    // With Qt6, qsimd.h became public header and was included in <QtCore>. That
    // introduced a conflict with libclang headers on macOS. To be able to include
    // <QtCore>, we prevent its inclusion by adding its include guard.
    a->append(QByteArrayLiteral("-DQSIMD_H"));
}

bool ApiExtractorPrivate::runHelper(ApiExtractorFlags flags)
{
    if (m_builder)
        return false;

    if (!TypeDatabase::instance()->parseFile(m_typeSystemFileName)) {
        std::cerr << "Cannot parse file: " << qPrintable(m_typeSystemFileName);
        return false;
    }

    const QString pattern = QDir::tempPath() + u'/'
        + m_cppFileNames.constFirst().baseName() + "_XXXXXX.hpp"_L1;
    QTemporaryFile ppFile(pattern);
    bool autoRemove = !qEnvironmentVariableIsSet("KEEP_TEMP_FILES");
    // make sure that a tempfile can be written
    if (!ppFile.open()) {
        std::cerr << "could not create tempfile " << qPrintable(pattern)
            << ": " << qPrintable(ppFile.errorString()) << '\n';
        return false;
    }
    for (const auto &cppFileName : std::as_const(m_cppFileNames)) {
        ppFile.write("#include \"");
        ppFile.write(cppFileName.absoluteFilePath().toLocal8Bit());
        ppFile.write("\"\n");
    }
    const QString preprocessedCppFileName = ppFile.fileName();
    ppFile.close();
    m_builder = new AbstractMetaBuilder;
    m_builder->setLogDirectory(m_logDirectory);
    m_builder->setGlobalHeaders(m_cppFileNames);
    m_builder->setSkipDeprecated(m_skipDeprecated);
    m_builder->setHeaderPaths(m_includePaths);
    m_builder->setApiExtractorFlags(flags);

    QByteArrayList arguments;
    const auto clangOptionsSize = m_clangOptions.size();
    arguments.reserve(m_includePaths.size() + clangOptionsSize + 1);

    bool addCompilerSupportArguments = true;
    if (clangOptionsSize > 0) {
        qsizetype i = 0;
        if (m_clangOptions.at(i) == u"-") {
            ++i;
            addCompilerSupportArguments = false; // No built-in options
        }
        for (; i < clangOptionsSize; ++i)
            arguments.append(m_clangOptions.at(i).toUtf8());
    }

    for (const HeaderPath &headerPath : std::as_const(m_includePaths))
        arguments.append(HeaderPath::includeOption(headerPath));
    if (flags.testFlag(ApiExtractorFlag::UsePySideExtensions))
        addPySideExtensions(&arguments);
    arguments.append(QFile::encodeName(preprocessedCppFileName));

    if (ReportHandler::isDebug(ReportHandler::SparseDebug)) {
        qCInfo(lcShiboken).noquote().nospace()
            << "clang language level: " << int(m_languageLevel)
            << "\nclang arguments: " << arguments;
    }

    const bool result = m_builder->build(arguments, flags, addCompilerSupportArguments,
                                         m_languageLevel);
    if (!result)
        autoRemove = false;
    if (!autoRemove) {
        ppFile.setAutoRemove(false);
        std::cerr << "Keeping temporary file: " << qPrintable(QDir::toNativeSeparators(preprocessedCppFileName)) << '\n';
    }
    return result;
}

static inline void classListToCList(const AbstractMetaClassList &list, AbstractMetaClassCList *target)
{
    target->reserve(list.size());
    std::copy(list.cbegin(), list.cend(), std::back_inserter(*target));
}

std::optional<ApiExtractorResult> ApiExtractor::run(ApiExtractorFlags flags)
{
    if (!d->runHelper(flags))
        return {};
    InstantiationCollectContext collectContext;
    d->collectInstantiatedContainersAndSmartPointers(collectContext);

    ApiExtractorResult result;
    classListToCList(d->m_builder->takeClasses(), &result.m_metaClasses);
    classListToCList(d->m_builder->takeSmartPointers(), &result.m_smartPointers);
    result.m_globalFunctions = d->m_builder->globalFunctions();
    result.m_globalEnums = d->m_builder->globalEnums();
    result.m_enums = d->m_builder->typeEntryToEnumsHash();
    result.m_flags = flags;
    result.m_typedefTargetToName = d->m_builder->typedefTargetToName();
    qSwap(result.m_instantiatedContainers, collectContext.instantiatedContainers);
    qSwap(result.m_instantiatedSmartPointers, collectContext.instantiatedSmartPointers);
    return result;
}

LanguageLevel ApiExtractor::languageLevel() const
{
    return d->m_languageLevel;
}

QStringList ApiExtractor::clangOptions() const
{
    return d->m_clangOptions;
}

AbstractMetaFunctionPtr
    ApiExtractor::inheritTemplateFunction(const AbstractMetaFunctionCPtr &function,
                                          const AbstractMetaTypeList &templateTypes)
{
    return AbstractMetaBuilder::inheritTemplateFunction(function, templateTypes);
}

AbstractMetaFunctionPtr
    ApiExtractor::inheritTemplateMember(const AbstractMetaFunctionCPtr &function,
                                        const AbstractMetaTypeList &templateTypes,
                                        const AbstractMetaClassCPtr &templateClass,
                                        const AbstractMetaClassPtr &subclass)
{
    return AbstractMetaBuilder::inheritTemplateMember(function, templateTypes,
                                                      templateClass, subclass);
}

AbstractMetaClassPtr ApiExtractor::inheritTemplateClass(const ComplexTypeEntryPtr &te,
                                                      const AbstractMetaClassCPtr &templateClass,
                                                      const AbstractMetaTypeList &templateTypes)
{
    return AbstractMetaBuilder::inheritTemplateClass(te, templateClass,
                                                     templateTypes);
}

QString ApiExtractorPrivate::getSimplifiedContainerTypeName(const AbstractMetaType &type)
{
    QString signature = type.cppSignature();
    if (!type.typeEntry()->isContainer() && !type.typeEntry()->isSmartPointer())
        return signature;
    QString typeName = signature;
    if (type.isConstant())
        typeName.remove(0, sizeof("const ") / sizeof(char) - 1);
    switch (type.referenceType()) {
    case NoReference:
        break;
    case LValueReference:
        typeName.chop(1);
        break;
    case RValueReference:
        typeName.chop(2);
        break;
    }
    while (typeName.endsWith(u'*') || typeName.endsWith(u' '))
        typeName.chop(1);
    return typeName;
}

// Strip a "const QSharedPtr<const Foo> &" or similar to "QSharedPtr<Foo>" (PYSIDE-1016/454)
AbstractMetaType canonicalSmartPtrInstantiation(const AbstractMetaType &type)
{
    const AbstractMetaTypeList &instantiations = type.instantiations();
    Q_ASSERT(instantiations.size() == 1);
    const bool needsFix = type.isConstant() || type.referenceType() != NoReference;
    const bool pointeeNeedsFix = instantiations.constFirst().isConstant();
    if (!needsFix && !pointeeNeedsFix)
        return type;
    auto fixedType = type;
    fixedType.setReferenceType(NoReference);
    fixedType.setConstant(false);
    if (pointeeNeedsFix) {
        auto fixedPointeeType = instantiations.constFirst();
        fixedPointeeType.setConstant(false);
        fixedType.setInstantiations(AbstractMetaTypeList(1, fixedPointeeType));
    }
    return fixedType;
}

static inline TypeEntryCPtr pointeeTypeEntry(const AbstractMetaType &smartPtrType)
{
    return smartPtrType.instantiations().constFirst().typeEntry();
}

static AbstractMetaType simplifiedType(AbstractMetaType type)
{
    type.setIndirections(0);
    type.setConstant(false);
    type.setReferenceType(NoReference);
    type.decideUsagePattern();
    return type;
}

void
ApiExtractorPrivate::addInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context,
                                                               const AbstractMetaType &type,
                                                               const QString &contextName)
{
    for (const auto &t : type.instantiations())
        addInstantiatedContainersAndSmartPointers(context, t, contextName);
    const auto typeEntry = type.typeEntry();
    const bool isContainer = typeEntry->isContainer();
    if (!isContainer
        && !(typeEntry->isSmartPointer() && typeEntry->generateCode())) {
        return;
    }
    if (type.hasTemplateChildren()) {
        const auto piece = isContainer ? "container"_L1 : "smart pointer"_L1;
        QString warning =
            QString::fromLatin1("Skipping instantiation of %1 '%2' because it has template"
                                " arguments.").arg(piece, type.originalTypeDescription());
        if (!contextName.isEmpty())
            warning.append(" Calling context: "_L1 + contextName);

        qCWarning(lcShiboken).noquote().nospace() << warning;
        return;

    }
    if (isContainer) {
        const QString typeName = getSimplifiedContainerTypeName(type);
        if (!context.instantiatedContainersNames.contains(typeName)) {
            context.instantiatedContainersNames.append(typeName);
            context.instantiatedContainers.append(simplifiedType(type));
        }
        return;
    }

    // Is smart pointer. Check if the (const?) pointee is already known for the given
    // smart pointer type entry.
    auto pt = pointeeTypeEntry(type);
    const bool present =
        std::any_of(context.instantiatedSmartPointers.cbegin(),
                    context.instantiatedSmartPointers.cend(),
                    [typeEntry, pt] (const InstantiatedSmartPointer &smp) {
                        return smp.type.typeEntry() == typeEntry
                            && pointeeTypeEntry(smp.type) == pt;
                    });
    if (!present)
        addInstantiatedSmartPointer(context, type);
}

// Create a modification that invalidates the pointee argument of a smart
// pointer constructor or reset().
static FunctionModification invalidateArgMod(const AbstractMetaFunctionCPtr &f, int index = 1)
{
    ArgumentModification argMod;
    argMod.setTargetOwnerShip(TypeSystem::CppOwnership);
    argMod.setIndex(index);
    FunctionModification funcMod;
    funcMod.setSignature(f->minimalSignature());
    funcMod.setArgument_mods({argMod});
    return funcMod;
}

static void addOwnerModification(const AbstractMetaFunctionCList &functions,
                                 const ComplexTypeEntryPtr &typeEntry)
{
    for (const auto &f : functions) {
        if (!f->arguments().isEmpty()
            && f->arguments().constFirst().type().indirections() > 0) {
            std::const_pointer_cast<AbstractMetaFunction>(f)->clearModificationsCache();
            typeEntry->addFunctionModification(invalidateArgMod(f));
        }
    }
}

void ApiExtractorPrivate::addInstantiatedSmartPointer(InstantiationCollectContext &context,
                                                      const AbstractMetaType &type) const
{
    InstantiatedSmartPointer smp;
    smp.type =  canonicalSmartPtrInstantiation(type);
    smp.smartPointer = AbstractMetaClass::findClass(m_builder->smartPointers(),
                                                    type.typeEntry());
    Q_ASSERT(smp.smartPointer);

    const auto &instantiatedType = type.instantiations().constFirst();
    const auto ste = std::static_pointer_cast<const SmartPointerTypeEntry>(smp.smartPointer->typeEntry());
    QString name = ste->getTargetName(smp.type);
    auto parentTypeEntry = ste->parent();

    // FIXME PYSIDE 7: Make global scope the default behavior?
    // Note: Also requires changing SmartPointerTypeEntry::getTargetName()
    // to not strip namespaces from unnamed instances.
    const bool globalScope = name.startsWith("::"_L1);
    if (globalScope) {
        name.remove(0, 2);
        parentTypeEntry = typeSystemTypeEntry(ste);
    }

    auto colonPos = name.lastIndexOf(u"::");
    const bool withinNameSpace = colonPos != -1;
    if (withinNameSpace) { // user defined
        const QString nameSpace = name.left(colonPos);
        name.remove(0, colonPos + 2);
        const auto nameSpaces = TypeDatabase::instance()->findNamespaceTypes(nameSpace);
        if (nameSpaces.isEmpty())
            throw Exception(msgNamespaceNotFound(name));
        parentTypeEntry = nameSpaces.constFirst();
    }

    TypedefEntryPtr typedefEntry(new TypedefEntry(name, ste->name(), ste->version(),
                                                  parentTypeEntry));
    typedefEntry->setTargetLangPackage(ste->targetLangPackage());
    auto instantiationEntry = TypeDatabase::initializeTypeDefEntry(typedefEntry, ste);
    instantiationEntry->setParent(parentTypeEntry);

    smp.specialized = ApiExtractor::inheritTemplateClass(instantiationEntry, smp.smartPointer,
                                                         {instantiatedType});
    Q_ASSERT(smp.specialized);
    if (parentTypeEntry->type() != TypeEntry::TypeSystemType) { // move class to desired namespace
        const auto enclClass = AbstractMetaClass::findClass(m_builder->classes(), parentTypeEntry);
        Q_ASSERT(enclClass);
        auto specialized = std::const_pointer_cast<AbstractMetaClass>(smp.specialized);
        specialized->setEnclosingClass(enclClass);
        enclClass->addInnerClass(specialized);
    }

    if (instantiationEntry->isComplex()) {
        addOwnerModification(smp.specialized->queryFunctions(FunctionQueryOption::Constructors),
                             instantiationEntry);
        if (!ste->resetMethod().isEmpty()) {
            addOwnerModification(smp.specialized->findFunctions(ste->resetMethod()),
                                 instantiationEntry);
        }
    }

    context.instantiatedSmartPointers.append(smp);
}

void
ApiExtractorPrivate::collectInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context,
                                                                   const AbstractMetaFunctionCPtr &func)
{
    addInstantiatedContainersAndSmartPointers(context, func->type(), func->signature());
    for (const AbstractMetaArgument &arg : func->arguments()) {
        const auto &argType = arg.type();
        const auto type = argType.viewOn() != nullptr ? *argType.viewOn() : argType;
        addInstantiatedContainersAndSmartPointers(context, type, func->signature());
    }
}

void
ApiExtractorPrivate::collectInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context,
                                                                   const AbstractMetaClassCPtr &metaClass)
{
    if (!metaClass->typeEntry()->generateCode())
        return;
    for (const auto &func : metaClass->functions())
        collectInstantiatedContainersAndSmartPointers(context, func);
    for (const auto &func : metaClass->userAddedPythonOverrides())
        collectInstantiatedContainersAndSmartPointers(context, func);
    for (const AbstractMetaField &field : metaClass->fields())
        addInstantiatedContainersAndSmartPointers(context, field.type(), field.name());

    // The list of inner classes might be extended when smart pointer
    // instantiations are specified to be in namespaces.
    const auto &innerClasses = metaClass->innerClasses();
    for (auto i = innerClasses.size() - 1; i >= 0; --i) {
         const auto innerClass = innerClasses.at(i);
         if (!innerClass->typeEntry()->isSmartPointer())
             collectInstantiatedContainersAndSmartPointers(context, innerClass);
    }
}

void
ApiExtractorPrivate::collectInstantiatedContainersAndSmartPointers(InstantiationCollectContext &context)
{
    collectInstantiatedOpqaqueContainers(context);
    for (const auto &func : m_builder->globalFunctions())
        collectInstantiatedContainersAndSmartPointers(context, func);
    for (const auto &metaClass : m_builder->classes())
        collectInstantiatedContainersAndSmartPointers(context, metaClass);
    collectContainerTypesFromSnippets(context);
}

// Whether to generate an opaque container: If the instantiation type is in
// the current package or, for primitive types, if the container is in the
// current package.
static bool generateOpaqueContainer(const AbstractMetaType &type,
                                    const TypeSystemTypeEntryCPtr &moduleEntry)
{
    auto te = type.instantiations().constFirst().typeEntry();
    auto typeModuleEntry = typeSystemTypeEntry(te);
    return typeModuleEntry == moduleEntry
           || (te->isPrimitive() && typeSystemTypeEntry(type.typeEntry()) == moduleEntry);
}

void ApiExtractorPrivate::collectInstantiatedOpqaqueContainers(InstantiationCollectContext &context)
{
    // Add all instantiations of opaque containers for types from the current
    // module.
    auto *td = TypeDatabase::instance();
    const auto moduleEntry = TypeDatabase::instance()->defaultTypeSystemType();
    const auto &containers = td->containerTypes();
    for (const auto &container : containers) {
        for (const auto &oc : container->opaqueContainers()) {
            QString errorMessage;
            const QString typeName = container->qualifiedCppName() + oc.templateParameters();
            auto typeOpt = AbstractMetaType::fromString(typeName, &errorMessage);
            if (typeOpt.has_value()
                && generateOpaqueContainer(typeOpt.value(), moduleEntry)) {
                addInstantiatedContainersAndSmartPointers(context, typeOpt.value(),
                                                          u"opaque containers"_s);
            }
        }
    }
}

static void getCode(QStringList &code, const CodeSnipList &codeSnips)
{
    for (const CodeSnip &snip : std::as_const(codeSnips))
        code.append(snip.code());
}

static void getCode(QStringList &code, const TypeEntryCPtr &type)
{
    if (type->isComplex())
        getCode(code, std::static_pointer_cast<const ComplexTypeEntry>(type)->codeSnips());
    else if (type->isTypeSystem())
        getCode(code, std::static_pointer_cast<const TypeSystemTypeEntry>(type)->codeSnips());

    auto customConversion = CustomConversion::getCustomConversion(type);
    if (!customConversion)
        return;

    if (!customConversion->nativeToTargetConversion().isEmpty())
        code.append(customConversion->nativeToTargetConversion());

    const auto &toCppConversions = customConversion->targetToNativeConversions();
    if (toCppConversions.isEmpty())
        return;

    for (const auto &toNative : std::as_const(toCppConversions))
        code.append(toNative.conversion());
}

void ApiExtractorPrivate::collectContainerTypesFromSnippets(InstantiationCollectContext &context)
{
    QStringList snips;
    auto *td = TypeDatabase::instance();
    const PrimitiveTypeEntryCList &primitiveTypeList = td->primitiveTypes();
    for (const auto &type : primitiveTypeList)
        getCode(snips, type);
    const ContainerTypeEntryCList &containerTypeList = td->containerTypes();
    for (const auto &type : containerTypeList)
        getCode(snips, type);
    for (const auto &metaClass : m_builder->classes())
        getCode(snips, metaClass->typeEntry());

    const auto moduleEntry = td->defaultTypeSystemType();
    Q_ASSERT(moduleEntry);
    getCode(snips, moduleEntry);

    for (const auto &func : m_builder->globalFunctions())
        getCode(snips, func->injectedCodeSnips());

    for (const QString &code : std::as_const(snips)) {
        collectContainerTypesFromConverterMacros(context, code, true);
        collectContainerTypesFromConverterMacros(context, code, false);
    }
}

void
ApiExtractorPrivate::collectContainerTypesFromConverterMacros(InstantiationCollectContext &context,
                                                              const QString &code,
                                                              bool toPythonMacro)
{
    QString convMacro = toPythonMacro ? u"%CONVERTTOPYTHON["_s : u"%CONVERTTOCPP["_s;
    const qsizetype offset = toPythonMacro ? sizeof("%CONVERTTOPYTHON") : sizeof("%CONVERTTOCPP");
    qsizetype start = 0;
    QString errorMessage;
    while ((start = code.indexOf(convMacro, start)) != -1) {
        const auto end = code.indexOf(u']', start);
        start += offset;
        if (code.at(start) != u'%') {
            QString typeString = code.mid(start, end - start);
            auto type = AbstractMetaType::fromString(typeString, &errorMessage);
            if (type.has_value()) {
                const QString &d = type->originalTypeDescription();
                addInstantiatedContainersAndSmartPointers(context, type.value(), d);
            } else {
                QString m;
                QTextStream(&m) << __FUNCTION__ << ": Cannot translate type \""
                                << typeString << "\": " << errorMessage;
                throw Exception(m);
            }
        }
        start = end;
    }
}

#ifndef QT_NO_DEBUG_STREAM
template <class Container>
static void debugFormatSequence(QDebug &d, const char *key, const Container& c)
{
    if (c.isEmpty())
        return;
    const auto begin = c.begin();
    d << "\n  " << key << '[' << c.size() << "]=(";
    for (auto it = begin, end = c.end(); it != end; ++it) {
        if (it != begin)
            d << ", ";
        d << *it;
    }
    d << ')';
}

QDebug operator<<(QDebug d, const ApiExtractor &ae)
{
    QDebugStateSaver saver(d);
    d.noquote();
    d.nospace();
    if (ReportHandler::debugLevel() >= ReportHandler::FullDebug)
        d.setVerbosity(3); // Trigger verbose output of AbstractMetaClass
    d << "ApiExtractor(typeSystem=\"" << ae.typeSystem() << "\", cppFileNames=\""
      << ae.cppFileNames() << ", ";
    ae.d->m_builder->formatDebug(d);
    d << ')';
    return d;
}
#endif // QT_NO_DEBUG_STREAM
