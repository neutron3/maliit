/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 * Copyright (C) 2013 Jolla Ltd.
 * Copyright (C) 2019 Chukwudi Nwutobo. 
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

#include "qmcontextadaptor.h"
#include "qmserverdbusaddress.h"
#include "qmserverproxy.h"

#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QTextFormat>
#include <QDebug>
#include <QByteArray>
#include <QRectF>
#include <QLocale>
#include <QWindow>
#include <QSharedDataPointer>

namespace
{
    const int SoftwareInputPanelHideTimer = 100;
    const char * const InputContextName = "MInputContext";

    int orientationAngle(Qt::ScreenOrientation orientation)
    {
        // Maliit uses orientations relative to screen, Qt relative to world
        // Note: doesn't work with inverted portrait or landscape as native screen orientation.
        static int portraitRotated = qGuiApp->primaryScreen()->primaryOrientation() == Qt::PortraitOrientation;

        switch (orientation) {
        case Qt::PrimaryOrientation: // Urgh.
        case Qt::PortraitOrientation:
            return portraitRotated = QMaliitPlatformInputContext::Angle270;
        case Qt::LandscapeOrientation:
            return portraitRotated = QMaliitPlatformInputContext::Angle0;
        case Qt::InvertedPortraitOrientation:
            return portraitRotated = QMaliitPlatformInputContext::Angle90;
        case Qt::InvertedLandscapeOrientation:
            return portraitRotated = QMaliitPlatformInputContext::Angle180;
        }
        return QMaliitPlatformInputContext::Angle0;
    }

    enum InputPanelState {
        InputPanelShowPending,   // input panel showing requested, but activation pending
        InputPanelShown,
        InputPanelHidden
    };
}

static QString maliitServerAddress()
{
    org::maliit::Server::Address serverAddress(QStringLiteral("org.maliit.server"), QStringLiteral("/org/maliit/server/address"), QDBusConnection::sessionBus());

    QString address(serverAddress.address());

    // Fallback to old socket when org.maliit.server service is not available
    if (address.isEmpty())
        return QStringLiteral("unix:path=/tmp/meego-im-uiserver/d->server_dbus");

    return address;
}

static Maliit::TextContentType contentType(Qt::InputMethodHints hints)
{
    Maliit::TextContentType type = Maliit::FreeTextContentType;
    hints &= Qt::ImhExclusiveInputMask;

    if (hints == Qt::ImhFormattedNumbersOnly || hints == Qt::ImhDigitsOnly) {
        type = Maliit::NumberContentType;
    } else if (hints == Qt::ImhDialableCharactersOnly) {
        type = Maliit::PhoneNumberContentType;
    } else if (hints == Qt::ImhEmailCharactersOnly) {
        type = Maliit::EmailContentType;
    } else if (hints == Qt::ImhUrlCharactersOnly) {
        type = Maliit::UrlContentType;
    }

    return type;
}

bool QMaliitPlatformInputContext::debug = false;

class QMaliitPlatformInputContextPrivate
{
public:
    QMaliitPlatformInputContextPrivate(QMaliitPlatformInputContext *qq);
    ~QMaliitPlatformInputContextPrivate()
    {
        delete adaptor;
        delete server;
    }

    void sendStateUpdate(bool focusChanged = false);

    QDBusConnection connection;
    ComMeegoInputmethodUiserver1Interface *server;
    QMaliitInputcontext1Adaptor *adaptor;;

    InputPanelState inputPanelState; // state for the input method server's software input panel

    bool valid;
    bool active; // is connection active
    bool correctionEnabled;
    QRect keyboardRectangle;
    QString preedit;
    QPointer<QWindow> window;
    QMap<QString, QVariant> imState;

    QMaliitPlatformInputContext *q;
};



QMaliitPlatformInputContext::QMaliitPlatformInputContext()
    : d(new QMaliitPlatformInputContextPrivate(this))
{
    if (debug)
        qDebug() << "QMaliitPlatformInputContext::QMaliitPlatformInputContext()";
}


QMaliitPlatformInputContext::~QMaliitPlatformInputContext()
{
    delete d;
}

bool QMaliitPlatformInputContext::isValid() const
{
    return d->valid;
}

void QMaliitPlatformInputContext::setLanguage(const QString &)
{

}

void QMaliitPlatformInputContext::reset()
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__;

    const bool hadPreedit = !d->preedit.isEmpty();
    if (hadPreedit && inputMethodAccepted()) {
        // ### selection
        QInputMethodEvent event;
        event.setCommitString(d->preedit);
        QGuiApplication::sendEvent(qGuiApp->focusObject(), &event);
        d->preedit.clear();
    }

    QDBusPendingReply<void> reply = d->server->reset();
    if (hadPreedit)
        reply.waitForFinished();
}

