// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangutils.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QHashFunctions>
#include <QtCore/QProcess>

#include <string_view>

bool operator==(const CXCursor &c1, const CXCursor &c2) noexcept
{
    return c1.kind == c2.kind
        && c1.xdata == c2.xdata
        && std::equal(c1.data, c1.data + sizeof(c1.data) / sizeof(c1.data[0]), c2.data);
}

size_t qHash(const CXCursor &c, size_t seed) noexcept
{
    return qHashMulti(seed, c.kind, c.xdata, c.data[0], c.data[1], c.data[2]);
}

bool operator==(const CXType &t1, const CXType &t2) noexcept
{
    return t1.kind == t2.kind && t1.data[0] == t2.data[0]
        && t1.data[1] == t2.data[1];
}

size_t qHash(const CXType &ct, size_t seed) noexcept
{
    return qHashMulti(seed, ct.kind, ct.data[0], ct.data[1]);
}

namespace clang {

SourceLocation getExpansionLocation(const CXSourceLocation &location)
{
    SourceLocation result;
    clang_getExpansionLocation(location, &result.file, &result.line, &result.column, &result.offset);
    return result;
}

QString getFileName(CXFile file)
{
    QString result;
    const CXString cxFileName = clang_getFileName(file);
    // Has been observed to be 0 for invalid locations
    if (const char *cFileName = clang_getCString(cxFileName))
        result = QString::fromUtf8(cFileName);
    clang_disposeString(cxFileName);
    return result;
}

SourceLocation getCursorLocation(const CXCursor &cursor)
{
    const CXSourceRange extent = clang_getCursorExtent(cursor);
    return getExpansionLocation(clang_getRangeStart(extent));
}

CXString getFileNameFromLocation(const CXSourceLocation &location)
{
    CXFile file{};
    unsigned line{};
    unsigned column{};
    unsigned offset{};
    clang_getExpansionLocation(location, &file, &line, &column, &offset);
    return clang_getFileName(file);
}

SourceRange getCursorRange(const CXCursor &cursor)
{
    const CXSourceRange extent = clang_getCursorExtent(cursor);
    return std::make_pair(getExpansionLocation(clang_getRangeStart(extent)),
                          getExpansionLocation(clang_getRangeEnd(extent)));
}

QString getCursorKindName(CXCursorKind cursorKind)
{
    CXString kindName  = clang_getCursorKindSpelling(cursorKind);
    const QString result = QString::fromUtf8(clang_getCString(kindName));
    clang_disposeString(kindName);
    return result;
}

QString getCursorSpelling(const CXCursor &cursor)
{
    CXString cursorSpelling = clang_getCursorSpelling(cursor);
    const QString result = QString::fromUtf8(clang_getCString(cursorSpelling));
    clang_disposeString(cursorSpelling);
    return result;
}

QString getCursorDisplayName(const CXCursor &cursor)
{
    CXString displayName = clang_getCursorDisplayName(cursor);
    const QString result = QString::fromUtf8(clang_getCString(displayName));
    clang_disposeString(displayName);
    return result;
}

static inline bool isBuiltinType(CXTypeKind kind)
{
    return kind >= CXType_FirstBuiltin && kind <= CXType_LastBuiltin;
}

// Resolve elaborated types occurring with clang 16
static CXType resolveElaboratedType(const CXType &type)
{
    if (!isBuiltinType(type.kind)) {
        CXCursor decl = clang_getTypeDeclaration(type);
        auto resolvedType = clang_getCursorType(decl);
        if (resolvedType.kind != CXType_Invalid && resolvedType.kind != type.kind)
            return resolvedType;
    }
    return type;
}

// Resolve typedefs
static CXType resolveTypedef(const CXType &type)
{
    auto result = type;
    while (result.kind == CXType_Typedef) {
        auto decl = clang_getTypeDeclaration(result);
        auto resolved = clang_getTypedefDeclUnderlyingType(decl);
        if (resolved.kind == CXType_Invalid)
            break;
        result = resolved;
    }
    return result;
}

// Fully resolve a type from elaborated & typedefs
CXType fullyResolveType(const CXType &type)
{
    return resolveTypedef(resolveElaboratedType(type));
}

QString getTypeName(const CXType &type)
{
    CXString typeSpelling = clang_getTypeSpelling(type);
    const QString result = QString::fromUtf8(clang_getCString(typeSpelling));
    clang_disposeString(typeSpelling);
    return result;
}

// Quick check for "::Type"
bool hasScopeResolution(const CXType &type)
{
    CXString typeSpelling = clang_getTypeSpelling(type);
    std::string_view spelling = clang_getCString(typeSpelling);
    const bool result = spelling.compare(0, 2, "::") == 0
        || spelling.find(" ::") != std::string::npos;
    clang_disposeString(typeSpelling);
    return result;
}

// Resolve elaborated types occurring with clang 16
QString getResolvedTypeName(const CXType &type)
{
    return getTypeName(resolveElaboratedType(type));
}

Diagnostic::Diagnostic(const QString &m, const CXCursor &c, CXDiagnosticSeverity s)
    : message(m), source(Other), severity(s)
{
    setLocation(getCursorLocation(c));
}

Diagnostic Diagnostic::fromCXDiagnostic(CXDiagnostic cd)
{
    Diagnostic result;
    result.source = Clang;
    CXString spelling = clang_getDiagnosticSpelling(cd);
    result.message = QString::fromUtf8(clang_getCString(spelling));
    clang_disposeString(spelling);
    result.severity = clang_getDiagnosticSeverity(cd);
    result.setLocation(getExpansionLocation(clang_getDiagnosticLocation(cd)));

    CXDiagnosticSet childDiagnostics = clang_getChildDiagnostics(cd);
    if (const unsigned childCount = clang_getNumDiagnosticsInSet(childDiagnostics)) {
        result.childMessages.reserve(int(childCount));
        const unsigned format = clang_defaultDiagnosticDisplayOptions();
        for (unsigned i = 0; i < childCount; ++i) {
            CXDiagnostic childDiagnostic = clang_getDiagnosticInSet(childDiagnostics, i);
            CXString cdm = clang_formatDiagnostic(childDiagnostic, format);
            result.childMessages.append(QString::fromUtf8(clang_getCString(cdm)));
            clang_disposeString(cdm);
            clang_disposeDiagnostic(childDiagnostic);
        }
    }

    return result;
}

void Diagnostic::setLocation(const SourceLocation &sourceLocation)
{
    file = getFileName(sourceLocation.file);
    line = sourceLocation.line;
    column = sourceLocation.column;
    offset = sourceLocation.offset;
}

QList<Diagnostic> getDiagnostics(CXTranslationUnit tu)
{
    QList<Diagnostic> result;
    const unsigned count = clang_getNumDiagnostics(tu);
    result.reserve(int(count));
    for (unsigned i = 0; i < count; ++i) {
        const CXDiagnostic d = clang_getDiagnostic(tu, i);
        result.append(Diagnostic::fromCXDiagnostic(d));
        clang_disposeDiagnostic(d);
    }
    return result;
}

std::pair<qsizetype, qsizetype>
    parseTemplateArgumentList(const QString &l,
                              const TemplateArgumentHandler &handler,
                              qsizetype from)
{
    const auto ltPos = l.indexOf(u'<', from);
    if (ltPos == - 1)
        return std::make_pair(-1, -1);
    auto startPos = ltPos + 1;
    int level = 1;
    for (qsizetype p = startPos, end = l.size(); p < end; ) {
        const char c = l.at(p).toLatin1();
        switch (c) {
        case ',':
        case '>':
            handler(level, QStringView{l}.mid(startPos, p - startPos).trimmed());
            ++p;
            if (c == '>') {
                if (--level == 0)
                    return std::make_pair(ltPos, p);
                // Skip over next ',': "a<b<c,d>,e>"
                for (; p < end && (l.at(p).isSpace() || l.at(p) == u','); ++p) {}
            }
            startPos = p;
            break;
        case '<':
            handler(level, QStringView{l}.mid(startPos, p - startPos).trimmed());
            ++level;
            startPos = ++p;
            break;
        default:
             ++p;
            break;
        }
    }
    return std::make_pair(-1, -1);
}

CXDiagnosticSeverity maxSeverity(const QList<Diagnostic> &ds)
{
    CXDiagnosticSeverity result = CXDiagnostic_Ignored;
    for (const Diagnostic& d : ds) {
        if (d.severity > result)
            result = d.severity;
    }
    return result;
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug s, const SourceLocation &l)
{
    QDebugStateSaver saver(s);
    s.nospace();
    s.noquote();
    s << QDir::toNativeSeparators(clang::getFileName(l.file)) << ':' << l.line;
    if (l.column)
        s << ':' << l.column;
    return s;
}

// Roughly follow g++ format:
// file.cpp:214:37: warning: cast from pointer to integer of different size [-Wpointer-to-int-cast]
QDebug operator<<(QDebug s, const Diagnostic &d)
{
    QDebugStateSaver saver(s);
    s.nospace();
    s.noquote();
    s << d.file << ':'<< d.line << ':' << d.column << ": ";
    switch (d.severity) {
    case CXDiagnostic_Ignored:
        s << "ignored";
        break;
    case CXDiagnostic_Note:
        s << "note";
        break;
    case CXDiagnostic_Warning:
        s << "warning";
        break;
    case CXDiagnostic_Error:
        s << "error";
        break;
    case CXDiagnostic_Fatal:
        s << "fatal";
        break;
    }
    s << ": " << d.message;

    if (d.source != Diagnostic::Clang)
        s << " [other]";

    if (const auto childMessagesCount = d.childMessages.size()) {
        s << '\n';
        for (qsizetype i = 0; i < childMessagesCount; ++i)
            s << "   " << d.childMessages.at(i) << '\n';
    }

    return s;
}

#endif // QT_NO_DEBUG_STREAM

} // namespace clang
