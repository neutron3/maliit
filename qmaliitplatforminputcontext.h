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

#ifndef QMALIITPLATFORMINPUTCONTEXT_H
#define QMALIITPLATFORMINPUTCONTEXT_H

#include "qmnamespace.h"

#include <QObject>
#include <QTimer>
#include <QPointer>
#include <QRect>
#include <QDBusArgument>

#include <qpa/qplatforminputcontext.h>

class QMaliitPlatformInputContextPrivate;
class QDBusMessage;
class QMaliitPlatformInputContext : public QPlatformInputContext
{
    Q_OBJECT
    // Exposing preedit state as an extension. Use only if you know what you're doing.
    Q_PROPERTY(QString preedit READ preeditString NOTIFY preeditChanged)

public:
    enum OrientationAngle {
        Angle0   =   0,
        Angle90  =  90,
        Angle180 = 180,
        Angle270 = 270
    };

    QMaliitPlatformInputContext();
    virtual ~QMaliitPlatformInputContext();

    bool isValid() const override;
    void reset() override;
    void update(Qt::InputMethodQueries) override;
    void invokeAction(QInputMethod::Action, int cursorPosition) override;
    QRectF keyboardRect() const override;
    bool isAnimating() const override;
    void showInputPanel() override;
    void hideInputPanel() override;
    bool isInputPanelVisible() const override;
    void setFocusObject(QObject *object) override;

    QString preeditString();

public Q_SLOTS:
    // Hooked up to the input method server
    void activationLostEvent();
    void imInitiatedHide();

    void commitString(const QString &string, int replacementStart = 0,
                      int replacementLength = 0, int cursorPos = -1);

    void updatePreedit(const QDBusMessage &message);

    void keyEvent(int type, int key, int modifiers, const QString &text, bool autoRepeat,
                  int count, uchar requestType_);
    bool preeditRectangle(int &x, int &y, int &width, int &height);
    bool selection(QString &selection);
    void updateInputMethodArea(int x, int y, int width, int height);
    void setGlobalCorrectionEnabled(bool);
    void onInvokeAction(const QString &action, const QKeySequence &sequence);
    void setRedirectKeys(bool enabled);
    void setDetectableAutoRepeat(bool enabled);
    void setSelection(int start, int length);
    void setLanguage(const QString &);
    // End input method server connection slots.

private Q_SLOTS:
    void updateServerOrientation(Qt::ScreenOrientation orientation);

Q_SIGNALS:
    void preeditChanged();

private:
    QMaliitPlatformInputContextPrivate *d;

    static bool debug;
};

#endif
