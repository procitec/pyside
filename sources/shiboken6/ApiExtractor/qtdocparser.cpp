// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtdocparser.h"
#include "classdocumentation.h"
#include "abstractmetaargument.h"
#include "abstractmetaenum.h"
#include "abstractmetafunction.h"
#include "abstractmetalang.h"
#include "abstractmetatype.h"
#include "documentation.h"
#include "exception.h"
#include "modifications.h"
#include "messages.h"
#include "propertyspec.h"
#include "reporthandler.h"
#include "flagstypeentry.h"
#include "complextypeentry.h"
#include "functiontypeentry.h"
#include "enumtypeentry.h"
#include "typesystemtypeentry.h"
#include "typedatabase.h"

#include "qtcompat.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QUrl>

using namespace Qt::StringLiterals;

enum { debugFunctionSearch = 0 };

constexpr auto briefStartElement = "<brief>"_L1;
constexpr auto briefEndElement = "</brief>"_L1;
constexpr auto webxmlSuffix = ".webxml"_L1;

Documentation QtDocParser::retrieveModuleDocumentation()
{
    return retrieveModuleDocumentation(packageName());
}

// Return the package of a type "PySide6.QtGui.QPainter" -> "PySide6.QtGui"
static QStringView packageFromPythonType(QStringView pythonType)
{
    qsizetype pos = pythonType.startsWith(u"PySide6.") ? 8 : 0;
    auto dot = pythonType.indexOf(u'.', pos);
    return dot != -1 ? pythonType.sliced(0, dot) : pythonType;
}

// Return the qdoc dir "PySide6.QtGui.QPainter" -> "qtgui/webxml" (QTBUG-119500)
static QString qdocModuleDirFromPackage(QStringView package)
{
    if (package.startsWith(u"PySide6."))
        package = package.sliced(8);
    return package.toString().toLower() + "/webxml"_L1;
}

// Populate a cache of package to WebXML dir
static QHash<QString, QString> getPackageToModuleDir()
{
    QHash<QString, QString> result;
    const auto &typeSystemEntries = TypeDatabase::instance()->typeSystemEntries();
    for (const auto &te : typeSystemEntries) {
        const QString &package = te->name();
        const QString &docPackage = te->hasDocTargetLangPackage()
                                    ? te->docTargetLangPackage() : package;
        result.insert(package, qdocModuleDirFromPackage(docPackage));
    }
    return result;
}

QString QtDocParser::qdocModuleDir(const QString &pythonType)
{
    static const QHash<QString, QString> packageToModuleDir = getPackageToModuleDir();

    const QStringView package = packageFromPythonType(pythonType);
    const auto it = packageToModuleDir.constFind(package);
    if (it == packageToModuleDir.cend()) {
        const QString known = packageToModuleDir.keys().join(", "_L1);
        qCWarning(lcShibokenDoc, "Type from unknown package: \"%s\" (known: %s).",
                  qPrintable(pythonType), qPrintable(known));
        return qdocModuleDirFromPackage(package);
    }
    return it.value();
}

static QString xmlFileNameRoot(const AbstractMetaClassPtr &metaClass)
{
    QString className = metaClass->qualifiedCppName().toLower();
    className.replace("::"_L1, "-"_L1);

    return QtDocParser::qdocModuleDir(metaClass->typeEntry()->targetLangPackage())
           + u'/' + className;
}

static void formatPreQualifications(QTextStream &str, const AbstractMetaType &type)
{
    if (type.isConstant())
        str << "const " ;
}

static void formatPostQualifications(QTextStream &str, const AbstractMetaType &type)
{
    if (type.referenceType() == LValueReference)
        str << " &";
    else if (type.referenceType() == RValueReference)
        str << " &&";
    else if (type.indirections())
        str << ' ' << QByteArray(type.indirections(), '*');
}