void QMaliitPlatformInputContext::invokeAction(QInputMethod::Action action, int x)
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__;

    if (!inputMethodAccepted())
        return;

    if (action == QInputMethod::Click) {
        if (x < 0 || x >= d->preedit.length()) {
            reset();
            return;
        }

        d->imState["preeditClickPos"] = x;
        d->sendStateUpdate();
        // The first argument is the mouse pos and the second is the
        // preedit rectangle. Both are unused on the server side.
        d->server->mouseClickedOnPreedit(0, 0, 0, 0, 0, 0);
    } else {
        QPlatformInputContext::invokeAction(action, x);
    }
}

void QMaliitPlatformInputContext::update(Qt::InputMethodQueries queries)
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__;

    if (!qGuiApp->focusObject())
        return;

    QInputMethodQueryEvent query(queries);
    QGuiApplication::sendEvent(qGuiApp->focusObject(), &query);

    if (queries & Qt::ImSurroundingText)
        d->imState["surroundingText"] = query.value(Qt::ImSurroundingText);
    if (queries & Qt::ImCursorPosition)
        d->imState["cursorPosition"] = query.value(Qt::ImCursorPosition);
    if (queries & Qt::ImAnchorPosition)
        d->imState["anchorPosition"] = query.value(Qt::ImAnchorPosition);
    if (queries & Qt::ImCursorRectangle) {
        QRect rect = query.value(Qt::ImCursorRectangle).toRect();
        rect = qGuiApp->inputMethod()->inputItemTransform().mapRect(rect);
        QWindow *window = qGuiApp->focusWindow();
        if (window)
            d->imState["cursorRectangle"] = QRect(window->mapToGlobal(rect.topLeft()), rect.size());
    }

    if (queries & Qt::ImCurrentSelection)
        d->imState["hasSelection"] = !query.value(Qt::ImCurrentSelection).toString().isEmpty();

    if (queries & Qt::ImHints) {
        Qt::InputMethodHints hints = Qt::InputMethodHints(query.value(Qt::ImHints).toUInt());

        d->imState["predictionEnabled"] = !(hints & Qt::ImhNoPredictiveText);
        d->imState["autocapitalizationEnabled"] = !(hints & Qt::ImhNoAutoUppercase);
        d->imState["hiddenText"] = (hints & Qt::ImhHiddenText) != 0;

        d->imState["contentType"] = contentType(hints);
    }

    d->sendStateUpdate(/*focusChanged*/true);
}

void QMaliitPlatformInputContext::updateServerOrientation(Qt::ScreenOrientation orientation)
{
    d->server->appOrientationChanged(orientationAngle(orientation));
}

void QMaliitPlatformInputContext::setFocusObject(QObject *focused)
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__ << focused;

    if (!d->valid)
        return;

    QWindow *window = qGuiApp->focusWindow();
    if (window != d->window.data()) {
        if (d->window)
            disconnect(d->window.data(), SIGNAL(contentOrientationChanged(Qt::ScreenOrientation)),
                       this, SLOT(updateServerOrientation(Qt::ScreenOrientation)));
        d->window = window;
        if (d->window)
            connect(d->window.data(), SIGNAL(contentOrientationChanged(Qt::ScreenOrientation)),
                    this, SLOT(updateServerOrientation(Qt::ScreenOrientation)));
    }

    d->imState["focusState"] = (focused != 0);
    if (inputMethodAccepted()) {
        if (window)
            d->imState["winId"] = static_cast<qulonglong>(window->winId());

        if (!d->active) {
            d->active = true;
            d->server->activateContext();

            if (window)
                d->server->appOrientationChanged(orientationAngle(window->contentOrientation()));
        }
    }
    d->sendStateUpdate(/*focusChanged*/true);
    if (inputMethodAccepted() && window && d->inputPanelState == InputPanelShown)
        showInputPanel();

}

QString QMaliitPlatformInputContext::preeditString()
{
    return d->preedit;
}

QRectF QMaliitPlatformInputContext::keyboardRect() const
{
    return d->keyboardRectangle;
}

bool QMaliitPlatformInputContext::isAnimating() const
{
    return false; // don't know here when input method server is actually doing transitions
}

void QMaliitPlatformInputContext::showInputPanel()
{
    if (debug) qDebug() << __PRETTY_FUNCTION__;

    if (debug)
        qDebug() << "showInputPanel";

    if (!inputMethodAccepted())
        d->inputPanelState = InputPanelShowPending;
    else {
        d->server->showInputMethod();
        d->inputPanelState = InputPanelShown;
        emitInputPanelVisibleChanged();
    }
}

