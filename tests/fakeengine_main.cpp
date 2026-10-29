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

// FakeEngine as a program: runs guitariqx without guitarix, e.g. for
// screenshots of the documentation.

#include "fakeengine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <cstdio>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("JSON-RPC server answering like a recorded guitarix engine."));
    parser.addHelpOption();
    QCommandLineOption portOpt({QStringLiteral("p"), QStringLiteral("port")},
                               QStringLiteral("Port (default 7124)."), QStringLiteral("port"), QStringLiteral("7124"));
    QCommandLineOption minorOpt(QStringLiteral("interface-minor"),
                                QStringLiteral("JSON-RPC interface minor version (default 2)."),
                                QStringLiteral("minor"), QStringLiteral("2"));
    parser.addOptions({portOpt, minorOpt});
    parser.process(app);

    FakeEngine engine(parser.value(minorOpt).toInt());
    if (!engine.listen(quint16(parser.value(portOpt).toInt()))) {
        std::fprintf(stderr, "cannot listen on port %s\n", qPrintable(parser.value(portOpt)));
        return 1;
    }
    std::printf("fake engine on port %d\n", engine.port());
    std::fflush(stdout);
    return app.exec();
}
