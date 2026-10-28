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

#include "uitree.h"
#include "parameter.h"

#include <QHash>
#include <QVector>

namespace uitree {

namespace {

// UiBuilder::set_next_flags values (gx_plugin.h)
enum { UI_NUM_POSITION_MASK = 0x07, UI_NUM_SHOW_ALWAYS = 0x08, UI_LABEL_INVERSE = 0x02 };

QVariantMap box(const QString& orientation, const QString& label = QString()) {
    return QVariantMap{
        {QStringLiteral("type"), QStringLiteral("box")},
        {QStringLiteral("orientation"), orientation},
        {QStringLiteral("label"), label},
        {QStringLiteral("frame"), false},
        {QStringLiteral("tabs"), false},
        {QStringLiteral("children"), QVariantList()},
    };
}

QVariantMap control(const QString& widget, const QString& id, const QString& label = QString(),
                    const QString& size = QStringLiteral("small")) {
    return QVariantMap{
        {QStringLiteral("type"), QStringLiteral("control")},
        {QStringLiteral("widget"), widget},
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("size"), size},
    };
}

struct OpDef {
    const char *widget;
    const char *size;
    // argument positions of id, label, extra ids; -1: none
    int id, label, sw_type, id2, id3;
};

// create_* calls of UiBuilder and how their arguments map to a control
const QHash<QString, OpDef>& controlOps() {
    static const QHash<QString, OpDef> ops{
        {QStringLiteral("create_master_slider"),      {"slider",   "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_feedback_slider"),    {"slider",   "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_eq_rackslider_no_caption"), {"slider", "eq", 1, -1, -1, -1, -1}},
        {QStringLiteral("create_small_rackknob"),     {"knob",     "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_small_rackknobr"),    {"knob",     "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_mid_rackknob"),       {"knob",     "mid",   1, 2, -1, -1, -1}},
        {QStringLiteral("create_big_rackknob"),       {"knob",     "big",   1, 2, -1, -1, -1}},
        {QStringLiteral("create_wheel"),              {"wheel",    "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_selector_no_caption"),{"selector", "small", 1, -1, -1, -1, -1}},
        {QStringLiteral("create_selector"),           {"selector", "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_switch_no_caption"),  {"switch",   "small", 2, -1, 1, -1, -1}},
        {QStringLiteral("create_feedback_switch"),    {"switch",   "small", 2, -1, 1, -1, -1}},
        {QStringLiteral("create_switch"),             {"switch",   "small", 2, 3, 1, -1, -1}},
        // (sw_type, id, file_id): id is unused, the button chooses a file
        // for the string parameter file_id
        {QStringLiteral("create_fload_switch"),       {"file",     "small", 3, -1, 1, -1, -1}},
        {QStringLiteral("create_spin_value"),         {"spin",     "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_simple_spin_value"),  {"spin",     "small", 1, -1, -1, -1, -1}},
        {QStringLiteral("create_port_display"),       {"display",  "small", 1, 2, -1, -1, -1}},
        {QStringLiteral("create_p_display"),          {"display",  "small", 1, -1, -1, 2, 3}},
        {QStringLiteral("create_simple_meter"),       {"meter",    "small", 1, -1, -1, -1, -1}},
        {QStringLiteral("create_simple_c_meter"),     {"meter",    "small", 1, 3, -1, 2, -1}},
    };
    return ops;
}

QString arg(const QJsonArray& op, int i) {
    return (i > 0 && i < op.size()) ? op.at(i).toString() : QString();
}

// Boxes are built bottom-up: QVariantMap is a value type, so the open box
// is only appended to its parent when it is closed.
class Builder {
public:
    Builder() { m_stack.append(box(QStringLiteral("v"))); }

    void open(QVariantMap b) { m_stack.append(std::move(b)); }

    void close() {
        if (m_stack.size() <= 1) {
            return;
        }
        QVariantMap b = m_stack.takeLast();
        add(b);
    }

    void add(const QVariantMap& node) {
        QVariantMap& top = m_stack.last();
        QVariantList children = top.value(QStringLiteral("children")).toList();
        children.append(node);
        top.insert(QStringLiteral("children"), children);
    }

