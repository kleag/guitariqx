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

// On-screen keyboard: physical keyboard detection, and when the keyboard is
// shown or hidden, with a recording setter instead of busctl.

#include "systemkeyboard.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

class TestSystemKeyboard : public QObject {
    Q_OBJECT
private slots:
    void physicalKeyboard_data();
    void physicalKeyboard();
    void init();
    void cleanup();
    void focus();
    void tapFocusedField();
    void readOnly();
    void notNeeded();
    void keyboardOnTap();
    void spinButtons();
    void focusByTapElsewhere();
    void fullScreen();

private:
    QQuickItem *item(const char *name) const { return m_root->findChild<QQuickItem *>(name); }
    void tap(QQuickItem *target);

    QQmlEngine *m_engine = nullptr;
    QQuickWindow *m_window = nullptr;
    QQuickItem *m_root = nullptr;
    SystemKeyboard *m_keyboard = nullptr;
    QList<bool> m_calls;
    bool m_needed = true;
};

void TestSystemKeyboard::physicalKeyboard_data() {
    QTest::addColumn<QByteArray>("devices");
    QTest::addColumn<bool>("keyboard");
    const QByteArray power = "N: Name=\"Power Button\"\nH: Handlers=kbd event2\nB: EV=3\n"
                             "B: KEY=8000 10000000000000 0\n\n";
    QTest::newRow("none") << QByteArray() << false;
    QTest::newRow("power button") << power << false;
    QTest::newRow("laptop") << power + "N: Name=\"AT Translated Set 2 keyboard\"\nB: EV=120013\n"
                                       "B: KEY=6000000000000020 0 0 110500f02100007 ff803078f900d401 "
                                       "feffffdfffcfffff fffffffffffffffe\n\n"
                            << true;
    QTest::newRow("32 bit words") << QByteArray("B: KEY=ff9f207a c14057ff febeffdf ffefffff ffffffff fffffffe\n")
                                  << true;
    // a foot switch sending page up/down, a touch screen
    QTest::newRow("foot switch") << QByteArray("B: KEY=1680 0 0 0\n") << false;
    QTest::newRow("touch screen") << QByteArray("B: KEY=400 0 0 0 0 0\n") << false;
}

void TestSystemKeyboard::physicalKeyboard() {
    QFETCH(QByteArray, devices);
    QFETCH(bool, keyboard);
    QCOMPARE(SystemKeyboard::hasPhysicalKeyboard(devices), keyboard);
}

void TestSystemKeyboard::init() {
    m_calls.clear();
    m_needed = true;
    m_engine = new QQmlEngine;
    QQmlComponent component(m_engine);
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls
        Column {
            TextInput { objectName: "a"; width: 200; height: 40; text: "a" }
            TextInput { objectName: "b"; width: 200; height: 40; text: "b" }
            TextInput { objectName: "ro"; width: 200; height: 40; readOnly: true; text: "ro" }
            TextInput { objectName: "onTap"; width: 200; height: 40; text: "t"
                        property bool keyboardOnTap: true }
            Rectangle { objectName: "button"; width: 200; height: 40; activeFocusOnTab: true }
            SpinBox { objectName: "spin"; width: 200; height: 40; editable: true }
            MouseArea { objectName: "focuser"; width: 200; height: 40
                        onPressed: parent.children[1].forceActiveFocus() }
        })", QUrl());
    m_root = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(m_root, qPrintable(component.errorString()));
    m_window = new QQuickWindow;
    m_window->resize(200, 320);
    m_root->setParentItem(m_window->contentItem());
    m_window->show();
    m_window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(m_window));
    m_keyboard = new SystemKeyboard([this](bool visible) { m_calls << visible; },
                                    [this]() { return m_needed; });
}

void TestSystemKeyboard::cleanup() {
    delete m_keyboard;
    delete m_root;
    delete m_window;
    delete m_engine;
}

void TestSystemKeyboard::tap(QQuickItem *target) {
    const QPoint center = target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
    QTest::mouseClick(m_window, Qt::LeftButton, {}, center);
}

