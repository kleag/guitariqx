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

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <functional>

class QGuiApplication;

/*
 * Shows the desktop's on-screen keyboard while a text field has the focus.
 *
 * On Wayland the keyboard (squeekboard on Raspberry Pi OS, Phosh...) pops
 * up by itself only for clients speaking text-input-unstable-v3, which the
 * Qt 6.4 of Debian 12 / Raspberry Pi OS Bookworm lacks (it has v1, v2 and
 * v4 only), as do X11 clients. So the keyboard is driven explicitly through its D-Bus
 * interface sm.puri.OSK0 (SetVisible), with busctl rather than Qt DBus,
 * which is not among the modules guitariqx may use.
 *
 * Only when no physical keyboard is plugged in (checked each time, as one
 * may be plugged in later): squeekboard types through a Wayland virtual
 * keyboard, not a kernel input device, so it does not count.
 *
 * The keyboard is shown when the program gives the focus to an item
 * accepting input method events (TextField...), unless that item has a
 * property keyboardOnTap set to true, and on a tap into the focused field
 * (it may have been closed by hand), but not when a tap elsewhere focuses
 * a field (the buttons of an editable SpinBox). It is hidden shortly after the focus left text
 * fields (not at once: the focus may move to another field).
 */
class SystemKeyboard : public QObject {
    Q_OBJECT
public:
    // runs the command showing (true) or hiding (false) the keyboard
    using Setter = std::function<void(bool)>;

    // tells whether the on-screen keyboard is wanted now
    using Needed = std::function<bool()>;

    // Installs the busctl driven keyboard on Wayland (or X11) when busctl
    // exists; does nothing otherwise. GUITARIQX_SYSTEM_KEYBOARD=0 disables it, =1
    // shows it even with a physical keyboard.
    static void install(QGuiApplication *app);

    SystemKeyboard(Setter setter, Needed needed, QObject *parent = nullptr);

    // whether the contents of /proc/bus/input/devices list a keyboard
    // having letter keys (not only power buttons, foot switches...)
    static bool hasPhysicalKeyboard(const QByteArray &devices);
    static bool hasPhysicalKeyboard();

    bool visible() const { return m_visible; }

    // time the keyboard stays after the focus left text fields
    static constexpr int hideDelay = 200;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void focusChanged(QObject *focus);
    void show();
    void hide();
    static bool acceptsText(QObject *object);

    Setter m_setter;
    Needed m_needed;
    bool m_visible = false;
    bool m_pressed = false;
    QTimer m_hideTimer;
    QElapsedTimer m_lastShow; // a tap gives a touch and a mouse release
};
