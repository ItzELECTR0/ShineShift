#pragma once

#include "worker.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSettings>
#include <QSocketNotifier>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariantList>

#include <functional>
#include <memory>

class Displays;
class Settings;

namespace Vcp {
inline constexpr int Brightness = 0x10;
inline constexpr int Contrast = 0x12;
inline constexpr int Input = 0x60;
inline constexpr int Volume = 0x62;
inline constexpr int Power = 0xd6;
}

class Monitor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY settingsChanged)
    Q_PROPERTY(QString customName READ customName NOTIFY settingsChanged)
    Q_PROPERTY(QString model READ model CONSTANT)
    Q_PROPERTY(QString connector READ connector CONSTANT)
    Q_PROPERTY(QString capabilities READ capabilities CONSTANT)
    Q_PROPERTY(int brightness READ brightness NOTIFY valuesChanged)
    Q_PROPERTY(int contrast READ contrast NOTIFY valuesChanged)
    Q_PROPERTY(int volume READ volume NOTIFY valuesChanged)
    Q_PROPERTY(int input READ input NOTIFY valuesChanged)
    Q_PROPERTY(bool powered READ powered NOTIFY valuesChanged)
    Q_PROPERTY(QVariantList inputs READ inputs CONSTANT)
    Q_PROPERTY(bool supportsContrast READ supportsContrast CONSTANT)
    Q_PROPERTY(bool supportsVolume READ supportsVolume CONSTANT)
    Q_PROPERTY(bool supportsInput READ supportsInput CONSTANT)
    Q_PROPERTY(bool supportsPower READ supportsPower CONSTANT)
    Q_PROPERTY(bool showContrast READ showContrast WRITE setShowContrast NOTIFY settingsChanged)
    Q_PROPERTY(bool showVolume READ showVolume WRITE setShowVolume NOTIFY settingsChanged)
    Q_PROPERTY(bool showInput READ showInput WRITE setShowInput NOTIFY settingsChanged)
    Q_PROPERTY(bool showPower READ showPower WRITE setShowPower NOTIFY settingsChanged)
    Q_PROPERTY(bool hold READ hold WRITE setHold NOTIFY settingsChanged)
    Q_PROPERTY(bool hidden READ hidden WRITE setHidden NOTIFY settingsChanged)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum NOTIFY settingsChanged)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList features READ features NOTIFY featuresChanged)
    Q_PROPERTY(bool hdr READ hdr NOTIFY outputChanged)
    Q_PROPERTY(double sdr READ sdr NOTIFY outputChanged)
    Q_PROPERTY(bool sdrAdjustable READ sdrAdjustable NOTIFY outputChanged)

public:
    Monitor(Displays *owner, const DisplayRecord &record);

    QString id() const { return m_record.id; }
    QString name() const { return m_name.isEmpty() ? m_record.model : m_name; }
    QString customName() const { return m_name; }
    void setName(const QString &name);
    QString model() const { return m_record.model; }
    QString connector() const { return m_record.connector; }
    QString capabilities() const { return m_record.capabilities; }

    int brightness() const;
    int contrast() const { return scaled(Vcp::Contrast); }
    int volume() const { return scaled(Vcp::Volume); }
    int input() const { return raw(Vcp::Input) & 0xff; }
    bool powered() const { return !known(Vcp::Power) || (raw(Vcp::Power) & 0xff) <= 1; }
    QVariantList inputs() const { return m_inputs; }
    bool supportsContrast() const { return supports(Vcp::Contrast); }
    bool supportsVolume() const { return supports(Vcp::Volume); }
    bool supportsInput() const { return supports(Vcp::Input); }
    bool supportsPower() const { return supports(Vcp::Power); }

    bool showContrast() const { return m_showContrast; }
    void setShowContrast(bool show);
    bool showVolume() const { return m_showVolume; }
    void setShowVolume(bool show);
    bool showInput() const { return m_showInput; }
    void setShowInput(bool show);
    bool showPower() const { return m_showPower; }
    void setShowPower(bool show);
    bool hold() const { return m_hold; }
    void setHold(bool hold);
    bool hidden() const { return m_hidden; }
    void setHidden(bool hidden);
    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    QVariantList features() const;

    bool hdr() const { return m_hdr; }
    double sdr() const { return m_sdr; }
    bool sdrAdjustable() const { return !m_selector.isEmpty(); }

    Q_INVOKABLE void setBrightness(int percent);
    Q_INVOKABLE void setContrast(int percent) { setScaled(Vcp::Contrast, percent); }
    Q_INVOKABLE void setVolume(int percent) { setScaled(Vcp::Volume, percent); }
    Q_INVOKABLE void setInput(int value);
    Q_INVOKABLE void setPowered(bool on);
    Q_INVOKABLE void setSdr(double value);
    Q_INVOKABLE void setFeature(int feature, int percent) { setScaled(feature, percent); }
    // Returns an empty string, or why the code was refused.
    Q_INVOKABLE QString addFeature(const QString &code, const QString &label);
    Q_INVOKABLE void removeFeature(int feature);