void QMaliitPlatformInputContext::hideInputPanel()
{
    if (debug) qDebug() << __PRETTY_FUNCTION__;

    d->server->hideInputMethod();
    d->inputPanelState = InputPanelHidden;
    emitInputPanelVisibleChanged();
}

bool QMaliitPlatformInputContext::isInputPanelVisible() const
{
    return d->inputPanelState == InputPanelShown;
}

void QMaliitPlatformInputContext::activationLostEvent()
{
    // This method is called when activation was gracefully lost.
    // There is similar cleaning up done in onDBusDisconnection.
    d->active = false;
    d->inputPanelState = InputPanelHidden;
}


void QMaliitPlatformInputContext::imInitiatedHide()
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__;

    d->inputPanelState = InputPanelHidden;
    emitInputPanelVisibleChanged();
    // ### clear focus

}

void QMaliitPlatformInputContext::commitString(const QString &string, int replacementStart,
                                 int replacementLength, int  /*cursorPos*/)
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__;

    if (!inputMethodAccepted())
        return;

    d->preedit.clear();

    if (debug)
        qWarning() << "CommitString" << string;
    // ### start/cursorPos
    QInputMethodEvent event;
    event.setCommitString(string, replacementStart, replacementLength);
    QCoreApplication::sendEvent(qGuiApp->focusObject(), &event);
}


void QMaliitPlatformInputContext::updatePreedit(const QDBusMessage &message)
{
    if (debug) {
        qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__ ;
    }

    if (!inputMethodAccepted())
        return;

    QList<QVariant> arguments = message.arguments();
    if (arguments.count() != 5) {
        qWarning() << "QMaliitPlatformInputContext::updatePreedit: Received message from input method server with wrong parameters.";
        return;
    }

    d->preedit = arguments[0].toString();

    QList<QInputMethodEvent::Attribute> attributes;

    const QDBusArgument formats = arguments[1].value<QDBusArgument>();
    formats.beginArray();
    while (!formats.atEnd()) {
        formats.beginStructure();
        int start, length, preeditFace;
        formats >> start >> length >> preeditFace;
        formats.endStructure();

        QTextCharFormat format;

        enum PreeditFace {
            PreeditDefault,
            PreeditNoCandidates,
            PreeditKeyPress,      //!< Used for displaying the hwkbd key just pressed
            PreeditUnconvertible, //!< Inactive preedit region, not clickable
            PreeditActive,        //!< Preedit region with active suggestions

        };
        switch (PreeditFace(preeditFace)) {
        case PreeditDefault:
        case PreeditKeyPress:
            format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
            format.setUnderlineColor(QColor(0, 0, 0));
            break;
        case PreeditNoCandidates:
            format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
            format.setUnderlineColor(QColor(255, 0, 0));
            break;
        case PreeditUnconvertible:
            format.setForeground(QBrush(QColor(128, 128, 128)));
            break;
        case PreeditActive:
            format.setForeground(QBrush(QColor(153, 50, 204)));
            format.setFontWeight(QFont::Bold);
            break;
        default:
            break;
        }

        attributes << QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, start, length, format);
    }
    formats.endArray();

    int replacementStart = arguments[2].toInt();
    int replacementLength = arguments[3].toInt();
    int cursorPos = arguments[4].toInt();

    if (debug)
        qWarning() << "updatePreedit" << d->preedit << replacementStart << replacementLength << cursorPos;

    if (cursorPos >= 0)
        attributes << QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, cursorPos, 1, QVariant());

    QInputMethodEvent event(d->preedit, attributes);
    if (replacementStart || replacementLength)
        event.setCommitString(QString(), replacementStart, replacementLength);
    QCoreApplication::sendEvent(qGuiApp->focusObject(), &event);

}

void QMaliitPlatformInputContext::keyEvent(int type, int key, int modifiers, const QString &text,
                             bool autoRepeat, int count, uchar requestType_)
{
    if (debug) qDebug() << InputContextName << "in" << __PRETTY_FUNCTION__;

    Maliit::EventRequestType requestType = Maliit::EventRequestType(requestType_);
    if (requestType == Maliit::EventRequestSignalOnly) {
        qWarning() << "Maliit: Signal emitted key events are not supported.";
        return;
    }

    // HACK: This code relies on QEvent::Type for key events and modifiers to be binary compatible between
    // Qt 4 and 5.
    QEvent::Type eventType = static_cast<QEvent::Type>(type);
    if (type != QEvent::KeyPress && type != QEvent::KeyRelease) {
        qWarning() << "Maliit: Unknown key event type" << type;
        return;
    }

    QKeyEvent event(eventType, key, static_cast<Qt::KeyboardModifiers>(modifiers),
                    text, autoRepeat, count);
    if (d->window)
        QCoreApplication::sendEvent(d->window.data(), &event);
}