static void formatFunctionUnqualifiedArgTypeQuery(QTextStream &str,
                                                  const AbstractMetaType &metaType)
{
    switch (metaType.typeUsagePattern()) {
    case AbstractMetaType::FlagsPattern: {
        // Modify qualified name "QFlags<Qt::AlignmentFlag>" with name "Alignment"
        // to "Qt::Alignment" as seen by qdoc.
        const auto flagsEntry = std::static_pointer_cast<const FlagsTypeEntry>(metaType.typeEntry());
        QString name = flagsEntry->qualifiedCppName();
        if (name.endsWith(u'>') && name.startsWith(u"QFlags<")) {
            const auto lastColon = name.lastIndexOf(u':');
            if (lastColon != -1) {
                name.replace(lastColon + 1, name.size() - lastColon - 1, metaType.name());
                name.remove(0, 7);
            } else {
                name = metaType.name(); // QFlags<> of enum in global namespace
            }
        }
        str << name;
    }
        break;
    case AbstractMetaType::ContainerPattern: { // QVector<int>
        str << metaType.typeEntry()->qualifiedCppName() << '<';
        const auto &instantiations = metaType.instantiations();
        for (qsizetype i = 0, size = instantiations.size(); i < size; ++i) {
            if (i)
                str << ", ";
            const auto &instantiation = instantiations.at(i);
            formatPreQualifications(str, instantiation);
            str << instantiation.typeEntry()->qualifiedCppName();
            formatPostQualifications(str, instantiation);
        }
        str << '>';
    }
        break;
    default: // Fully qualify enums (Qt::AlignmentFlag), nested classes, etc.
        str << metaType.typeEntry()->qualifiedCppName();
        break;
    }
}

static QString formatFunctionArgTypeQuery(const AbstractMetaType &metaType)
{
    QString result;
    QTextStream str(&result);formatPreQualifications(str, metaType);
    formatFunctionUnqualifiedArgTypeQuery(str, metaType);
    formatPostQualifications(str, metaType);
    return result;
}

QString QtDocParser::functionDocumentation(const QString &sourceFileName,
                                           const ClassDocumentation &classDocumentation,
                                           const AbstractMetaClassCPtr &metaClass,
                                           const AbstractMetaFunctionCPtr &func,
                                           QString *errorMessage)
{
    errorMessage->clear();

    const QString docString =
        queryFunctionDocumentation(sourceFileName, classDocumentation, metaClass,
                                   func, errorMessage);

    const auto funcModifs = DocParser::getXpathDocModifications(func, metaClass);
    return docString.isEmpty() || funcModifs.isEmpty()
        ? docString : applyDocModifications(funcModifs, docString);
}