void TestSystemKeyboard::focus() {
    item("a")->forceActiveFocus();
    QCOMPARE(m_calls, QList<bool>{true});
    QVERIFY(m_keyboard->visible());
    // to another field: stays shown, without a new call
    item("b")->forceActiveFocus();
    QTest::qWait(SystemKeyboard::hideDelay + 100);
    QCOMPARE(m_calls, QList<bool>{true});
    // away from text fields: hidden after the delay
    item("button")->forceActiveFocus();
    QCOMPARE(m_calls, QList<bool>{true});
    QTRY_COMPARE(m_calls, (QList<bool>{true, false}));
    QVERIFY(!m_keyboard->visible());
}

void TestSystemKeyboard::tapFocusedField() {
    item("a")->forceActiveFocus();
    QCOMPARE(m_calls, QList<bool>{true});
    // closed by hand, then a tap into the field
    QTest::qWait(350);
    tap(item("a"));
    QCOMPARE(m_calls, (QList<bool>{true, true}));
    // a tap elsewhere does not show it
    QTest::qWait(350);
    tap(item("button"));
    QCOMPARE(m_calls, (QList<bool>{true, true}));
}

void TestSystemKeyboard::readOnly() {
    item("ro")->forceActiveFocus();
    QVERIFY(m_calls.isEmpty());
}

void TestSystemKeyboard::notNeeded() {
    // a physical keyboard is plugged in
    m_needed = false;
    item("a")->forceActiveFocus();
    tap(item("a"));
    item("button")->forceActiveFocus();
    QTest::qWait(SystemKeyboard::hideDelay + 100);
    QVERIFY(m_calls.isEmpty());
}

void TestSystemKeyboard::keyboardOnTap() {
    // the search field of the add unit dialog: not when opening
    item("onTap")->forceActiveFocus();
    QVERIFY(m_calls.isEmpty());
    tap(item("onTap"));
    QCOMPARE(m_calls, QList<bool>{true});
}

void TestSystemKeyboard::spinButtons() {
    // a tap on the + of an editable SpinBox (ParamSpin) does not show it
    auto *spin = item("spin");
    auto *up = spin->property("up").value<QObject *>()->property("indicator").value<QQuickItem *>();
    QVERIFY(up);
    tap(up);
    QCOMPARE(spin->property("value").toInt(), 1);
    QVERIFY(m_calls.isEmpty());
    // a tap into its text does show the keyboard
    auto *text = spin->property("contentItem").value<QQuickItem *>();
    tap(text);
    QVERIFY(text->hasActiveFocus());
    QCOMPARE(m_calls, QList<bool>{true});
}

void TestSystemKeyboard::focusByTapElsewhere() {
    // a press out of the field giving it the focus: no request to type
    tap(item("focuser"));
    QVERIFY(item("b")->hasActiveFocus());
    QVERIFY(m_calls.isEmpty());
}

void TestSystemKeyboard::fullScreen() {
    // the keyboard would be behind a full screen window
    m_window->showFullScreen();
    QTRY_COMPARE(m_window->visibility(), QWindow::FullScreen);
    item("a")->forceActiveFocus();
    QCOMPARE(m_calls, QList<bool>{true});
    QTRY_COMPARE(m_window->visibility(), QWindow::Maximized);
    item("button")->forceActiveFocus();
    QTRY_COMPARE(m_calls, (QList<bool>{true, false}));
    QTRY_COMPARE(m_window->visibility(), QWindow::FullScreen);
    // a window that was not full screen stays as it is
    m_window->showNormal();
    QTRY_COMPARE(m_window->visibility(), QWindow::Windowed);
    item("a")->forceActiveFocus();
    QTRY_COMPARE(m_calls, (QList<bool>{true, false, true}));
    item("button")->forceActiveFocus();
    QTRY_COMPARE(m_calls, (QList<bool>{true, false, true, false}));
    QCOMPARE(m_window->visibility(), QWindow::Windowed);
}

QTEST_MAIN(TestSystemKeyboard)
#include "tst_systemkeyboard.moc"
