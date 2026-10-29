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

#include "machine.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <cstdio>

namespace {

// Saves the window size when the window is closed, before it is hidden
// (aboutToQuit comes too late). An event filter rather than a connection
// to QQuickWindow::closing, whose QQuickCloseEvent argument is an
// incomplete type in Qt 6.4.
class WindowSizeSaver : public QObject {
public:
    explicit WindowSizeSaver(QQuickWindow *window) : QObject(window), m_window(window) {
        window->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == m_window && event->type() == QEvent::Close) {
            // a full screen or maximized window keeps the last normal size
            QSettings settings;
            const QWindow::Visibility v = m_window->visibility();
            if (v == QWindow::Windowed) {
                settings.setValue(QStringLiteral("window/size"), m_window->size());
            }
            if (v == QWindow::Windowed || v == QWindow::Maximized) {
                settings.setValue(QStringLiteral("window/maximized"), v == QWindow::Maximized);
            }
        }
        return false;
    }

private:
    QQuickWindow *m_window;
};

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("guitariqx"));
    QGuiApplication::setOrganizationName(QStringLiteral("guitarix"));
    QGuiApplication::setApplicationVersion(QStringLiteral(GUITARIQX_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Qt Quick user interface for guitarix.\n"
        "Start the engine with \"guitarix -N -p PORT\" first."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption hostOpt({QStringLiteral("H"), QStringLiteral("host")},
                               QStringLiteral("Engine host (default: the last one, else localhost)."),
                               QStringLiteral("host"));
    QCommandLineOption portOpt({QStringLiteral("p"), QStringLiteral("port")},
                               QStringLiteral("Engine JSON-RPC port (default: the last one, else 7000)."),
                               QStringLiteral("port"));
    QCommandLineOption fullOpt({QStringLiteral("f"), QStringLiteral("fullscreen")},
                               QStringLiteral("Start full screen (touch screens)."));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                               QStringLiteral("Save a screenshot to FILE and quit (for testing)."),
                               QStringLiteral("file"));
    QCommandLineOption delayOpt(QStringLiteral("screenshot-delay"),
                                QStringLiteral("Delay before the screenshot in ms (default 3000)."),
                                QStringLiteral("ms"), QStringLiteral("3000"));
    QCommandLineOption sizeOpt(QStringLiteral("size"),
                               QStringLiteral("Window size WxH (default: the last one, else 1280x800)."),
                               QStringLiteral("size"));
    QCommandLineOption dumpOpt(QStringLiteral("dump"),
                               QStringLiteral("Print engine state on the terminal (debugging)."));
    QCommandLineOption debugOpt(QStringLiteral("debug-state"),
                                QStringLiteral("Comma separated UI states for testing without instrument or "
                                               "MIDI device: tuner:HZ, switcher[:STATE], learnmode, learn:PARAM, midimap, saveas, files:UNIT (open the file chooser of UNIT), bankload, banksave:BANK."),
                                QStringLiteral("states"));
    parser.addOptions({hostOpt, portOpt, fullOpt, shotOpt, delayOpt, sizeOpt, dumpOpt, debugOpt});
    parser.process(app);

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("Material"));
    }

    Machine machine(nullptr);
    Machine::setInstance(&machine);

    if (parser.isSet(dumpOpt)) {
        QObject::connect(&machine, &Machine::statusChanged, [&machine]() {
            std::printf("status: %s\n", qPrintable(machine.status()));
            std::fflush(stdout);
        });
        QObject::connect(&machine, &Machine::readyChanged, [&machine]() {
            if (machine.ready()) {
                std::printf("parameters: %d, plugins: %lld\n", machine.parameterCount(),
                            static_cast<long long>(machine.plugins().size()));
            }
        });
        QObject::connect(&machine, &Machine::presetChanged, [&machine]() {
            std::printf("preset: %s / %s\n", qPrintable(machine.currentBank()),
                        qPrintable(machine.currentPreset()));
            std::fflush(stdout);
        });
        QObject::connect(&machine, &Machine::rackChanged, [&machine]() {
            std::printf("rack mono: %s\nrack stereo: %s\n",
                        qPrintable(machine.monoRack().join(QStringLiteral(", "))),
                        qPrintable(machine.stereoRack().join(QStringLiteral(", "))));
            std::fflush(stdout);
        });
        QObject::connect(&machine, &Machine::engineStateChanged, [&machine]() {
            std::printf("engine: %s\n", qPrintable(machine.engineState()));
            std::fflush(stdout);
        });
    }

    QQmlApplicationEngine engine;
    // the Guitariqx module (and its qmldir declaring the Theme singleton) is
    // embedded at qrc:/Guitariqx/, which is not a default import path. In the
    // build directory the qmldir next to the binary hides this, but an
    // installed guitariqx would otherwise not find Theme.
    engine.addImportPath(QStringLiteral("qrc:/"));
    // window size, maximization and engine of the last session
    QSettings settings;
    QSize size = settings.value(QStringLiteral("window/size"), QSize(1280, 800)).toSize();
    bool maximized = settings.value(QStringLiteral("window/maximized"), false).toBool();
    if (parser.isSet(sizeOpt)) {
        const QStringList wh = parser.value(sizeOpt).split(QLatin1Char('x'));
        if (wh.size() == 2) {
            size = QSize(wh.at(0).toInt(), wh.at(1).toInt());
        }
        maximized = false;
    }
    if (size.isEmpty()) {
        size = QSize(1280, 800);
    }
    const QStringList debugStates = parser.value(debugOpt).split(QLatin1Char(','), Qt::SkipEmptyParts);
    engine.setInitialProperties({
        {QStringLiteral("startFullScreen"), parser.isSet(fullOpt)},
        {QStringLiteral("startMaximized"), maximized},
        {QStringLiteral("openMidiMap"), debugStates.contains(QStringLiteral("midimap"))},
        {QStringLiteral("width"), size.width()},
        {QStringLiteral("height"), size.height()},
    });
    // QQmlApplicationEngine::loadFromModule needs Qt 6.5
    engine.load(QUrl(QStringLiteral("qrc:/Guitariqx/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    new WindowSizeSaver(window);

    // Machine saves the engine after a successful connection
    const QString host = parser.isSet(hostOpt)
        ? parser.value(hostOpt)
        : settings.value(QStringLiteral("connection/host"), QStringLiteral("localhost")).toString();
    const int port = parser.isSet(portOpt)
        ? parser.value(portOpt).toInt()
        : settings.value(QStringLiteral("connection/port"), 7000).toInt();
    machine.connectTo(host, port);

    if (!debugStates.isEmpty()) {
        QObject::connect(&machine, &Machine::readyChanged, &machine, [&machine, debugStates]() {
            if (!machine.ready()) {
                return;
            }
            for (const QString& st : debugStates) {
                const QString arg = st.section(QLatin1Char(':'), 1);
                if (st.startsWith(QLatin1String("tuner:"))) {
                    // show the tuner with a fixed pitch instead of the engine's
                    machine.param(QStringLiteral("system.show_tuner"))->setValue(true);
                    QTimer::singleShot(500, &machine, [&machine, arg]() {
                        machine.tuner()->setFrequency(arg.toDouble());
                    });
                } else if (st == QLatin1String("switcher") || st.startsWith(QLatin1String("switcher:"))) {
                    // the switcher as the engine shows it
                    machine.tuner()->switcherNotification(QStringLiteral("set_display_state"),
                                                          {arg.isEmpty() ? QStringLiteral("listening") : arg});
                } else if (st == QLatin1String("learnmode")) {
                    machine.midi()->setLearnMode(true);
                } else if (st.startsWith(QLatin1String("learn:"))) {
                    machine.midi()->startLearn(arg);
                }
            }
        });
    }

    if (parser.isSet(shotOpt)) {
        const QString file = parser.value(shotOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), &app, [window, file]() {
            const bool ok = window && window->grabWindow().save(file);
            std::printf("screenshot %s: %s\n", ok ? "saved" : "FAILED", qPrintable(file));
            std::fflush(stdout);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }

    const int ret = app.exec();
    machine.shutdown();
    return ret;
}