QString QtDocParser::queryFunctionDocumentation(const QString &sourceFileName,
                                                const ClassDocumentation &classDocumentation,
                                                const AbstractMetaClassCPtr &metaClass,
                                                const AbstractMetaFunctionCPtr &func,
                                                QString *errorMessage)
{
    // Search candidates by name and const-ness
    FunctionDocumentationList candidates =
        classDocumentation.findFunctionCandidates(func->name(), func->isConstant());
    if (candidates.isEmpty()) {
        *errorMessage = msgCannotFindDocumentation(sourceFileName, func.get())
                        + u" (no matches)"_s;
        return {};
    }

    // Try an exact query
    FunctionDocumentationQuery fq;
    fq.name = func->name();
    fq.constant = func->isConstant();
    for (const auto &arg : func->arguments())
        fq.parameters.append(formatFunctionArgTypeQuery(arg.type()));

    const auto funcFlags = func->flags();
    // Re-add arguments removed by the metabuilder to binary operator functions
    if (funcFlags.testFlag(AbstractMetaFunction::Flag::OperatorLeadingClassArgumentRemoved)
        || funcFlags.testFlag(AbstractMetaFunction::Flag::OperatorTrailingClassArgumentRemoved)) {
        QString classType = metaClass->qualifiedCppName();
        if (!funcFlags.testFlag(AbstractMetaFunction::Flag::OperatorClassArgumentByValue)) {
            classType.prepend(u"const "_s);
            classType.append(u" &"_s);
        }
        if (funcFlags.testFlag(AbstractMetaFunction::Flag::OperatorLeadingClassArgumentRemoved))
            fq.parameters.prepend(classType);
        else
            fq.parameters.append(classType);
    }

    const qsizetype index = ClassDocumentation::indexOfFunction(candidates, fq);

    if (debugFunctionSearch) {
        qDebug() << __FUNCTION__ << metaClass->name() << fq << funcFlags << "returns"
            << index << "\n  " << candidates.value(index) << "\n  " << candidates;
    }

    if (index != -1)
        return candidates.at(index).description;

    // Fallback: Try matching by argument count
    const auto parameterCount = func->arguments().size();
    auto pend = std::remove_if(candidates.begin(), candidates.end(),
                               [parameterCount](const FunctionDocumentation &fd) {
                                   return fd.parameters.size() != parameterCount; });
    candidates.erase(pend, candidates.end());
    if (candidates.size() == 1) {
        const auto &match = candidates.constFirst();
        QTextStream(errorMessage) << msgFallbackForDocumentation(sourceFileName, func.get())
            << "\n  Falling back to \"" << match.signature
            << "\" obtained by matching the argument count only.";
        return candidates.constFirst().description;
    }

    QTextStream(errorMessage) << msgCannotFindDocumentation(sourceFileName, func.get())
        << " (" << candidates.size() << " candidates matching the argument count)";
    return {};
}

// Extract the <brief> section from a WebXML (class) documentation and remove it
// from the source.
static QString extractBrief(QString *value)
{
    const auto briefStart = value->indexOf(briefStartElement);
    if (briefStart < 0)
        return {};
    const auto briefEnd = value->indexOf(briefEndElement,
                                         briefStart + briefStartElement.size());
    if (briefEnd < briefStart)
        return {};
    const auto briefLength = briefEnd + briefEndElement.size() - briefStart;
    QString briefValue = value->mid(briefStart, briefLength);
    briefValue.insert(briefValue.size() - briefEndElement.size(),
                      u"<rst> More_...</rst>"_s);
    value->remove(briefStart, briefLength);
    return briefValue;
}

// Find the webxml file for global functions/enums
// by the doc-file typesystem attribute or via include file.
static QString findGlobalWebXmLFile(const QString &documentationDataDirectory,
                                    const QString &package,
                                    const QString &docFile,
                                    const Include &include)
{
    QString result;
    const QString root = documentationDataDirectory + u'/'
                         + QtDocParser::qdocModuleDir(package) + u'/';
    if (!docFile.isEmpty()) {
        result = root + docFile;
        if (!result.endsWith(webxmlSuffix))
            result += webxmlSuffix;
        return QFileInfo::exists(result) ? result : QString{};
    }
    if (include.name().isEmpty())
        return {};
    // qdoc "\headerfile <QtLogging>" directive produces "qtlogging.webxml"
    result = root + QFileInfo(include.name()).baseName() + webxmlSuffix;
    if (QFileInfo::exists(result))
        return result;
    // qdoc "\headerfile <qdrawutil.h>" produces "qdrawutil-h.webxml"
    result.insert(result.size() - webxmlSuffix.size(), "-h"_L1);
    return QFileInfo::exists(result) ? result : QString{};
}

void  QtDocParser::fillGlobalFunctionDocumentation(const AbstractMetaFunctionPtr &f)
{
    auto te = f->typeEntry();
    if (te == nullptr)
        return;

    const QString sourceFileName =
        findGlobalWebXmLFile(documentationDataDirectory(), te->targetLangPackage(), te->docFile(), te->include());
    if (sourceFileName.isEmpty())
        return;

    QString errorMessage;
    auto classDocumentationO = parseWebXml(sourceFileName, &errorMessage);
    if (!classDocumentationO.has_value()) {
        qCWarning(lcShibokenDoc, "%s", qPrintable(errorMessage));
        return;
    }
    const QString detailed =
        functionDocumentation(sourceFileName, classDocumentationO.value(),
                              {}, f, &errorMessage);
    if (!errorMessage.isEmpty())
        qCWarning(lcShibokenDoc, "%s", qPrintable(errorMessage));
    Documentation documentation(detailed, {}, sourceFileName);
    f->setDocumentation(documentation);
}

