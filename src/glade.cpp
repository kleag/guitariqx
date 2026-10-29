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

// Rack unit layouts from the Glade files of gx_head (src/gx_head/builder,
// src/faust/*_ui.glade, src/plugins/*_ui.glade), translated to the layout
// tree of uitree.h.
//
// gx_head loads them with GxBuilder (gx_ui_builder.cpp): the parameter of
// a gxw widget is its var_id property, the labels named by label_ref get
// the parameter name, and some signal handlers connect special widgets
// (StackBoxBuilder::connect_signals in gx_stackbox_builder.cpp).

#include "uitree.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <algorithm>
#include <memory>

namespace uitree {

namespace {

struct GNode {
    QString cls;
    QString id;
    QHash<QString, QString> props;
    QHash<QString, QString> packing;
    QStringList handlers;
    // "tab", "label" (frame label) or empty
    QString childType;
    std::vector<std::unique_ptr<GNode>> children;

    QString prop(const char *name, const QString& def = QString()) const {
        // glade writes both var_id and var-id
        QString n = QString::fromLatin1(name);
        auto it = props.constFind(n);
        if (it == props.constEnd()) {
            it = props.constFind(QString(n).replace(QLatin1Char('_'), QLatin1Char('-')));
        }
        return it == props.constEnd() ? def : it.value();
    }
    QString pack(const char *name, const QString& def = QString()) const {
        QString n = QString::fromLatin1(name);
        auto it = packing.constFind(n);
        if (it == packing.constEnd()) {
            it = packing.constFind(QString(n).replace(QLatin1Char('_'), QLatin1Char('-')));
        }
        return it == packing.constEnd() ? def : it.value();
    }
    bool visible() const {
        return prop("visible").compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    }
};

// <object> ... </object>; the reader is on the start element
std::unique_ptr<GNode> readObject(QXmlStreamReader& xml) {
    auto node = std::make_unique<GNode>();
    node->cls = xml.attributes().value(QLatin1String("class")).toString();
    node->id = xml.attributes().value(QLatin1String("id")).toString();
    while (xml.readNextStartElement()) {
        const auto name = xml.name();
        if (name == QLatin1String("property")) {
            const QString pn = xml.attributes().value(QLatin1String("name")).toString();
            node->props.insert(pn, xml.readElementText().trimmed());
        } else if (name == QLatin1String("signal")) {
            node->handlers.append(xml.attributes().value(QLatin1String("handler")).toString());
            xml.skipCurrentElement();
        } else if (name == QLatin1String("child")) {
            const QString type = xml.attributes().value(QLatin1String("type")).toString();
            std::unique_ptr<GNode> child;
            QHash<QString, QString> packing;
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("object")) {
                    child = readObject(xml);
                } else if (xml.name() == QLatin1String("packing")) {
                    while (xml.readNextStartElement()) {
                        if (xml.name() == QLatin1String("property")) {
                            const QString pn = xml.attributes().value(QLatin1String("name")).toString();
                            packing.insert(pn, xml.readElementText().trimmed());
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
            if (child) {
                child->childType = type;
                child->packing = packing;
                node->children.push_back(std::move(child));
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    return node;
}

const GNode *findId(const GNode& node, const QString& id) {
    if (node.id == id) {
        return &node;
    }
    for (const auto& c : node.children) {
        if (const GNode *f = findId(*c, id)) {
            return f;
        }
    }
    return nullptr;
}

void collectLabelRefs(const GNode& node, QSet<QString>& refs) {
    const QString ref = node.prop("label_ref");
    if (!ref.isEmpty()) {
        refs.insert(ref);
    }
    for (const auto& c : node.children) {
        collectLabelRefs(*c, refs);
    }
}

QVariantMap makeBox(const QString& orientation, const QString& label = QString()) {
    return QVariantMap{
        {QStringLiteral("type"), QStringLiteral("box")},
        {QStringLiteral("orientation"), orientation},
        {QStringLiteral("label"), label},
        {QStringLiteral("frame"), false},
        {QStringLiteral("tabs"), false},
        {QStringLiteral("children"), QVariantList()},
    };
}

QVariantMap makeControl(const QString& widget, const QString& id, const QString& size = QStringLiteral("small")) {
    return QVariantMap{
        {QStringLiteral("type"), QStringLiteral("control")},
        {QStringLiteral("widget"), widget},
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), QString()},
        {QStringLiteral("size"), size},
    };
}

QString stripMarkup(QString s) {
    s.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return s.trimmed();
}

class Translator {
public:
    explicit Translator(const GNode& root) { collectLabelRefs(root, m_labelRefs); }

    // empty map: nothing to show
    QVariantMap node(const GNode& n) {
        if (!n.visible()) {
            return QVariantMap();
        }
        const QString& cls = n.cls;
        if (QVariantMap c = special(n); !c.isEmpty()) {
            return c;
        }
        if (cls == QLatin1String("GtkBox") || cls == QLatin1String("GxPaintBox")
            || cls == QLatin1String("GtkButtonBox")) {
            return box(n, n.prop("orientation") == QLatin1String("vertical") ? QStringLiteral("v")
                                                                                : QStringLiteral("h"));
        }
        if (cls == QLatin1String("GtkGrid")) {
            return grid(n);
        }
        if (cls == QLatin1String("GtkAlignment") || cls == QLatin1String("GtkEventBox")
            || cls == QLatin1String("GtkViewport") || cls == QLatin1String("GtkScrolledWindow")) {
            // single child containers
            return box(n, QStringLiteral("v"));
        }
        if (cls == QLatin1String("GtkFrame")) {
            QString label;
            QVariantMap b = makeBox(QStringLiteral("v"));
            QVariantList children;
            for (const auto& c : n.children) {
                if (c->childType == QLatin1String("label")) {
                    label = stripMarkup(c->prop("label"));
                    continue;
                }
                QVariantMap cn = node(*c);
                if (!cn.isEmpty()) {
                    children.append(cn);
                }
            }
            b.insert(QStringLiteral("label"), label);
            b.insert(QStringLiteral("frame"), true);
            b.insert(QStringLiteral("children"), children);
            return b;
        }
        if (cls == QLatin1String("GtkNotebook")) {
            return notebook(n);
        }
        if (cls == QLatin1String("GtkLabel")) {
            // the labels of gxw widgets show the parameter name: the
            // control shows it itself
            if (m_labelRefs.contains(n.id)) {
                return QVariantMap();
            }
            const QString text = stripMarkup(n.prop("label"));
            if (text.isEmpty()) {
                return QVariantMap();
            }
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), text}};
        }
        return widget(n);
    }

private:
    // gxw widgets and other leaves; a gxw widget shows the parameter name
    // only through its label_ref label
    QVariantMap widget(const GNode& n) {
        QVariantMap c = leaf(n);
        if (!c.isEmpty() && n.prop("label_ref").isEmpty() && c.value(QStringLiteral("label")).toString().isEmpty()) {
            if (c.value(QStringLiteral("widget")) == QLatin1String("switch")) {
                // gx_head draws these as icons (record, play, ...): name
                // them instead
                c.insert(QStringLiteral("nameLabel"), true);
            } else {
                c.insert(QStringLiteral("noLabel"), true);
            }
        }
        return c;
    }

    QVariantMap leaf(const GNode& n) {
        const QString& cls = n.cls;
        const QString var = n.prop("var_id");
        if (cls == QLatin1String("GxSmallKnob") || cls == QLatin1String("GxSmallKnobR")
            || cls == QLatin1String("GxKnob")) {
            if (var.isEmpty()) {
                return QVariantMap();
            }
            QVariantMap c = makeControl(QStringLiteral("knob"), var);
            if (cls == QLatin1String("GxSmallKnob")) {
                // the smaller knob image of skins (default: smallknobr)
                c.insert(QStringLiteral("knob"), QStringLiteral("smallknob"));
            }
            return c;
        }
        if (cls == QLatin1String("GxMidKnob")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("knob"), var, QStringLiteral("mid"));
        }
        if (cls == QLatin1String("GxBigKnob")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("knob"), var, QStringLiteral("big"));
        }
        if (cls == QLatin1String("GxHSlider") || cls == QLatin1String("GxMiniSlider")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("slider"), var);
        }
        if (cls == QLatin1String("GxVSlider") || cls == QLatin1String("GxEQSlider")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("slider"), var, QStringLiteral("eq"));
        }
        if (cls == QLatin1String("GxSwitch") || cls == QLatin1String("GxToggleImage")) {
            if (var.isEmpty()) {
                return QVariantMap();
            }
            QVariantMap c = makeControl(QStringLiteral("switch"), var);
            c.insert(QStringLiteral("label"), n.prop("label"));
            c.insert(QStringLiteral("switchType"), n.prop("base_name"));
            return c;
        }
        if (cls == QLatin1String("GxSelector") || cls == QLatin1String("GxComboBox")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("selector"), var);
        }
        if (cls == QLatin1String("GxFastMeter") || cls == QLatin1String("GxLevelSlider")) {
            if (var.isEmpty()) {
                return QVariantMap();
            }
            QVariantMap c = makeControl(QStringLiteral("meter"), var);
            c.insert(QStringLiteral("horizontal"), n.prop("orientation") == QLatin1String("horizontal"));
            return c;
        }
        if (cls == QLatin1String("GxValueDisplay") || cls == QLatin1String("GxSimpleValueDisplay")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("spin"), var);
        }
        if (cls == QLatin1String("GxPortDisplay")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("display"), var);
        }
        if (cls == QLatin1String("GxWheel")) {
            return var.isEmpty() ? QVariantMap() : makeControl(QStringLiteral("wheel"), var);
        }
        // GtkButton without known handler, GtkDrawingArea ...
        return QVariantMap();
    }

    // widgets connected by signal handlers (see connect_signals in
    // gx_stackbox_builder.cpp)
    QVariantMap special(const GNode& n) {
        for (const QString& h : n.handlers) {
            const QStringList t = h.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (t.isEmpty()) {
                continue;
            }
            const QString& tok = t.first();
            // file buttons: "nam:load_nam_file nam.loadfile",
            // "dubber:load_loop_file dubber.loadfile1", "rtneural:load_json_file ..."
            if ((tok.endsWith(QLatin1String(":load_nam_file")) || tok.endsWith(QLatin1String(":load_loop_file"))
                 || tok.endsWith(QLatin1String(":load_json_file"))) && t.size() > 1) {
                return file(t.at(1));
            }
            // model list selectors: "nam.filelist", "mnam.afilelist"
            for (const char *ab : {"", "a", "b"}) {
                const QString x = QString::fromLatin1(ab);
                const QString suffix = QStringLiteral(".%1filelist").arg(x);
                if (tok.endsWith(suffix)) {
                    return file(tok.left(tok.size() - suffix.size()) + QStringLiteral(".load%1file").arg(x));
                }
            }
            // "jconv.convolver:IRFile": label with the IR file name
            if (tok.endsWith(QLatin1String(".convolver:IRFile"))) {
                return file(tok.section(QLatin1Char(':'), 0, 0), QStringLiteral("ir"));
            }
            // portdisplay:clip <cut> <clip> on the play head display of a
            // looper tape: the part of the tape played, in percent
            // (create_p_display of the stack layouts)
            if (tok == QLatin1String("portdisplay:clip") && t.size() > 2) {
                QVariantMap c = makeControl(QStringLiteral("display"), n.prop("var_id"));
                c.insert(QStringLiteral("id2"), t.at(1));
                c.insert(QStringLiteral("id3"), t.at(2));
                return c;
            }
            // buttons opening the IR editor and the drum sequencer windows
            // of gx_head (IRWindow::create is given these units)
            if (tok == QLatin1String("ir_mono_window_reload_and_show")
                || tok == QLatin1String("ir_stereo_window_reload_and_show")) {
                const bool mono = tok.startsWith(QLatin1String("ir_mono"));
                QVariantMap c = makeControl(QStringLiteral("iredit"),
                                            mono ? QStringLiteral("jconv_mono.convolver") : QStringLiteral("jconv.convolver"));
                c.insert(QStringLiteral("label"), n.prop("label"));
                return c;
            }
            if (tok == QLatin1String("SEQWindow_reload_and_show")) {
                QVariantMap c = makeControl(QStringLiteral("sequencer"), QStringLiteral("seq.sequencer.kick"));
                c.insert(QStringLiteral("label"), n.prop("label"));
                return c;
            }
            if (tok == QLatin1String("connect_oscilloscope")) {
                return makeControl(QStringLiteral("oscilloscope"), QStringLiteral("oscilloscope.info"));
            }
            if (tok.endsWith(QLatin1String("_reload_and_show"))) {
                return QVariantMap{{QStringLiteral("type"), QStringLiteral("skip")}};
            }
        }
        return QVariantMap();
    }

    QVariantMap file(const QString& id, const QString& widget = QStringLiteral("file")) {
        // a file button and a model list selector often come together
        if (m_files.contains(id)) {
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("skip")}};
        }
        m_files.insert(id);
        return makeControl(widget, id);
    }

    // children sorted like GtkBox packs them: start children by position,
    // then the end children in reverse order
    std::vector<const GNode*> packed(const GNode& n) {
        std::vector<const GNode*> start, end;
        for (const auto& c : n.children) {
            if (!c->childType.isEmpty()) {
                continue;
            }
            (c->pack("pack_type") == QLatin1String("end") ? end : start).push_back(c.get());
        }
        auto pos = [](const GNode *a, const GNode *b) {
            return a->pack("position", QStringLiteral("0")).toInt() < b->pack("position", QStringLiteral("0")).toInt();
        };
        std::stable_sort(start.begin(), start.end(), pos);
        std::stable_sort(end.begin(), end.end(), pos);
        start.insert(start.end(), end.rbegin(), end.rend());
        return start;
    }

    void append(QVariantList& children, QVariantMap cn) {
        if (cn.isEmpty() || cn.value(QStringLiteral("type")) == QLatin1String("skip")) {
            return;
        }
        children.append(cn);
    }

    QVariantMap box(const GNode& n, const QString& orientation) {
        QVariantMap b = makeBox(orientation);
        QVariantList children;
        for (const GNode *c : packed(n)) {
            append(children, node(*c));
        }
        b.insert(QStringLiteral("children"), children);
        return b;
    }

    QVariantMap grid(const GNode& n) {
        QVariantMap b = makeBox(QStringLiteral("grid"));
        QVariantList children;
        for (const auto& c : n.children) {
            QVariantMap cn = node(*c);
            if (cn.isEmpty() || cn.value(QStringLiteral("type")) == QLatin1String("skip")) {
                continue;
            }
            cn.insert(QStringLiteral("column"), c->pack("left_attach", QStringLiteral("0")).toInt());
            cn.insert(QStringLiteral("row"), c->pack("top_attach", QStringLiteral("0")).toInt());
            cn.insert(QStringLiteral("columnSpan"), std::max(1, c->pack("width", QStringLiteral("1")).toInt()));
            cn.insert(QStringLiteral("rowSpan"), std::max(1, c->pack("height", QStringLiteral("1")).toInt()));
            // GtkWidget halign/valign: start, end, center (default fill)
            for (const char *a : {"halign", "valign"}) {
                const QString v = c->prop(a);
                if (v == QLatin1String("start") || v == QLatin1String("end")) {
                    cn.insert(QLatin1String(a), v);
                }
            }
            children.append(cn);
        }
        b.insert(QStringLiteral("children"), children);
        return b;
    }

    // pages, each followed by its tab label child
    QVariantMap notebook(const GNode& n) {
        QVariantMap b = makeBox(QStringLiteral("v"));
        b.insert(QStringLiteral("tabs"), true);
        QVariantList pages;
        for (const auto& c : n.children) {
            if (c->childType == QLatin1String("tab")) {
                if (!pages.isEmpty()) {
                    QVariantMap last = pages.takeLast().toMap();
                    last.insert(QStringLiteral("label"), stripMarkup(c->prop("label")));
                    pages.append(last);
                }
                continue;
            }
            QVariantMap page = node(*c);
            if (page.isEmpty() || page.value(QStringLiteral("type")) == QLatin1String("skip")) {
                // keep the page count in step with the tab labels
                page = makeBox(QStringLiteral("v"));
            }
            if (page.value(QStringLiteral("type")) != QLatin1String("box")) {
                QVariantMap wrap = makeBox(QStringLiteral("v"));
                wrap.insert(QStringLiteral("children"), QVariantList{page});
                page = wrap;
            }
            pages.append(page);
        }
        b.insert(QStringLiteral("children"), pages);
        return b;
    }

    QSet<QString> m_labelRefs;
    QSet<QString> m_files;
};

} // anonymous namespace

