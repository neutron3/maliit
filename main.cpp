/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 *
 * All rights reserved.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include "qmaliitplatforminputcontext.h"

#include <qpa/qplatforminputcontextplugin_p.h>
#include <QtCore/QStringList>
#include <QDebug>

QT_BEGIN_NAMESPACE

class QMaliitPlatformInputContextPlugin: public QPlatformInputContextPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformInputContextFactoryInterface_iid FILE "maliit.json")

public:
    QMaliitPlatformInputContext *create(const QString &, const QStringList &) override;
};

QMaliitPlatformInputContext *QMaliitPlatformInputContextPlugin::create(const QString &system, const QStringList &paramList)
{
    Q_UNUSED(paramList);

    if (system.compare(system, QStringLiteral("minputcontext"), Qt::CaseInsensitive) == 0) {
        return new QMaliitPlatformInputContext;
    }
    return nullptr;
}

QT_END_NAMESPACE

#include "main.moc"
