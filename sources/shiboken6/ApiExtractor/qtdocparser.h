// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef QTDOCPARSER_H
#define QTDOCPARSER_H

#include "docparser.h"

struct ClassDocumentation;

class QtDocParser : public DocParser
{
public:
    QtDocParser() = default;
    QString fillDocumentation(const AbstractMetaClassPtr &metaClass) override;
    void fillGlobalFunctionDocumentation(const AbstractMetaFunctionPtr &f) override;
    void fillGlobalEnumDocumentation(AbstractMetaEnum &e) override;

    Documentation retrieveModuleDocumentation() override;
    Documentation retrieveModuleDocumentation(const QString& name) override;

    static QString qdocModuleDir(const QString &pythonType);

private:
    static QString functionDocumentation(const QString &sourceFileName,
                                         const ClassDocumentation &classDocumentation,
                                         const AbstractMetaClassCPtr &metaClass,
                                         const AbstractMetaFunctionCPtr &func,
                                         QString *errorMessage);

    static QString queryFunctionDocumentation(const QString &sourceFileName,
                                              const ClassDocumentation &classDocumentation,
                                              const AbstractMetaClassCPtr &metaClass,
                                              const AbstractMetaFunctionCPtr &func,
                                              QString *errorMessage);
    static bool extractEnumDocumentation(const ClassDocumentation &classDocumentation,
                                         const QString &sourceFileName,
                                         AbstractMetaEnum &meta_enum);

};

#endif // QTDOCPARSER_H