void QtDocParser::fillGlobalEnumDocumentation(AbstractMetaEnum &e)
{
    auto te = e.typeEntry();
    const QString sourceFileName =
        findGlobalWebXmLFile(documentationDataDirectory(), te->targetLangPackage(), te->docFile(), te->include());
    if (sourceFileName.isEmpty())
        return;

    QString errorMessage;
    auto classDocumentationO = parseWebXml(sourceFileName, &errorMessage);
    if (!classDocumentationO.has_value()) {
        qCWarning(lcShibokenDoc, "%s", qPrintable(errorMessage));
        return;
    }
    if (!extractEnumDocumentation(classDocumentationO.value(), sourceFileName, e)) {
        qCWarning(lcShibokenDoc, "%s",
                  qPrintable(msgCannotFindDocumentation(sourceFileName, {}, e, {})));
    }
}

QString QtDocParser::fillDocumentation(const AbstractMetaClassPtr &metaClass)
{
    if (!metaClass)
        return {};

    auto context = metaClass->enclosingClass();
    while (context) {
        if (!context->enclosingClass())
            break;
        context = context->enclosingClass();
    }

    QString sourceFileRoot = documentationDataDirectory() + u'/' + xmlFileNameRoot(metaClass);

    QFileInfo sourceFile(sourceFileRoot + webxmlSuffix);
    if (!sourceFile.exists())
        sourceFile.setFile(sourceFileRoot + ".xml"_L1);
   if (!sourceFile.exists()) {
        qCWarning(lcShibokenDoc).noquote().nospace()
            << "Can't find qdoc file for class " << metaClass->name() << ", tried: "
            << QDir::toNativeSeparators(sourceFile.absoluteFilePath());
       return {};
    }

    const QString sourceFileName = sourceFile.absoluteFilePath();
    QString errorMessage;

    const auto classDocumentationO = parseWebXml(sourceFileName, &errorMessage);
    if (!classDocumentationO.has_value()) {
        qCWarning(lcShibokenDoc, "%s", qPrintable(errorMessage));
        return {};
    }

    const auto &classDocumentation = classDocumentationO.value();
    for (const auto &p : classDocumentation.properties) {
        Documentation doc(p.description, p.brief, sourceFileName);
        metaClass->setPropertyDocumentation(p.name, doc);
    }

    QString docString = applyDocModifications(DocParser::getXpathDocModifications(metaClass),
                                              classDocumentation.description);

    if (docString.isEmpty()) {
        QString className = metaClass->name();
        qCWarning(lcShibokenDoc, "%s",
                  qPrintable(msgCannotFindDocumentation(sourceFileName, "class", className, {})));
    }
    const QString brief = extractBrief(&docString);

    Documentation doc;
    doc.setSourceFile(sourceFileName);
    if (!brief.isEmpty())
        doc.setValue(brief, Documentation::Brief);
    doc.setValue(docString);
    metaClass->setDocumentation(doc);

    //Functions Documentation
    const auto &funcs = DocParser::documentableFunctions(metaClass);
    for (const auto &func : funcs) {
        const QString detailed =
            functionDocumentation(sourceFileName, classDocumentation,
                                  metaClass, func, &errorMessage);
        if (!errorMessage.isEmpty())
            qCWarning(lcShibokenDoc, "%s", qPrintable(errorMessage));
        const Documentation documentation(detailed, {}, sourceFileName);
        std::const_pointer_cast<AbstractMetaFunction>(func)->setDocumentation(documentation);
    }
#if 0
    // Fields
    const AbstractMetaFieldList &fields = metaClass->fields();
    for (AbstractMetaField *field : fields) {
        if (field->isPrivate())
            return;

        QString query = "/doxygen/compounddef/sectiondef/memberdef/name[text()=\"" + field->name() + "\"]/..";
        Documentation doc = getDocumentation(DocModificationList(), xquery, query);
        field->setDocumentation(doc);
    }
#endif
    // Enums
    for (AbstractMetaEnum &meta_enum : metaClass->enums()) {
        if (!extractEnumDocumentation(classDocumentation, sourceFileName, meta_enum)) {
            qCWarning(lcShibokenDoc, "%s",
                      qPrintable(msgCannotFindDocumentation(sourceFileName, metaClass, meta_enum, {})));
        }
    }

    return sourceFileName;
}