signals:
    void settingsChanged();
    void valuesChanged();
    void featuresChanged();
    void outputChanged();

private:
    friend class Displays;
    struct Reading {
        int value = 0;
        int maximum = 100;
        bool known = false;
    };
    struct Feature {
        int code;
        QString label;
    };

    bool supports(int feature) const { return m_supported.contains(feature); }
    bool known(int feature) const { return m_values.value(feature).known; }
    int raw(int feature) const { return m_values.value(feature).value; }
    int scaled(int feature) const;
    void setScaled(int feature, int percent);
    int powerMode(int state) const;
    void setPower(int state);
    int rawBrightnessFor(int percent) const;
    void load();
    void store(const QString &key, const QVariant &value);
    void storeFeatures();
    QList<int> shownFeatures() const;

    Displays *m_owner;
    DisplayRecord m_record;
    int m_index = -1;
    QHash<int, Reading> m_values;
    QHash<int, int> m_pending;
    QList<int> m_supported;
    QList<int> m_powerStates;
    QVariantList m_inputs;
    QList<Feature> m_features;
    QString m_name;
    bool m_showContrast = false;
    bool m_showVolume = false;
    bool m_showInput = false;
    bool m_showPower = false;
    bool m_hold = false;
    bool m_hidden = false;
    int m_minimum = 0;
    int m_maximum = 100;
    int m_order = 0;
    int m_remembered = -1;
    double m_rememberedSdr = 0;
    qint64 m_enforceUntil = 0;
    bool m_hdr = false;
    bool m_hdrKnown = false;
    double m_sdr = 1.0;
    QString m_selector;
};

