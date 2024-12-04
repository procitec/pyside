// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "documentation.h"

#include <QtCore/QDebug>

Documentation::Documentation(const QString &detailed,
                             const QString &brief,
                             const QString &sourceFile,
                             Format fmt) :
    m_detailed(detailed.trimmed()),
    m_brief(brief.trimmed()),
    m_sourceFile(sourceFile),
    m_format(fmt)
{
}

bool Documentation::isEmpty() const
{
    return m_detailed.isEmpty() && m_brief.isEmpty();
}

Documentation::Format Documentation::format() const
{
    return m_format;
}

void Documentation::setValue(const QString &value, Documentation::Type t)
{
    if (t == Brief)
        setBrief(value);
    else
        setDetailed(value);
}

void Documentation::setFormat(Documentation::Format f)
{
    m_format = f;
}

void Documentation::setDetailed(const QString &detailed)
{
    m_detailed = detailed.trimmed();
}

void Documentation::setBrief(const QString &brief)
{
    m_brief = brief.trimmed();
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug debug, const Documentation &d)
{
    QDebugStateSaver saver(debug);
    debug.noquote();
    debug.nospace();
    debug << "Documentation(";
    if (!d.isEmpty()) {
        debug << "format=" << d.format();
        if (d.hasSourceFile())
            debug << ", file=\"" << d.sourceFile() << '"';
        if (!d.brief().isEmpty())
            debug << ", brief=\"" << d.brief() << '"';
        if (!d.detailed().isEmpty())
            debug << ", detailed=\"" << d.detailed() << '"';
    }
    debug << ')';
    return debug;
}
#endif // QT_NO_DEBUG_STREAM