bool QMaliitPlatformInputContext::preeditRectangle(int &x, int &y, int &width, int &height)
{
    // ###
    QRect r = qApp->inputMethod()->cursorRectangle().toRect();
    if (!r.isValid())
        return false;
    x = r.x();
    y = r.y();
    width = r.width();
    height = r.height();
    return true;
}

bool QMaliitPlatformInputContext::selection(QString &selection)
{
    selection.clear();

    if (!inputMethodAccepted())
        return false;

    QInputMethodQueryEvent query(Qt::ImCurrentSelection);
    QGuiApplication::sendEvent(qGuiApp->focusObject(), &query);
    QVariant value = query.value(Qt::ImCurrentSelection);
    if (!value.isValid())
        return false;

    selection = value.toString();
    return true;
}

void QMaliitPlatformInputContext::updateInputMethodArea(int x, int y, int width, int height)
{
    bool wasVisible = isInputPanelVisible();

    d->keyboardRectangle = QRect(x, y, width, height);
    emitKeyboardRectChanged();

    if (wasVisible != isInputPanelVisible()) {
        emitInputPanelVisibleChanged();
    }

}


void QMaliitPlatformInputContext::setGlobalCorrectionEnabled(bool enabled)
{
    d->correctionEnabled = enabled;
}

void QMaliitPlatformInputContext::onInvokeAction(const QString &action, const QKeySequence &sequence)
{
    if (debug) qDebug() << InputContextName << __PRETTY_FUNCTION__ << "action" << action;

    // NOTE: currently not trying to trigger action directly
    static const Qt::KeyboardModifiers AllModifiers = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier
            | Qt::MetaModifier | Qt::KeypadModifier;

    Maliit::EventRequestType requestType = Maliit::EventRequestType();

    for (int i = 0; i < sequence.count(); i++) {
        const int key = sequence[i] & ~AllModifiers;
        const int modifiers = sequence[i] & AllModifiers;
        QString text("");
        if (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier) {
            text = QString(key);
        }

        keyEvent(QEvent::KeyPress, key, modifiers, text, false, 1, requestType);
        keyEvent(QEvent::KeyRelease, key, modifiers, text, false, 1, requestType);
    }
}

void QMaliitPlatformInputContext::setRedirectKeys(bool enabled)
{
    Q_UNUSED(enabled)
}

void QMaliitPlatformInputContext::setDetectableAutoRepeat(bool enabled)
{
    Q_UNUSED(enabled);
    if (debug) qWarning() << "Detectable autorepeat not supported.";
}

void QMaliitPlatformInputContext::setSelection(int start, int length)
{
    if (!inputMethodAccepted())
        return;

    QList<QInputMethodEvent::Attribute> attributes;
    attributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, start, length, QVariant());
    QInputMethodEvent event(QString(), attributes);
    QGuiApplication::sendEvent(qGuiApp->focusObject(), &event);
}

QMaliitPlatformInputContextPrivate::QMaliitPlatformInputContextPrivate(QMaliitPlatformInputContext* qq)
    : connection(QDBusConnection::connectToPeer(maliitServerAddress(), QLatin1String("MaliitIMProxy")))
    , server(nullptr)
    , adaptor(nullptr)
    , inputPanelState(InputPanelHidden)
    , valid(false)
    , active(false)
    , correctionEnabled(false)
    , q(qq)
{
    if (!connection.isConnected())
        return;

    server = new ComMeegoInputmethodUiserver1Interface(QString(""), QStringLiteral("/com/meego/inputmethod/uiserver1"), connection);
    adaptor = new QMaliitInputcontext1Adaptor(qq);
    connection.registerObject("/com/meego/inputmethod/inputcontext", qq);

    enum InputMethodMode {
        //! Normal mode allows to use preedit and error correction
        InputMethodModeNormal,

        //! Virtual keyboard sends QKeyEvent for every key press or release
        InputMethodModeDirect,

        //! Used with proxy widget
        InputMethodModeProxy
    };
    imState["inputMethodMode"] = InputMethodModeNormal;

    imState["correctionEnabled"] = true;

    valid = true;
}

void QMaliitPlatformInputContextPrivate::sendStateUpdate(bool focusChanged)
{
    server->updateWidgetInformation(imState, focusChanged);
}

