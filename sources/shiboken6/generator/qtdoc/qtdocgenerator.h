// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#ifndef DOCGENERATOR_H
#define DOCGENERATOR_H

#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QScopedPointer>

#include "generator.h"
#include "documentation.h"
#include <optionsparser.h>
#include "typesystem_enums.h"
#include "modifications_typedefs.h"
#include "qtxmltosphinxinterface.h"

class DocParser;
struct DocGeneratorOptions;
struct GeneratorDocumentation;
struct DocPackage;

struct ResolvedDocImage;

/**
*   The DocGenerator generates documentation from library being binded.
*/
class QtDocGenerator : public Generator, public QtXmlToSphinxDocGeneratorInterface
{
public:
    Q_DISABLE_COPY_MOVE(QtDocGenerator)

    QtDocGenerator();
    ~QtDocGenerator() override;

    bool doSetup() override;

    const char* name() const override
    {
        return "QtDocGenerator";
    }

    static QList<OptionDescription> options();
    static std::shared_ptr<OptionsParser> createOptionsParser();

    // QtXmlToSphinxDocGeneratorInterface
    QString expandFunction(const QString &function) const override;
    QString expandClass(const QString &context,
                        const QString &name) const override;
    QString resolveContextForMethod(const QString &context,
                                    const QString &methodName) const override;
    const QLoggingCategory &loggingCategory() const override;
    QtXmlToSphinxLink resolveLink(const QtXmlToSphinxLink &) const override;

    static QString getFuncName(const AbstractMetaFunctionCPtr &cppFunc);
    static QString formatArgs(const AbstractMetaFunctionCPtr &func);

protected:
    bool shouldGenerate(const TypeEntryCPtr &) const override;
    static QString fileNameSuffix();
    QString fileNameForContext(const GeneratorContext &context) const override;
    void generateClass(TextStream &ts, const QString &targetDir,
                       const GeneratorContext &classContext,
                       QList<GeneratorContext> *contexts) override;
    bool finishGeneration() override;

private:
    void generateClassRecursion(TextStream &s, const QString &targetDir,
                                const GeneratorContext &classContext,
                                QList<GeneratorContext> *contexts);
    void doGenerateClass(TextStream &ts, const QString &targetDir,
                         const AbstractMetaClassCPtr &metaClass);
    void writeEnums(TextStream &s, const AbstractMetaEnumList &enums,
                    const QString &scope, QtXmlToSphinxImages *images) const;

    void writeFields(TextStream &s, const AbstractMetaClassCPtr &cppClass,
                     QtXmlToSphinxImages *images) const;
    void writeFunctions(TextStream &s, const AbstractMetaFunctionCList &funcs,
                        const AbstractMetaClassCPtr &cppClass, const QString &scope,
                        QtXmlToSphinxImages *images) const;
    void writeFunction(TextStream &s, const AbstractMetaFunctionCPtr &func,
                       QtXmlToSphinxImages *images,
                       const AbstractMetaClassCPtr &cppClass = {},
                       const QString &scope = {}, bool indexed = true) const;
    void writeFunctionDocumentation(TextStream &s, const AbstractMetaFunctionCPtr &func,
                                    const DocModificationList &modifications,
                                    const QString &scope,
                                    QtXmlToSphinxImages *images) const;
    void writeFunctionParametersType(TextStream &s, const AbstractMetaClassCPtr &cppClass,
                                     const AbstractMetaFunctionCPtr &func) const;
    static void writeFunctionToc(TextStream &s, const QString &title,
                                 const AbstractMetaFunctionCList &functions);
    static void writePropertyToc(TextStream &s,
                                 const GeneratorDocumentation &doc);
    void writeProperties(TextStream &s,
                         const GeneratorDocumentation &doc,
                         const AbstractMetaClassCPtr &cppClass,
                         QtXmlToSphinxImages *images) const;
    void writeParameterType(TextStream &s, const AbstractMetaClassCPtr &cppClass,
                            const AbstractMetaArgument &arg) const;
    void writeFormattedText(TextStream &s, const QString &doc,
                            Documentation::Format format,
                            const QString &scope,
                            QtXmlToSphinxImages *images) const;
    void writeFormattedBriefText(TextStream &s, const Documentation &doc,
                                 const QString &scope, QtXmlToSphinxImages *images) const;
    void writeFormattedDetailedText(TextStream &s, const Documentation &doc,
                                    const QString &scope,
                                    QtXmlToSphinxImages *images) const;

    bool writeInjectDocumentation(TextStream &s, TypeSystem::DocModificationMode mode,
                                  const AbstractMetaClassCPtr &cppClass,
                                  QtXmlToSphinxImages *images) const;
    bool writeInjectDocumentation(TextStream &s, TypeSystem::DocModificationMode mode,
                                  const DocModificationList &modifications,
                                  const AbstractMetaFunctionCPtr &func,
                                  const QString &scope,
                                  QtXmlToSphinxImages *images) const;
    bool writeDocModifications(TextStream &s, const DocModificationList &mods,
                               TypeSystem::DocModificationMode mode,
                               const QString &scope,
                               QtXmlToSphinxImages *images) const;
    static void writeDocSnips(TextStream &s, const CodeSnipList &codeSnips,
                              TypeSystem::CodeSnipPosition position, TypeSystem::Language language);

    void writeModuleDocumentation();
    void writeGlobals(const QString &package, const QString &fileName,
                      const DocPackage &docPackage);
    void writeAdditionalDocumentation() const;
    bool writeInheritanceFile();
    ResolvedDocImage resolveImage(const QtXmlToSphinxImage &image,
                                  const QStringList &sourceDirs,
                                  const QString &targetDir) const;
    void copyParsedImages(const QtXmlToSphinxImages &images,
                          const QStringList &sourceDocumentFiles,
                          const QString &targetDocumentFile) const;
    QString translateToPythonType(const AbstractMetaType &type,
                                  const AbstractMetaClassCPtr &cppClass,
                                  bool createRef = true) const;

    bool convertToRst(const QString &sourceFileName,
                      const QString &targetFileName,
                      const QString &context = QString(),
                      QString *errorMessage = nullptr) const;

    static GeneratorDocumentation generatorDocumentation(const AbstractMetaClassCPtr &cppClass);

    QStringList m_functionList;
    QMap<QString, DocPackage> m_packages;
    QScopedPointer<DocParser> m_docParser;
    static DocGeneratorOptions m_options;
};

#endif // DOCGENERATOR_H