bool QtDocParser::extractEnumDocumentation(const ClassDocumentation &classDocumentation,
                                           const QString &sourceFileName,
                                           AbstractMetaEnum &meta_enum)
{
    const auto index = classDocumentation.indexOfEnum(meta_enum.name());
    if (index == -1)
        return false;
    QString doc = classDocumentation.enums.at(index).description;
    const auto firstPara = doc.indexOf(u"<para>");
    if (firstPara != -1) {
        const QString baseClass = QtDocParser::enumBaseClass(meta_enum);
        if (baseClass != "Enum"_L1) {
            const QString note = "(inherits <teletype>enum."_L1 + baseClass
                                 + "</teletype>) "_L1;
            doc.insert(firstPara + 6, note);
        }
    }
    Documentation enumDoc(doc, {}, sourceFileName);
    meta_enum.setDocumentation(enumDoc);
    return true;
}

static QString qmlReferenceLink(const QFileInfo &qmlModuleFi)
{
    QString result;
    QTextStream(&result) << "<para>The module also provides <link"
        << R"( type="page" page="https://doc.qt.io/qt-)" << QT_VERSION_MAJOR
        << '/' << qmlModuleFi.baseName() << R"(.html")"
        << ">QML types</link>.</para>";
    return result;
}

Documentation QtDocParser::retrieveModuleDocumentation(const QString& name)
{
    // TODO: This method of acquiring the module name supposes that the target language uses
    // dots as module separators in package names. Improve this.
    QString completeModuleName = name;
    if (completeModuleName.endsWith("QtQuickControls2"_L1))
        completeModuleName.chop(1);
    const QString moduleName = completeModuleName.sliced(name.lastIndexOf(u'.') + 1);
    const QString lowerModuleName = moduleName.toLower();

    const QString prefix = documentationDataDirectory() + u'/'
                           + qdocModuleDir(completeModuleName) + u'/' + lowerModuleName;
    const QString sourceFile = prefix + "-index.webxml"_L1;
    if (!QFile::exists(sourceFile)) {
        qCWarning(lcShibokenDoc).noquote().nospace()
            << "Can't find qdoc file for module " <<  name << ", tried: "
            << QDir::toNativeSeparators(sourceFile);
        return {};
    }

    QString errorMessage;
    QString docString = webXmlModuleDescription(sourceFile, &errorMessage);
    if (!errorMessage.isEmpty()) {
        qCWarning(lcShibokenDoc, "%s", qPrintable(errorMessage));
        return {};
    }

    Documentation doc(docString, {}, sourceFile);
    if (doc.isEmpty()) {
        qCWarning(lcShibokenDoc, "%s",
                  qPrintable(msgCannotFindDocumentation(sourceFile, "module", name)));
        return doc;
    }

    // If a QML module info file exists, insert a link to the Qt docs.
    const QFileInfo qmlModuleFi(prefix + "-qmlmodule.webxml"_L1);
    if (qmlModuleFi.isFile()) {
        QString docString = doc.detailed();
        const int pos = docString.lastIndexOf(u"</description>");
        if (pos != -1) {
            docString.insert(pos, qmlReferenceLink(qmlModuleFi));
            doc.setDetailed(docString);
        }
    }

    return doc;
}
