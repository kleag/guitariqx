/*
 * Copyright (C) 2026 Gaël de Chalendar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "systemkeyboard.h"

#include <QFile>
#include <QGuiApplication>
#include <QInputMethodQueryEvent>
#include <QPointerEvent>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QWindow>

void SystemKeyboard::install(QGuiApplication *app) {
    const QString mode = qEnvironmentVariable("GUITARIQX_SYSTEM_KEYBOARD");
    // xcb: under XWayland when qt6-wayland is missing
    const QString platform = QGuiApplication::platformName();
    if ((!platform.startsWith(QLatin1String("wayland")) && platform != QLatin1String("xcb"))
        || mode == QLatin1String("0")) {
        return;
    }
    const QString busctl = QStandardPaths::findExecutable(QStringLiteral("busctl"));
    if (busctl.isEmpty()) {
        return;
    }
    new SystemKeyboard(
        [busctl](bool visible) {
            // --auto-start=no: never start a keyboard on desktops without
            // one running; errors (no keyboard service) are not shown
            QProcess p;
            p.setProgram(busctl);
            p.setArguments({QStringLiteral("--user"), QStringLiteral("--auto-start=no"),
                            QStringLiteral("call"), QStringLiteral("sm.puri.OSK0"),
                            QStringLiteral("/sm/puri/OSK0"), QStringLiteral("sm.puri.OSK0"),
                            QStringLiteral("SetVisible"), QStringLiteral("b"),
                            visible ? QStringLiteral("true") : QStringLiteral("false")});
            p.setStandardOutputFile(QProcess::nullDevice());
            p.setStandardErrorFile(QProcess::nullDevice());
            p.startDetached();
        },
        [always = mode == QLatin1String("1")]() { return always || !hasPhysicalKeyboard(); },
        app);
}

SystemKeyboard::SystemKeyboard(Setter setter, Needed needed, QObject *parent)
    : QObject(parent), m_setter(std::move(setter)), m_needed(std::move(needed)) {
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(hideDelay);
    connect(&m_hideTimer, &QTimer::timeout, this, &SystemKeyboard::hide);
    auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    connect(app, &QGuiApplication::focusObjectChanged, this, &SystemKeyboard::focusChanged);
    app->installEventFilter(this);
}

bool SystemKeyboard::hasPhysicalKeyboard(const QByteArray &devices) {
    // "B: KEY=" is the key bitmap in hexadecimal words (unsigned long),
    // most significant first; Q..P (keys 16-25), A and S (30, 31) are in
    // the last word with 32 as with 64 bit words
    constexpr quint32 letters = 0xC3FF0000;
    for (const QByteArray &line : devices.split('\n')) {
        if (line.startsWith("B: KEY=")) {
            const QByteArray last = line.mid(7).trimmed().split(' ').constLast();
            const quint32 low = quint32(last.right(8).toUInt(nullptr, 16));
            if ((low & letters) == letters) {
                return true;
            }
        }
    }
    return false;
}

bool SystemKeyboard::hasPhysicalKeyboard() {
    QFile file(QStringLiteral("/proc/bus/input/devices"));
    // a sequential file: readAll, not size()
    return file.open(QIODevice::ReadOnly) && hasPhysicalKeyboard(file.readAll());
}

bool SystemKeyboard::acceptsText(QObject *object) {
    if (!object) {
        return false;
    }
    QInputMethodQueryEvent query(Qt::ImEnabled);
    QCoreApplication::sendEvent(object, &query);
    return query.value(Qt::ImEnabled).toBool();
}

void SystemKeyboard::focusChanged(QObject *focus) {
    if (!acceptsText(focus)) {
        if (m_visible) {
            m_hideTimer.start();
        }
    } else if (!m_pressed && !focus->property("keyboardOnTap").toBool()) {
        // given by the program (a dialog opening); during a press the
        // release decides (a tap on the buttons of a SpinBox focuses its
        // text but is no request to type)
        show();
    } else {
        m_hideTimer.stop();
    }
}

void SystemKeyboard::show() {
    m_hideTimer.stop();
    if ((m_lastShow.isValid() && m_lastShow.elapsed() < 300) || !m_needed()) {
        return;
    }
    m_lastShow.start();
    m_visible = true;
    QWindow *window = QGuiApplication::focusWindow();
    if (window && window->visibility() == QWindow::FullScreen) {
        m_fullScreenWindow = window;
        window->showMaximized();
    }
    m_setter(true);
}

void SystemKeyboard::hide() {
    if (m_visible) {
        m_visible = false;
        m_lastShow.invalidate();
        m_setter(false);
        // not when full screen was left meanwhile by other means
        if (m_fullScreenWindow && m_fullScreenWindow->visibility() == QWindow::Maximized) {
            m_fullScreenWindow->showFullScreen();
        }
        m_fullScreenWindow = nullptr;
    }
}

bool SystemKeyboard::eventFilter(QObject *watched, QEvent *event) {
    const QEvent::Type type = event->type();
    if (type == QEvent::MouseButtonPress || type == QEvent::TouchBegin) {
        m_pressed = qobject_cast<QQuickWindow *>(watched) != nullptr || m_pressed;
    } else if (type == QEvent::MouseButtonRelease || type == QEvent::TouchEnd) {
        // a tap into the focused text field shows the keyboard (again)
        auto *window = qobject_cast<QQuickWindow *>(watched);
        auto *pointer = static_cast<QPointerEvent *>(event);
        if (window && pointer->pointCount() > 0) {
            m_pressed = false;
            auto *item = qobject_cast<QQuickItem *>(window->focusObject());
            const QPointF pos = pointer->point(0).scenePosition();
            if (item && acceptsText(item) && item->contains(item->mapFromScene(pos))) {
                show();
            }
        }
    }
    return false;
}
