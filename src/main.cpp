/*
 * Copyright (C) 2026 Guitarix developers
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
#include <QTimer>
#include <cstdio>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("qt_head"));
    QGuiApplication::setOrganizationName(QStringLiteral("guitarix"));
    QGuiApplication::setApplicationVersion(QStringLiteral(QT_HEAD_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Qt Quick user interface for guitarix.\n"
        "Start the engine with \"guitarix -N -p PORT\" first."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption hostOpt({QStringLiteral("H"), QStringLiteral("host")},
                               QStringLiteral("Engine host (default localhost)."),
                               QStringLiteral("host"), QStringLiteral("localhost"));
    QCommandLineOption portOpt({QStringLiteral("p"), QStringLiteral("port")},
                               QStringLiteral("Engine JSON-RPC port (default 7000)."),
                               QStringLiteral("port"), QStringLiteral("7000"));
    QCommandLineOption fullOpt({QStringLiteral("f"), QStringLiteral("fullscreen")},
                               QStringLiteral("Start full screen (touch screens)."));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                               QStringLiteral("Save a screenshot to FILE and quit (for testing)."),
                               QStringLiteral("file"));
    QCommandLineOption delayOpt(QStringLiteral("screenshot-delay"),
                                QStringLiteral("Delay before the screenshot in ms (default 3000)."),
                                QStringLiteral("ms"), QStringLiteral("3000"));
    QCommandLineOption sizeOpt(QStringLiteral("size"),
                               QStringLiteral("Window size WxH (default 1280x800)."),
                               QStringLiteral("size"), QStringLiteral("1280x800"));
    QCommandLineOption dumpOpt(QStringLiteral("dump"),
                               QStringLiteral("Print engine state on the terminal (debugging)."));
    parser.addOptions({hostOpt, portOpt, fullOpt, shotOpt, delayOpt, sizeOpt, dumpOpt});
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
    QSize size(1280, 800);
    const QStringList wh = parser.value(sizeOpt).split(QLatin1Char('x'));
    if (wh.size() == 2) {
        size = QSize(wh.at(0).toInt(), wh.at(1).toInt());
    }
    engine.setInitialProperties({
        {QStringLiteral("startFullScreen"), parser.isSet(fullOpt)},
        {QStringLiteral("width"), size.width()},
        {QStringLiteral("height"), size.height()},
    });
    // QQmlApplicationEngine::loadFromModule needs Qt 6.5
    engine.load(QUrl(QStringLiteral("qrc:/QtHead/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    machine.connectTo(parser.value(hostOpt), parser.value(portOpt).toInt());

    if (parser.isSet(shotOpt)) {
        auto *window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        const QString file = parser.value(shotOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), &app, [window, file]() {
            const bool ok = window && window->grabWindow().save(file);
            std::printf("screenshot %s: %s\n", ok ? "saved" : "FAILED", qPrintable(file));
            std::fflush(stdout);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }

    return app.exec();
}