QVariantMap fromGlade(const QString& text) {
    QXmlStreamReader xml(text);
    std::unique_ptr<GNode> root = std::make_unique<GNode>();
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("interface")) {
        return QVariantMap();
    }
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("object")) {
            root->children.push_back(readObject(xml));
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        return QVariantMap();
    }
    const GNode *rack = findId(*root, QStringLiteral("rackbox"));
    if (!rack) {
        return QVariantMap();
    }
    Translator tr(*root);
    QVariantMap ui;
    QVariantMap main = tr.node(*rack);
    if (main.value(QStringLiteral("type")) != QLatin1String("box")) {
        QVariantMap wrap = makeBox(QStringLiteral("v"));
        wrap.insert(QStringLiteral("children"), main.isEmpty() ? QVariantList() : QVariantList{main});
        main = wrap;
    }
    ui.insert(QStringLiteral("main"), main);
    QVariantMap mini = makeBox(QStringLiteral("h"));
    if (const GNode *m = findId(*root, QStringLiteral("minibox"))) {
        QVariantMap mn = tr.node(*m);
        if (mn.value(QStringLiteral("type")) == QLatin1String("box")) {
            mini = mn;
        }
    }
    ui.insert(QStringLiteral("mini"), mini);
    return ui;
}

} // namespace uitree
