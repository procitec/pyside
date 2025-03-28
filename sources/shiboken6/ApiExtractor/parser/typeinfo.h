// Copyright (C) 2020 The Qt Company Ltd.
// Copyright (C) 2002-2005 Roberto Raggi <roberto@kdevelop.org>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef TYPEINFO_H
#define TYPEINFO_H

#include "codemodel_enums.h"
#include "codemodel_fwd.h"

#include <QtCore/QString>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QtCompare>
#include <QtCore/QStringList>

#include <utility>

QT_FORWARD_DECLARE_CLASS(QDebug)
QT_FORWARD_DECLARE_CLASS(QTextStream)

class TypeInfoData;

class TypeInfo
{
    friend class TypeParser;
public:
    using Indirections = QList<Indirection>;
    using TypeInfoList = QList<TypeInfo>;

    TypeInfo();
    ~TypeInfo();
    TypeInfo(const TypeInfo &);
    TypeInfo& operator=(const TypeInfo &);
    TypeInfo(TypeInfo &&) noexcept;
    TypeInfo &operator=(TypeInfo &&) noexcept;

    static TypeInfo voidType();
    static TypeInfo varArgsType();

    QStringList qualifiedName() const;
    void setQualifiedName(const QStringList &qualified_name);
    void addName(const QString &);

    bool isVoid() const;

    bool isConstant() const;
    void setConstant(bool is);

    bool isVolatile() const;

    void setVolatile(bool is);

    ReferenceType referenceType() const;
    void setReferenceType(ReferenceType r);

    const Indirections &indirectionsV() const;
    void setIndirectionsV(const Indirections &i);
    void addIndirection(Indirection i);

    // "Legacy", rename?
    int indirections() const;

    void setIndirections(int indirections);

    bool isFunctionPointer() const;
    void setFunctionPointer(bool is);

    const QStringList &arrayElements() const;
    void setArrayElements(const QStringList &arrayElements);

    void addArrayElement(const QString &a);

    const TypeInfoList &arguments() const;
    void setArguments(const TypeInfoList &arguments);
    void addArgument(const TypeInfo &arg);

    const TypeInfoList &instantiations() const;
    TypeInfoList &instantiations(); // for parsing only
    void setInstantiations(const TypeInfoList &i);
    void addInstantiation(const TypeInfo &i);
    void clearInstantiations();

    bool isPlain() const; // neither const,volatile, no indirections/references, array

    bool isStdType() const;

    std::pair<qsizetype, qsizetype>
        parseTemplateArgumentList(const QString &l, qsizetype from = 0);

    // ### arrays and templates??

    QString toString() const;

    static TypeInfo combine(const TypeInfo &__lhs, const TypeInfo &__rhs);
    static TypeInfo resolveType(TypeInfo const &__type, const ScopeModelItem &__scope);

    void formatTypeSystemSignature(QTextStream &str) const;

#ifndef QT_NO_DEBUG_STREAM
    void formatDebug(QDebug &d) const;
#endif

    static QString indirectionKeyword(Indirection i);

    static bool stripLeadingConst(QString *s);
    static bool stripLeadingVolatile(QString *s);
    static bool stripLeadingQualifier(QLatin1StringView qualifier, QString *s);
    static void stripQualifiers(QString *s);

    void simplifyStdType();

private:
    friend bool comparesEqual(const TypeInfo &lhs,
                              const TypeInfo &rhs) noexcept;
    Q_DECLARE_EQUALITY_COMPARABLE(TypeInfo)

    QSharedDataPointer<TypeInfoData> d;

    friend class TypeInfoTemplateArgumentHandler;

    static TypeInfo resolveType(const CodeModelItem &item, TypeInfo const &__type,
                                const ScopeModelItem &__scope);
};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const TypeInfo &t);
#endif

#endif // TYPEINFO_H
