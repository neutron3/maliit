/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef SERVERADDRESSPROXY_H
#define SERVERADDRESSPROXY_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface org.maliit.Server.Address
 */
class OrgMaliitServerAddressInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.maliit.Server.Address"; }

public:
    OrgMaliitServerAddressInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr);

    ~OrgMaliitServerAddressInterface() override;

    Q_PROPERTY(QString address READ address)
    inline QString address() const
    { return qvariant_cast< QString >(property("address")); }

public Q_SLOTS: // METHODS
Q_SIGNALS: // SIGNALS
};

namespace org {
  namespace maliit {
    namespace Server {
      using Address = ::OrgMaliitServerAddressInterface;
    }
  }
}
#endif