    QVariantMap finish() {
        while (m_stack.size() > 1) {
            close();
        }
        return m_stack.first();
    }

    int depth() const { return m_stack.size() - 1; }

private:
    QVector<QVariantMap> m_stack;
};

// a single child box is replaced by the child
QVariantMap simplify(const QVariantMap& node) {
    if (node.value(QStringLiteral("type")) != QLatin1String("box")) {
        return node;
    }
    QVariantList children;
    for (const QVariant& c : node.value(QStringLiteral("children")).toList()) {
        QVariantMap child = simplify(c.toMap());
        // layouts use empty frames as decoration
        if (child.value(QStringLiteral("type")) == QLatin1String("box")
            && child.value(QStringLiteral("children")).toList().isEmpty()) {
            continue;
        }
        children.append(child);
    }
    QVariantMap out = node;
    out.insert(QStringLiteral("children"), children);
    if (children.size() == 1 && !node.value(QStringLiteral("frame")).toBool()
        && !node.value(QStringLiteral("tabs")).toBool()
        && node.value(QStringLiteral("label")).toString().isEmpty()
        && children.first().toMap().value(QStringLiteral("type")) == QLatin1String("box")) {
        return children.first().toMap();
    }
    return out;
}

} // anonymous namespace

QVariantMap fromStackOps(const QJsonArray& ops) {
    Builder main;
    Builder mini;
    Builder *cur = &main;
    int nextFlags = 0;
    bool miniSeen = false;

    for (const QJsonValue& v : ops) {
        const QJsonArray op = v.toArray();
        const QString name = op.at(0).toString();
        if (name == QLatin1String("openHorizontalhideBox")) {
            // the first hide box at top level is the compact view
            if (!miniSeen && cur == &main && main.depth() == 0) {
                miniSeen = true;
                cur = &mini;
            }
            cur->open(box(QStringLiteral("h"), arg(op, 1)));
        } else if (name == QLatin1String("openHorizontalBox")
                   || name == QLatin1String("openHorizontalTableBox")) {
            cur->open(box(QStringLiteral("h"), arg(op, 1)));
        } else if (name == QLatin1String("openVerticalBox")
                   || name == QLatin1String("openVerticalBox1")
                   || name == QLatin1String("openVerticalBox2")
                   || name == QLatin1String("openFlipLabelBox")) {
            cur->open(box(QStringLiteral("v"), arg(op, 1)));
        } else if (name == QLatin1String("openFrameBox")
                   || name == QLatin1String("openpaintampBox")) {
            QVariantMap b = box(QStringLiteral("h"), arg(op, 1));
            b.insert(QStringLiteral("frame"), true);
            cur->open(b);
        } else if (name == QLatin1String("openTabBox")) {
            QVariantMap b = box(QStringLiteral("v"), arg(op, 1));
            b.insert(QStringLiteral("tabs"), true);
            cur->open(b);
        } else if (name == QLatin1String("closeBox")) {
            cur->close();
            if (cur == &mini && mini.depth() == 0) {
                cur = &main;
            }
        } else if (name == QLatin1String("insertSpacer")) {
            cur->add(QVariantMap{{QStringLiteral("type"), QStringLiteral("spacer")}});
        } else if (name == QLatin1String("set_next_flags")) {
            nextFlags = op.at(1).toInt();
        } else if (name == QLatin1String("load_glade")) {
            // Glade layouts are not handled yet, the caller falls back
            // to the generic layout
            continue;
        } else {
            auto it = controlOps().constFind(name);
            if (it == controlOps().constEnd()) {
                continue;
            }
            const OpDef& d = it.value();
            QVariantMap c = control(QString::fromLatin1(d.widget), arg(op, d.id), arg(op, d.label),
                                    QString::fromLatin1(d.size));
            if (d.sw_type > 0) {
                c.insert(QStringLiteral("switchType"), arg(op, d.sw_type));
            }
            if (d.id2 > 0) {
                c.insert(QStringLiteral("id2"), arg(op, d.id2));
            }
            if (d.id3 > 0) {
                c.insert(QStringLiteral("id3"), arg(op, d.id3));
            }
            if (nextFlags) {
                c.insert(QStringLiteral("labelInverse"), bool(nextFlags & UI_LABEL_INVERSE));
                c.insert(QStringLiteral("numPosition"), nextFlags & UI_NUM_POSITION_MASK);
                c.insert(QStringLiteral("showValue"), bool(nextFlags & UI_NUM_SHOW_ALWAYS));
            }
            nextFlags = 0;
            cur->add(c);
        }
    }
    return QVariantMap{
        {QStringLiteral("mini"), simplify(mini.finish())},
        {QStringLiteral("main"), simplify(main.finish())},
    };
}