class Displays final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject *> monitors READ monitors NOTIFY monitorsChanged)
    Q_PROPERTY(QList<QObject *> allMonitors READ allMonitors NOTIFY monitorsChanged)
    Q_PROPERTY(bool detecting READ detecting NOTIFY detectingChanged)
    Q_PROPERTY(QString error READ error NOTIFY detectingChanged)
    Q_PROPERTY(bool flyoutOpen READ flyoutOpen NOTIFY flyoutOpenChanged)
    Q_PROPERTY(bool settingsOpen READ settingsOpen NOTIFY settingsOpenChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayChanged)
    Q_PROPERTY(QString overlayTitle READ overlayTitle NOTIFY overlayChanged)
    Q_PROPERTY(QString overlayIcon READ overlayIcon NOTIFY overlayChanged)
    Q_PROPERTY(QString overlayText READ overlayText NOTIFY overlayChanged)
    Q_PROPERTY(double overlayLevel READ overlayLevel NOTIFY overlayChanged)
    Q_PROPERTY(bool layered READ layered CONSTANT)
    Q_PROPERTY(QString compositor READ compositor CONSTANT)
    Q_PROPERTY(int hyprConfig READ hyprConfig NOTIFY hyprConfigChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit Displays(Settings *settings, QObject *parent = nullptr);
    ~Displays() override;

    QList<QObject *> monitors() const;
    QList<QObject *> allMonitors() const;
    bool detecting() const { return m_detecting; }
    QString error() const { return m_error; }
    bool flyoutOpen() const { return m_flyoutOpen; }
    bool settingsOpen() const { return m_settingsOpen; }
    bool overlayVisible() const { return m_overlayVisible; }
    QString overlayTitle() const { return m_overlayTitle; }
    QString overlayIcon() const { return m_overlayIcon; }
    QString overlayText() const { return m_overlayText; }
    double overlayLevel() const { return m_overlayLevel; }
    bool layered() const { return m_layered; }
    void setLayered(bool layered) { m_layered = layered; }
    QString compositor() const;
    int hyprConfig() const { return m_hyprConfig; }
    QString version() const;
    QString summary() const;

    // Runs a command line and returns what it prints. Commands that need monitors wait for the first detect.
    QString handleCommand(const QStringList &arguments);
    void start();

    Q_INVOKABLE void toggleFlyout();
    Q_INVOKABLE void closeFlyout();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void closeSettings();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void turnOffDisplays();
    Q_INVOKABLE void move(QObject *monitor, int direction);
    Q_INVOKABLE void floatWindow(const QString &title, int width, int height);
    // Turns keeping SDR brightness in the Hyprland config on or off; false when the config could not be changed.
    Q_INVOKABLE bool setPersistSdr(bool on);
    Q_INVOKABLE void scroll(int steps);
    Q_INVOKABLE void quit();

signals:
    void monitorsChanged();
    void detectingChanged();
    void flyoutOpenChanged();
    void settingsOpenChanged();
    void overlayChanged();
    void hyprConfigChanged();
    void summaryChanged();
    void output(const QString &text);

private:
    friend class Monitor;

    void write(Monitor *monitor, int feature, int value);
    void read(Monitor *monitor, int feature);
    void afterRead(Monitor *monitor, int feature, std::function<void(Monitor *, bool)> then);
    void applyReading(int display, int feature, int value, int maximum);
    Monitor *monitorAt(int display) const;
    void setBrightness(Monitor *monitor, int percent, bool spread);
    QList<Monitor *> visible() const;
    void reapply(Monitor *monitor);
    void reapplyAll();
    void refreshValues(Monitor *monitor);
    void refreshOutputs(bool known);
    void applySdr(Monitor *monitor, double value, bool remember);
    void writeHyprConfig();
    void saveState(Monitor *monitor);
    void flash(Monitor *monitor, const QString &icon, double level, const QString &text);
    void watchCompositor();
    void sortMonitors();
    QList<Monitor *> targets(const QString &target) const;
    QString run(const QStringList &arguments);
    QString list() const;

    void onDetected(int generation, const QList<DisplayRecord> &displays);
    void onValueRead(int generation, int display, int feature, int value, int maximum, bool ok);
    void onWritten(int generation, int display, int feature, int value, bool ok);

    Settings *m_settings;
    QSettings m_config;
    std::unique_ptr<QSettings> m_state;
    QThread m_thread;
    DdcWorker *m_worker;
    QList<Monitor *> m_monitors;
    int m_generation = 0;
    bool m_detecting = true;
    bool m_ready = false;
    QString m_error;
    QList<QStringList> m_waiting;
    struct Followup {
        int display;
        int feature;
        std::function<void(Monitor *, bool)> then;
    };
    QList<Followup> m_followups;
    bool m_flyoutOpen = false;
    bool m_settingsOpen = false;
    bool m_layered = false;
    bool m_overlayVisible = false;
    QString m_overlayTitle;
    QString m_overlayIcon;
    QString m_overlayText;
    double m_overlayLevel = 0;
    int m_hyprConfig = -1;
    QElapsedTimer m_clock;
    QTimer m_holdTimer;
    QTimer m_outputTimer;
    QTimer m_overlayTimer;
    QTimer m_stateTimer;
    QTimer m_rescanTimer;
    QTimer m_configTimer;
    std::unique_ptr<QSocketNotifier> m_events;
    std::unique_ptr<QSocketNotifier> m_hotplug;

private slots:
    void prepareForSleep(bool sleeping);
};
