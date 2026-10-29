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

#include "dirmodel.h"
#include "midimap.h"
#include "parameter.h"
#include "rpcclient.h"
#include "skin.h"
#include "tuner.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>

class QQmlEngine;
class QJSEngine;

/*
 * Engine proxy for the QML UI: the guitariqx counterpart of
 * GxMachineRemote in src/gx_head/gui/machine.cpp.
 *
 * Exposed to QML as the singleton "Gx".
 */
class Machine : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Gx)
    QML_SINGLETON
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString host READ host NOTIFY connectedChanged)
    Q_PROPERTY(int port READ port NOTIFY connectedChanged)
    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY readyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString engineState READ engineState NOTIFY engineStateChanged)
    Q_PROPERTY(double cpuLoad READ cpuLoad NOTIFY cpuLoadChanged)
    Q_PROPERTY(QVariantList banks READ banks NOTIFY banksChanged)
    Q_PROPERTY(QString currentBank READ currentBank NOTIFY presetChanged)
    Q_PROPERTY(QString currentPreset READ currentPreset NOTIFY presetChanged)
    Q_PROPERTY(QStringList monoRack READ monoRack NOTIFY rackChanged)
    Q_PROPERTY(QStringList stereoRack READ stereoRack NOTIFY rackChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY messageReceived)
    Q_PROPERTY(Tuner *tuner READ tuner CONSTANT)
    Q_PROPERTY(MidiMap *midi READ midi CONSTANT)
    Q_PROPERTY(Skin *skin READ skin CONSTANT)
    // the engine runs on this computer: its files can be read directly
    Q_PROPERTY(bool engineLocal READ engineLocal NOTIFY connectedChanged)
    Q_PROPERTY(QVariantList filePlaces READ filePlaces NOTIFY filePlacesChanged)
public:
    // no default argument: a default constructible class would be
    // instantiated by the QML engine instead of calling create()
    explicit Machine(QObject *parent);
    ~Machine() override;

    // used by the QML engine to get the instance created in main()
    static Machine *create(QQmlEngine *, QJSEngine *);
    static void setInstance(Machine *m) { s_instance = m; }
    static Machine *instance() { return s_instance; }

    bool connected() const { return m_rpc.isConnected(); }
    bool ready() const { return m_ready; }
    QString host() const { return m_rpc.host(); }
    int port() const { return m_rpc.port(); }
    QString serverVersion() const { return m_serverVersion; }
    QString status() const { return m_status; }
    QString engineState() const { return m_engineState; }
    double cpuLoad() const { return m_cpuLoad; }
    QVariantList banks() const { return m_banks; }
    QString currentBank() const { return m_currentBank; }
    QString currentPreset() const { return m_currentPreset; }
    QStringList monoRack() const { return m_rack[0]; }
    QStringList stereoRack() const { return m_rack[1]; }
    QVariantList plugins() const { return m_pluginList; }
    QString lastMessage() const { return m_lastMessage; }
    int parameterCount() const { return m_params.size(); }
    Tuner *tuner() { return &m_tuner; }
    MidiMap *midi() { return &m_midi; }
    Skin *skin() { return &m_skin; }

    Q_INVOKABLE void connectTo(const QString& host, int port);
    // Before quitting: the engine does not clean up after a connection
    // closes, so switch its tuner off (and unmute it), leave MIDI config
    // mode and wait until this is sent.
    void shutdown();

    Q_INVOKABLE Parameter *param(const QString& id) const;
    Q_INVOKABLE bool hasParam(const QString& id) const { return m_params.contains(id); }
    Q_INVOKABLE QVariantMap pluginInfo(const QString& id) const;

    // layout of a rack unit (see uitree.h); empty until unitUiReady(id)
    Q_INVOKABLE QVariantMap unitUi(const QString& id);

    Q_INVOKABLE void setPreset(const QString& bank, const QString& preset);
    Q_INVOKABLE void stepPreset(int delta);

    // Preset and bank editing. The engine does not check bank names (an
    // unknown one crashes it) nor duplicate preset names, so all names are
    // validated here first. The methods return an error text, or an empty
    // string when the request was sent; bank and current preset state is
    // fetched again afterwards since the engine does not broadcast it.
    Q_INVOKABLE QVariantMap bank(const QString& name) const;
    Q_INVOKABLE bool bankMutable(const QString& name) const;
    // banks presets can be saved to
    Q_INVOKABLE QStringList writableBanks() const;
    Q_INVOKABLE bool presetExists(const QString& bank, const QString& preset) const;
    Q_INVOKABLE QString saveCurrent();
    Q_INVOKABLE QString savePreset(const QString& bank, const QString& name);
    Q_INVOKABLE QString renamePreset(const QString& bank, const QString& oldName, const QString& newName);
    Q_INVOKABLE QString erasePreset(const QString& bank, const QString& name);
    Q_INVOKABLE QString movePreset(const QString& bank, const QString& name, int delta);
    Q_INVOKABLE QString copyPreset(const QString& fromBank, const QString& preset,
                                   const QString& toBank, const QString& newName);
    Q_INVOKABLE QString newBank(const QString& name);
    Q_INVOKABLE QString renameBank(const QString& oldName, const QString& newName);
    Q_INVOKABLE QString removeBank(const QString& name);
    Q_INVOKABLE QString moveBank(const QString& name, int delta);
    // Bank files (.gx) of this computer: the contents are sent to or read
    // from the engine, which can run elsewhere. Results: bankImported(),
    // bankExported().
    Q_INVOKABLE QString importBank(const QString& file);
    Q_INVOKABLE QString exportBank(const QString& name, const QString& file);
    // file name proposed when saving a bank
    Q_INVOKABLE QString bankFileName(const QString& name) const;
    Q_INVOKABLE void setEngineState(const QString& state);

    Q_INVOKABLE void insertUnit(const QString& id, const QString& before, bool stereo);
    Q_INVOKABLE void removeUnit(const QString& id, bool stereo);
    Q_INVOKABLE void moveUnit(const QString& id, int delta, bool stereo);

    // output parameters (meters) are polled while registered
    Q_INVOKABLE void watchOutput(const QString& id);
    Q_INVOKABLE void unwatchOutput(const QString& id);

    // Files on the engine's computer (see DirModel). The engine lists
    // remote directories from JSON-RPC interface 1.2 on.
    bool engineLocal() const { return m_engineLocal; }
    bool remoteFiles() const { return m_interfaceMinor >= 2; }
    using ListHandler = std::function<void(const QString& path, const QVector<DirModel::Entry>& entries,
                                           const QString& error)>;
    void listDirectory(const QString& path, ListHandler handler);
    // {name, path} of folders the engine uses (IR folders, model folders)
    QVariantList filePlaces() const { return m_filePlaces; }
    // IR settings store the factory folder as "%S"
    Q_INVOKABLE QString resolveIrDir(const QString& dir) const {
        return dir.startsWith(QLatin1String("%S")) && !m_irFactoryDir.isEmpty()
            ? m_irFactoryDir + dir.mid(2) : dir;
    }
    // load an impulse response file into a convolver unit (jconv,
    // jconv_mono): like "reset" in gx_head's IR window, the whole file
    // with normalized gain
    Q_INVOKABLE void loadImpulseResponse(const QString& unit, const QString& file);
    // load a model into a NAM or RTNeural unit; fileParam is the
    // *.loadfile (or loadafile/loadbfile) parameter
    Q_INVOKABLE void loadModel(const QString& fileParam, const QString& file);

