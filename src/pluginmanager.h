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

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>

class Machine;
class RpcClient;

/*
 * The LADSPA and LV2 plugins the engine finds on its computer, and which of
 * them it offers as rack units: gx_head's "LADSPA/LV2 plugins" window
 * (ladspa::PluginDisplay in src/gx_head/gui/ladspalist.cpp).
 *
 * load_ladspalist answers with every plugin description (ladspa::PluginDesc
 * serialized as JSON, several MB with all control ports). The list is kept
 * as the engine sent it and only the edited members change: save_ladspalist
 * takes the whole list back, writes the definitions of the active plugins
 * and removes the settings files of all the others that have no settings,
 * so it must never get a partial list. ladspaloader_update_plugins then
 * makes the engine load the new definitions (Machine::reloadPlugins()).
 *
 * The model has the plugins shown by the filter (text, view, channels);
 * the methods take the "index" role, an index in the whole list.
 */
class PluginManager : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Gx.pluginManager")
    // waiting for the list or for the engine to load the plugins
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    // edits not applied yet
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(int total READ total NOTIFY loadedChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY modifiedChanged)
    // plugins of the previous list that are not installed any more
    Q_PROPERTY(QStringList notFound READ notFound NOTIFY loadedChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int view READ view WRITE setView NOTIFY filterChanged)
    Q_PROPERTY(int channels READ channels WRITE setChannels NOTIFY filterChanged)
    // rack unit categories a plugin can be put in
    Q_PROPERTY(QStringList categories READ categories CONSTANT)
public:
    enum View { AllPlugins, ActivePlugins, ChangedPlugins, LadspaPlugins, Lv2Plugins };
    Q_ENUM(View)
    enum Channels { AnyChannels, MonoPlugins, StereoPlugins };
    Q_ENUM(Channels)
    // PluginDesc::quirks (gx_engine::quirkflag)
    enum Quirk { NeedActivate = 1, NoCleanup = 2 };
    Q_ENUM(Quirk)
    enum Role {
        IndexRole = Qt::UserRole + 1, NameRole, ShortnameRole, MakerRole, Lv2Role, StereoRole,
        ActiveRole, CategoryRole, HasSettingsRole, ChangedRole,
    };

    PluginManager(Machine& machine, RpcClient& rpc);

    bool busy() const { return m_busy; }
    bool loaded() const { return m_loaded; }
    bool modified() const { return !m_changed.isEmpty(); }
    int total() const { return int(m_plugins.size()); }
    int activeCount() const;
    QStringList notFound() const { return m_notFound; }
    QString filter() const { return m_filter; }
    void setFilter(const QString& f);
    int view() const { return m_view; }
    void setView(int v);
    int channels() const { return m_channels; }
    void setChannels(int c);
    QStringList categories() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // asks the engine for the list (it scans the plugin folders)
    Q_INVOKABLE void load();
    // drops the list and the edits
    Q_INVOKABLE void unload();
    Q_INVOKABLE void setActive(int index, bool active);
    // all the plugins shown
    Q_INVOKABLE void setAllActive(bool active);
    // the settings of a plugin: name, shortname, maker, lv2, stereo,
    // source (LV2 URI or LADSPA id, file and index), ladspaCategory,
    // category, deducedCategory, quirks, quirksDefault, masterIdx,
    // masterLabel, ports ([{pos, name}] of the controls), addWetDry,
    // stereoToMono, active, hasSettings
    Q_INVOKABLE QVariantMap plugin(int index) const;
    // changes the members given among shortname, category, quirks,
    // masterIdx, masterLabel, addWetDry, stereoToMono
    Q_INVOKABLE void setPlugin(int index, const QVariantMap& settings);
    // the factory settings (PluginDesc::reset)
    Q_INVOKABLE void resetPlugin(int index);
    Q_INVOKABLE void discard();
    // saves the list and makes the engine load the plugins: applied()
    Q_INVOKABLE void apply();

    // for the tests: the plugin description as the engine sent it
    QJsonObject description(int index) const { return m_plugins.at(index).toObject(); }

signals:
    void busyChanged();
    void loadedChanged();
    void modifiedChanged();
    void filterChanged();
    void pluginChanged(int index);
    void applied();
    // the engine sent no list
    void loadFailed();

private:
    bool accepts(const QJsonObject& p) const;
    void refilter();
    void setBusy(bool b);
    void update(int index, const QJsonObject& p);
    void commit(std::function<void()> done);
    static bool computeHasSettings(const QJsonObject& p);

    Machine& m_machine;
    RpcClient& m_rpc;
    // as received, then edited
    QJsonArray m_plugins;
    // as loaded or last applied
    QJsonArray m_saved;
    // indexes in m_plugins differing from m_saved
    QSet<int> m_changed;
    // indexes shown, in list order
    QList<int> m_rows;
    QStringList m_notFound;
    QString m_filter;
    int m_view = AllPlugins;
    int m_channels = AnyChannels;
    bool m_busy = false;
    bool m_loaded = false;
};