QVariantMap generic(const QList<Parameter*>& params) {
    QVariantMap root = box(QStringLiteral("flow"));
    QVariantList children;
    QVariantMap mini = box(QStringLiteral("h"));
    QVariantList miniChildren;
    for (Parameter *p : params) {
        const QString id = p->id();
        if (p->isOutput() || !p->isControllable()
            || id.endsWith(QLatin1String(".on_off")) || id.endsWith(QLatin1String(".position"))
            || id.endsWith(QLatin1String(".pp"))) {
            continue;
        }
        QString widget;
        switch (p->kind()) {
        case Parameter::Float:
        case Parameter::Int:
            widget = QStringLiteral("knob");
            break;
        case Parameter::Enum:
            widget = QStringLiteral("selector");
            break;
        case Parameter::Bool:
            widget = QStringLiteral("switch");
            break;
        default:
            continue;
        }
        children.append(control(widget, id, p->name()));
        if (miniChildren.isEmpty() && widget == QLatin1String("knob")) {
            QVariantMap s = control(QStringLiteral("slider"), id, p->name());
            miniChildren.append(s);
        }
    }
    root.insert(QStringLiteral("children"), children);
    mini.insert(QStringLiteral("children"), miniChildren);
    return QVariantMap{
        {QStringLiteral("mini"), mini},
        {QStringLiteral("main"), root},
    };
}

bool hasControls(const QVariantMap& node) {
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("control")) {
        return true;
    }
    for (const QVariant& c : node.value(QStringLiteral("children")).toList()) {
        if (hasControls(c.toMap())) {
            return true;
        }
    }
    return false;
}

namespace {

QVariantMap fixNode(const QVariantMap& node, const std::function<QString(const QString&)>& fix) {
    QVariantMap out = node;
    if (node.value(QStringLiteral("type")) == QLatin1String("control")) {
        out.insert(QStringLiteral("id"), fix(node.value(QStringLiteral("id")).toString()));
        return out;
    }
    QVariantList children;
    for (const QVariant& c : node.value(QStringLiteral("children")).toList()) {
        children.append(fixNode(c.toMap(), fix));
    }
    out.insert(QStringLiteral("children"), children);
    return out;
}

} // anonymous namespace

void fixIds(QVariantMap& ui, const std::function<QString(const QString&)>& fix) {
    for (const QString& view : {QStringLiteral("mini"), QStringLiteral("main")}) {
        ui.insert(view, fixNode(ui.value(view).toMap(), fix));
    }
}

bool hasWidget(const QVariantMap& node, const QString& widget) {
    if (node.value(QStringLiteral("type")) == QLatin1String("control")) {
        return node.value(QStringLiteral("widget")).toString() == widget;
    }
    for (const QVariant& c : node.value(QStringLiteral("children")).toList()) {
        if (hasWidget(c.toMap(), widget)) {
            return true;
        }
    }
    return false;
}

void prependControl(QVariantMap& ui, const QString& widget, const QString& id) {
    QVariantMap root = box(QStringLiteral("v"));
    root.insert(QStringLiteral("children"), QVariantList{
        control(widget, id),
        ui.value(QStringLiteral("main")).toMap(),
    });
    ui.insert(QStringLiteral("main"), root);
}

} // namespace uitree