signals:
    void connectedChanged();
    void readyChanged();
    void statusChanged();
    void engineStateChanged();
    void cpuLoadChanged();
    void banksChanged();
    void presetChanged();
    // a bank was created by newBank(), with the name the engine gave it
    void bankCreated(const QString& name);
    // name is the bank the engine created (empty on error)
    void bankImported(const QString& file, const QString& name, const QString& error);
    void bankExported(const QString& name, const QString& file, const QString& error);
    void rackChanged();
    void pluginsChanged();
    void unitUiReady(const QString& id);
    void filePlacesChanged();
    // result of loadImpulseResponse()
    void impulseResponseLoaded(const QString& unit, const QString& file, const QString& error);
    // result of loadModel()
    void modelLoaded(const QString& fileParam, const QString& file, const QString& error);
    void messageReceived(const QString& level, const QString& text);

private:
    void onConnected();
    void onDisconnected();
    void onNotification(const QString& method, const QJsonArray& params);
    void onParameterChangeRequested(Parameter *p);
    void setStatus(const QString& s);
    void loadParameters(const QJsonArray& list);
    void loadPlugins(const QJsonArray& list);
    void requestBanks();
    void refreshPresetState();
    void requestFilePlaces();
    // edits run one after the other: a move reads the list, then reorders
    // it, and must not miss an edit sent in between
    using EditDone = std::function<void()>;
    void enqueueEdit(std::function<void(EditDone)> op);
    void runNextEdit();
    void requestRack();
    void pollOutputs();
    QList<Parameter*> unitParameters(const QString& id) const;
    void finishUnitUi(const QString& id, QVariantMap ui);

    static Machine *s_instance;

    RpcClient m_rpc;
    Tuner m_tuner;
    MidiMap m_midi;
    Skin m_skin;
    QHash<QString, Parameter*> m_params;
    QHash<QString, QVariantMap> m_pluginInfo;
    QVariantList m_pluginList;
    QHash<QString, QVariantMap> m_unitUi;
    QSet<QString> m_unitUiPending;
    QHash<QString, int> m_watched;
    QTimer m_pollTimer;
    QTimer m_cpuTimer;
    QTimer m_rackTimer;
    QVariantList m_banks;
    QStringList m_rack[2];
    QString m_currentBank;
    QString m_currentPreset;
    QString m_engineState;
    QString m_serverVersion;
    int m_interfaceMinor = 0;
    bool m_engineLocal = true;
    QVariantList m_filePlaces;
    QString m_irFactoryDir;
    QString m_status;
    QString m_lastMessage;
    double m_cpuLoad = 0;
    bool m_ready = false;
    QList<std::function<void(EditDone)>> m_editQueue;
    bool m_editRunning = false;
};
