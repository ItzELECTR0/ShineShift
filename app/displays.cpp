#include "displays.h"

#include "core.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <poll.h>
#include <unistd.h>

namespace {
constexpr double SdrMinimum = 0.25;
constexpr double SdrMaximum = 3.0;
constexpr int EnforceWindow = 10000;

const char *inputName(int value) {
    static const char *names[] = {nullptr, "VGA 1", "VGA 2", "DVI 1", "DVI 2", "Composite 1", "Composite 2",
                                  "S-Video 1", "S-Video 2", "Tuner 1", "Tuner 2", "Tuner 3", "Component 1",
                                  "Component 2", "Component 3", "DisplayPort 1", "DisplayPort 2", "HDMI 1", "HDMI 2"};
    return value > 0 && value < int(std::size(names)) ? names[value] : nullptr;
}

QString inputLabel(int value) {
    if (const char *name = inputName(value)) return QString::fromLatin1(name);
    return QStringLiteral("Input 0x%1").arg(value, 2, 16, QLatin1Char('0'));
}

int percentOf(int value, int maximum) { return maximum > 0 ? int(std::lround(value * 100.0 / maximum)) : 0; }

bool parseInt(const QString &text, int &value) {
    bool ok = false;
    value = text.startsWith("0x", Qt::CaseInsensitive) ? text.mid(2).toInt(&ok, 16) : text.toInt(&ok);
    return ok;
}

bool isRelative(const QString &text) { return text.startsWith('+') || text.startsWith('-'); }

QString fail(const QString &message) { return QStringLiteral("error: ") + message; }
}

Monitor::Monitor(Displays *owner, const DisplayRecord &record) : QObject(owner), m_owner(owner), m_record(record) {
    const QStringList entries = record.capabilities.split(' ', Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        bool ok = false;
        const int code = entry.section('=', 0, 0).toInt(&ok, 16);
        if (!ok) continue;
        m_supported.append(code);
        for (const QString &option : entry.section('=', 1).split(',', Qt::SkipEmptyParts)) {
            const int value = option.toInt(&ok, 16);
            if (ok && code == Vcp::Input) m_inputs.append(QVariantMap{{"value", value}, {"label", inputLabel(value)}});
            if (ok && code == Vcp::Power) m_powerStates.append(value);
        }
    }
    // A monitor that reports no capabilities may still answer, so nothing is ruled out.
    if (entries.isEmpty()) m_supported = {Vcp::Brightness, Vcp::Contrast, Vcp::Volume, Vcp::Power};
    load();
}

void Monitor::load() {
    QSettings &config = m_owner->m_config;
    config.beginGroup("monitors/" + id());
    m_name = config.value("name").toString();
    m_showContrast = config.value("contrast", false).toBool();
    m_showVolume = config.value("volume", false).toBool();
    m_showInput = config.value("input", false).toBool();
    m_showPower = config.value("power", false).toBool();
    m_hold = config.value("hold", false).toBool();
    m_hidden = config.value("hidden", false).toBool();
    m_minimum = std::clamp(config.value("minimum", 0).toInt(), 0, 99);
    m_maximum = std::clamp(config.value("maximum", 100).toInt(), m_minimum + 1, 100);
    m_order = config.value("order", 1000).toInt();
    for (const QString &entry : config.value("features").toStringList()) {
        bool ok = false;
        const int code = entry.section(':', 0, 0).toInt(&ok, 16);
        if (ok) m_features.append({code, entry.section(':', 1)});
    }
    config.endGroup();
    m_remembered = m_owner->m_state->value(id() + "/brightness", -1).toInt();
    m_rememberedSdr = m_owner->m_state->value(id() + "/sdr", 0.0).toDouble();
}

void Monitor::store(const QString &key, const QVariant &value) {
    m_owner->m_config.setValue("monitors/" + id() + "/" + key, value);
    emit settingsChanged();
}

void Monitor::storeFeatures() {
    QStringList entries;
    for (const Feature &feature : m_features) entries.append(QString::number(feature.code, 16) + ":" + feature.label);
    m_owner->m_config.setValue("monitors/" + id() + "/features", entries);
    emit featuresChanged();
}

void Monitor::setName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed == m_name) return;
    m_name = trimmed;
    store("name", m_name);
    emit m_owner->summaryChanged();
}

void Monitor::setShowContrast(bool show) {
    if (show == m_showContrast) return;
    m_showContrast = show;
    store("contrast", show);
    if (show) m_owner->read(this, Vcp::Contrast);
}

void Monitor::setShowVolume(bool show) {
    if (show == m_showVolume) return;
    m_showVolume = show;
    store("volume", show);
    if (show) m_owner->read(this, Vcp::Volume);
}

void Monitor::setShowInput(bool show) {
    if (show == m_showInput) return;
    m_showInput = show;
    store("input", show);
    if (show) m_owner->read(this, Vcp::Input);
}

void Monitor::setShowPower(bool show) {
    if (show == m_showPower) return;
    m_showPower = show;
    store("power", show);
    if (show) m_owner->read(this, Vcp::Power);
}

void Monitor::setHold(bool hold) {
    if (hold == m_hold) return;
    m_hold = hold;
    store("hold", hold);
    if (hold) m_owner->read(this, Vcp::Brightness);
}

void Monitor::setHidden(bool hidden) {
    if (hidden == m_hidden) return;
    m_hidden = hidden;
    store("hidden", hidden);
    emit m_owner->monitorsChanged();
    emit m_owner->summaryChanged();
}

void Monitor::setMinimum(int minimum) {
    minimum = std::clamp(minimum, 0, m_maximum - 1);
    if (minimum == m_minimum) return;
    m_minimum = minimum;
    store("minimum", minimum);
    emit valuesChanged();
}

void Monitor::setMaximum(int maximum) {
    maximum = std::clamp(maximum, m_minimum + 1, 100);
    if (maximum == m_maximum) return;
    m_maximum = maximum;
    store("maximum", maximum);
    emit valuesChanged();
}

QVariantList Monitor::features() const {
    QVariantList out;
    for (const Feature &feature : m_features)
        out.append(QVariantMap{{"code", feature.code},
                               {"label", feature.label},
                               {"value", scaled(feature.code)},
                               {"known", known(feature.code)}});
    return out;
}

QList<int> Monitor::shownFeatures() const {
    QList<int> shown;
    if (m_showContrast) shown.append(Vcp::Contrast);
    if (m_showVolume) shown.append(Vcp::Volume);
    if (m_showInput) shown.append(Vcp::Input);
    if (m_showPower) shown.append(Vcp::Power);
    for (const Feature &feature : m_features) shown.append(feature.code);
    return shown;
}

int Monitor::scaled(int feature) const {
    const Reading reading = m_values.value(feature);
    return reading.known ? percentOf(reading.value, reading.maximum) : 0;
}

void Monitor::setScaled(int feature, int percent) {
    Reading &reading = m_values[feature];
    const int value = int(std::lround(std::clamp(percent, 0, 100) * reading.maximum / 100.0));
    if (reading.known && reading.value == value) return;
    reading.value = value;
    reading.known = true;
    m_owner->write(this, feature, value);
    emit valuesChanged();
    emit featuresChanged();
}

int Monitor::brightness() const { return shine_to_percent(scaled(Vcp::Brightness), m_minimum, m_maximum); }

int Monitor::rawBrightnessFor(int percent) const {
    const int maximum = m_values.value(Vcp::Brightness).maximum;
    return int(std::lround(shine_to_raw(percent, m_minimum, m_maximum) * maximum / 100.0));
}

void Monitor::setBrightness(int percent) { m_owner->setBrightness(this, percent, m_owner->m_settings->flag("linked")); }

void Monitor::setInput(int value) {
    Reading &reading = m_values[Vcp::Input];
    reading.value = value;
    reading.known = true;
    m_owner->write(this, Vcp::Input, value);
    emit valuesChanged();
}

void Monitor::setPowered(bool on) {
    // MCCS power modes: 1 on, 2 standby, 4 off, 5 off like the power button.
    const auto offered = [this](int state) { return m_powerStates.isEmpty() || m_powerStates.contains(state); };
    const bool off = m_owner->m_settings->text("powerSignal") == "off";
    Reading &reading = m_values[Vcp::Power];
    reading.value = on ? 1 : off ? (offered(4) ? 4 : 5) : (offered(2) ? 2 : 4);
    reading.known = true;
    m_owner->write(this, Vcp::Power, reading.value);
    emit valuesChanged();
}

void Monitor::setSdr(double value) { m_owner->applySdr(this, value, true); }

QString Monitor::addFeature(const QString &code, const QString &label) {
    int feature = 0;
    const QString text = code.trimmed();
    bool ok = false;
    feature = (text.startsWith("0x", Qt::CaseInsensitive) ? text.mid(2) : text).toInt(&ok, 16);
    if (!ok || feature < 1 || feature > 0xff) return tr("Enter a code like 0x16.");
    const QList<int> taken = {Vcp::Brightness, Vcp::Contrast, Vcp::Volume, Vcp::Input, Vcp::Power};
    if (taken.contains(feature)) return tr("That one already has its own switch.");
    for (const Feature &existing : m_features)
        if (existing.code == feature) return tr("This code is already added.");
    const QString name = label.trimmed().isEmpty() ? QStringLiteral("0x%1").arg(feature, 2, 16, QLatin1Char('0')).toUpper().replace("0X", "0x") : label.trimmed();
    m_features.append({feature, name});
    storeFeatures();
    m_owner->read(this, feature);
    return {};
}

void Monitor::removeFeature(int feature) {
    m_features.removeIf([feature](const Feature &entry) { return entry.code == feature; });
    storeFeatures();
}

Displays::Displays(Settings *settings, QObject *parent) : QObject(parent), m_settings(settings) {
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/ShineShift";
    QDir().mkpath(folder);
    m_state = std::make_unique<QSettings>(folder + "/state.conf", QSettings::IniFormat);
    m_clock.start();

    m_worker = new DdcWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &DdcWorker::detected, this, &Displays::onDetected);
    connect(m_worker, &DdcWorker::valueRead, this, &Displays::onValueRead);
    connect(m_worker, &DdcWorker::written, this, &Displays::onWritten);
    connect(m_worker, &DdcWorker::failed, this, [this](const QString &error) {
        m_error = error;
        m_detecting = false;
        emit detectingChanged();
    });

    m_holdTimer.callOnTimeout(this, [this] {
        for (Monitor *monitor : std::as_const(m_monitors))
            if (monitor->m_hold) read(monitor, Vcp::Brightness);
    });
    const auto paceHold = [this] { m_holdTimer.start(std::clamp(m_settings->number("holdInterval"), 2, 600) * 1000); };
    connect(m_settings, &QQmlPropertyMap::valueChanged, this, [paceHold](const QString &key) {
        if (key == "holdInterval") paceHold();
    });
    paceHold();

    m_outputTimer.callOnTimeout(this, [this] { refreshOutputs(false); });
    m_overlayTimer.setSingleShot(true);
    m_overlayTimer.setInterval(1600);
    m_overlayTimer.callOnTimeout(this, [this] {
        m_overlayVisible = false;
        emit overlayChanged();
    });
    m_stateTimer.setSingleShot(true);
    m_stateTimer.setInterval(500);
    m_stateTimer.callOnTimeout(this, [this] { m_state->sync(); });
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.callOnTimeout(this, [this] { m_worker->postDetect(true); });
    m_configTimer.setSingleShot(true);
    m_configTimer.setInterval(1500);
    m_configTimer.callOnTimeout(this, &Displays::writeHyprConfig);

    QDBusConnection::systemBus().connect("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager",
                                         "PrepareForSleep", this, SLOT(prepareForSleep(bool)));
}

Displays::~Displays() {
    m_thread.quit();
    m_thread.wait(5000);
    m_state->sync();
}

void Displays::start() {
    m_thread.start();
    m_worker->postDetect(false);
}

QString Displays::version() const { return QCoreApplication::applicationVersion(); }

QString Displays::compositor() const {
    switch (shine_compositor()) {
    case SHINE_COMPOSITOR_HYPRLAND: return "hyprland";
    case SHINE_COMPOSITOR_FAKE: return "fake";
    default: return {};
    }
}

QList<Monitor *> Displays::visible() const {
    QList<Monitor *> out;
    for (Monitor *monitor : m_monitors)
        if (!monitor->m_hidden) out.append(monitor);
    return out;
}

QList<QObject *> Displays::monitors() const {
    const QList<Monitor *> shown = visible();
    return {shown.begin(), shown.end()};
}

QList<QObject *> Displays::allMonitors() const { return {m_monitors.begin(), m_monitors.end()}; }

QString Displays::summary() const {
    QStringList parts;
    for (Monitor *monitor : visible()) parts.append(QStringLiteral("%1 %2%").arg(monitor->name()).arg(monitor->brightness()));
    return parts.isEmpty() ? tr("No displays") : parts.join(QStringLiteral(", "));
}

void Displays::sortMonitors() {
    std::stable_sort(m_monitors.begin(), m_monitors.end(), [](const Monitor *a, const Monitor *b) {
        return a->m_order != b->m_order ? a->m_order < b->m_order : a->m_index < b->m_index;
    });
}

void Displays::onDetected(int generation, const QList<DisplayRecord> &displays) {
    m_generation = generation;
    QList<Monitor *> next;
    for (int index = 0; index < displays.size(); ++index) {
        DisplayRecord record = displays.at(index);
        // Two identical monitors without serial numbers would otherwise share their settings.
        const QString base = record.id;
        for (int copy = 2; std::any_of(next.begin(), next.end(), [&](Monitor *m) { return m->id() == record.id; }); ++copy)
            record.id = QStringLiteral("%1-%2").arg(base).arg(copy);
        Monitor *monitor = nullptr;
        for (Monitor *existing : std::as_const(m_monitors))
            if (existing->id() == record.id) monitor = existing;
        if (!monitor) monitor = new Monitor(this, record);
        monitor->m_index = index;
        monitor->m_pending.clear();
        next.append(monitor);
    }
    for (Monitor *old : std::as_const(m_monitors))
        if (!next.contains(old)) old->deleteLater();
    m_monitors = next;
    sortMonitors();

    m_detecting = false;
    m_error.clear();
    if (!m_ready) {
        m_ready = true;
        m_hyprConfig = shine_config_status();
        emit hyprConfigChanged();
        watchCompositor();
    }
    refreshOutputs(true);
    for (Monitor *monitor : std::as_const(m_monitors)) {
        reapply(monitor);
        refreshValues(monitor);
    }
    emit monitorsChanged();
    emit detectingChanged();
    emit summaryChanged();

    const QList<QStringList> waiting = std::exchange(m_waiting, {});
    for (const QStringList &arguments : waiting) {
        const QString text = run(arguments);
        if (!text.isEmpty()) emit output(text);
    }
}

void Displays::write(Monitor *monitor, int feature, int value) {
    if (monitor->m_index < 0) return;
    monitor->m_pending[feature] = value;
    m_worker->postWrite(m_generation, monitor->m_index, feature, value);
}

void Displays::read(Monitor *monitor, int feature) {
    if (monitor->m_index < 0 || monitor->m_pending.contains(feature)) return;
    m_worker->postRead(m_generation, monitor->m_index, feature);
}

void Displays::onWritten(int generation, int display, int feature, int value, bool ok) {
    if (generation != m_generation) return;
    for (Monitor *monitor : std::as_const(m_monitors)) {
        if (monitor->m_index != display) continue;
        if (monitor->m_pending.value(feature, -1) == value) monitor->m_pending.remove(feature);
        if (!ok) read(monitor, feature);
    }
}

void Displays::onValueRead(int generation, int display, int feature, int value, int maximum, bool ok) {
    if (generation != m_generation || !ok) return;
    Monitor *monitor = nullptr;
    for (Monitor *candidate : std::as_const(m_monitors))
        if (candidate->m_index == display) monitor = candidate;
    // A write that is still queued makes this reading out of date already.
    if (!monitor || monitor->m_pending.contains(feature)) return;

    Monitor::Reading &reading = monitor->m_values[feature];
    reading.maximum = maximum > 0 ? maximum : 100;
    reading.known = true;
    if (feature != Vcp::Brightness) {
        reading.value = value;
        emit monitor->valuesChanged();
        emit monitor->featuresChanged();
        return;
    }

    const bool enforce = monitor->m_remembered >= 0 && (monitor->m_hold || m_clock.elapsed() < monitor->m_enforceUntil);
    if (enforce && value != monitor->m_remembered) {
        reading.value = monitor->m_remembered;
        write(monitor, Vcp::Brightness, monitor->m_remembered);
    } else {
        reading.value = value;
        if (monitor->m_remembered != value) {
            monitor->m_remembered = value;
            saveState(monitor);
        }
    }
    emit monitor->valuesChanged();
    emit summaryChanged();
}

void Displays::saveState(Monitor *monitor) {
    if (monitor->m_remembered >= 0) m_state->setValue(monitor->id() + "/brightness", monitor->m_remembered);
    if (monitor->m_rememberedSdr > 0) m_state->setValue(monitor->id() + "/sdr", monitor->m_rememberedSdr);
    m_stateTimer.start();
}

// Writes the remembered brightness back, then checks twice that the monitor kept it.
void Displays::reapply(Monitor *monitor) {
    const bool wanted = m_settings->flag("reapply") || monitor->m_hold;
    if (!wanted || monitor->m_remembered < 0) {
        read(monitor, Vcp::Brightness);
        return;
    }
    Monitor::Reading &reading = monitor->m_values[Vcp::Brightness];
    reading.value = monitor->m_remembered;
    reading.known = true;
    write(monitor, Vcp::Brightness, monitor->m_remembered);
    monitor->m_enforceUntil = m_clock.elapsed() + EnforceWindow;
    for (int delay : {3000, 8000}) {
        QTimer::singleShot(delay, this, [this, id = monitor->id(), generation = m_generation] {
            if (generation != m_generation) return;
            for (Monitor *candidate : std::as_const(m_monitors))
                if (candidate->id() == id) read(candidate, Vcp::Brightness);
        });
    }
    emit monitor->valuesChanged();
}

void Displays::reapplyAll() {
    for (Monitor *monitor : std::as_const(m_monitors)) reapply(monitor);
}

void Displays::refreshValues(Monitor *monitor) {
    for (int feature : monitor->shownFeatures()) read(monitor, feature);
}

// `spread` carries the level to every visible display, which is what linked levels mean.
void Displays::setBrightness(Monitor *monitor, int percent, bool spread) {
    QList<Monitor *> affected = spread ? visible() : QList<Monitor *>{};
    if (!affected.contains(monitor)) affected.append(monitor);
    for (Monitor *target : std::as_const(affected)) {
        Monitor::Reading &reading = target->m_values[Vcp::Brightness];
        const int value = target->rawBrightnessFor(std::clamp(percent, 0, 100));
        if (reading.known && reading.value == value && target->m_remembered == value) continue;
        reading.value = value;
        reading.known = true;
        target->m_remembered = value;
        write(target, Vcp::Brightness, value);
        saveState(target);
        emit target->valuesChanged();
    }
    emit summaryChanged();
}

// At a known moment (start, connect, reload) remembered SDR brightness is written; otherwise changes are adopted.
void Displays::refreshOutputs(bool known) {
    if (shine_compositor() == SHINE_COMPOSITOR_NONE) return;
    // A reload that is already waiting on the event socket has to win over this poll.
    if (!known && m_events) {
        pollfd waiting{int(m_events->socket()), POLLIN, 0};
        if (::poll(&waiting, 1, 0) > 0) return;
    }
    char buffer[8192] = "";
    if (shine_outputs(buffer, sizeof buffer, known ? 1 : 0) != SHINE_OK) return;
    const QStringList lines = QString::fromUtf8(buffer).split('\n', Qt::SkipEmptyParts);
    for (Monitor *monitor : std::as_const(m_monitors)) {
        QStringList match;
        for (const QString &line : lines) {
            const QStringList fields = line.split('\t');
            if (fields.size() < 5) continue;
            const bool byName = !monitor->connector().isEmpty() && fields[0] == monitor->connector();
            const bool byDescription = monitor->connector().isEmpty() && fields[1].contains(monitor->model()) &&
                                       fields[1].contains(monitor->m_record.serial);
            if (byName || byDescription) match = fields;
        }
        if (match.isEmpty()) continue;
        const bool hdr = match[2] == "1";
        const double reported = match[3].toDouble();
        const bool flipped = monitor->m_hdrKnown && monitor->m_hdr != hdr;
        const bool changed = monitor->m_hdr != hdr || monitor->m_selector != match[4] || std::abs(monitor->m_sdr - reported) > 0.0005;
        monitor->m_hdr = hdr;
        monitor->m_hdrKnown = true;
        monitor->m_selector = match[4];
        monitor->m_sdr = reported;
        const bool differs = std::abs(reported - monitor->m_rememberedSdr) > 0.0005;
        if (known && monitor->m_rememberedSdr > 0 && differs) {
            applySdr(monitor, monitor->m_rememberedSdr, false);
        } else if (!known && monitor->m_rememberedSdr > 0 && differs) {
            monitor->m_rememberedSdr = reported;
            saveState(monitor);
        }
        if (changed) emit monitor->outputChanged();
        // Monitors tend to reset their backlight when HDR switches.
        if (flipped) reapply(monitor);
    }
}

void Displays::applySdr(Monitor *monitor, double value, bool remember) {
    if (monitor->m_selector.isEmpty()) return;
    value = std::round(std::clamp(value, SdrMinimum, SdrMaximum) * 100.0) / 100.0;
    if (shine_set_sdr(monitor->m_selector.toUtf8().constData(), value) != SHINE_OK) return;
    monitor->m_sdr = value;
    if (remember) {
        monitor->m_rememberedSdr = value;
        saveState(monitor);
        if (m_settings->flag("persistSdr")) m_configTimer.start();
    }
    emit monitor->outputChanged();
}

void Displays::writeHyprConfig() {
    QString entries;
    for (Monitor *monitor : std::as_const(m_monitors))
        if (monitor->m_rememberedSdr > 0 && !monitor->m_selector.isEmpty())
            entries += QStringLiteral("%1\t%2\n").arg(monitor->m_selector).arg(monitor->m_rememberedSdr, 0, 'f', 3);
    shine_config_write(entries.toUtf8().constData());
}

bool Displays::setPersistSdr(bool on) {
    if (on) {
        if (shine_config_install() != SHINE_OK) return false;
        m_settings->set("persistSdr", true);
        writeHyprConfig();
    } else {
        m_settings->set("persistSdr", false);
        // The require line stays, so the file has to stay too; it just stops setting anything.
        shine_config_write("");
    }
    m_hyprConfig = shine_config_status();
    emit hyprConfigChanged();
    return true;
}

void Displays::watchCompositor() {
    if (shine_compositor() != SHINE_COMPOSITOR_NONE) m_outputTimer.start(3000);

    const int events = shine_events_open();
    if (events >= 0) {
        m_events = std::make_unique<QSocketNotifier>(events, QSocketNotifier::Read);
        connect(m_events.get(), &QSocketNotifier::activated, this, [this, events] {
            const int mask = shine_events_read(events);
            if (mask < 0) {
                m_events.reset();
                ::close(events);
                return;
            }
            if (mask & SHINE_EVENT_RELOADED) {
                refreshOutputs(true);
                reapplyAll();
                m_hyprConfig = shine_config_status();
                emit hyprConfigChanged();
            }
            if (mask & SHINE_EVENT_OUTPUTS) QTimer::singleShot(500, this, [this] { refreshOutputs(false); });
        });
    }

    const int hotplug = shine_is_fake() ? -1 : shine_hotplug_open();
    if (hotplug >= 0) {
        m_hotplug = std::make_unique<QSocketNotifier>(hotplug, QSocketNotifier::Read);
        connect(m_hotplug.get(), &QSocketNotifier::activated, this, [this, hotplug] {
            const int change = shine_hotplug_read(hotplug);
            if (change < 0) {
                m_hotplug.reset();
                ::close(hotplug);
            } else if (change == 1) {
                // Connectors settle over a second or two, and one plug sends several events.
                m_rescanTimer.start(2500);
            }
        });
    }
}

void Displays::prepareForSleep(bool sleeping) {
    if (!sleeping) m_rescanTimer.start(4000);
}

void Displays::toggleFlyout() {
    if (m_flyoutOpen) {
        closeFlyout();
        return;
    }
    m_flyoutOpen = true;
    m_overlayVisible = false;
    emit overlayChanged();
    emit flyoutOpenChanged();
    refreshOutputs(false);
    for (Monitor *monitor : std::as_const(m_monitors)) {
        read(monitor, Vcp::Brightness);
        refreshValues(monitor);
    }
}

void Displays::closeFlyout() {
    if (!m_flyoutOpen) return;
    m_flyoutOpen = false;
    emit flyoutOpenChanged();
}

void Displays::openSettings() {
    closeFlyout();
    if (m_settingsOpen) return;
    m_settingsOpen = true;
    emit settingsOpenChanged();
    for (Monitor *monitor : std::as_const(m_monitors)) refreshValues(monitor);
}

void Displays::closeSettings() {
    if (!m_settingsOpen) return;
    m_settingsOpen = false;
    emit settingsOpenChanged();
}

void Displays::refresh() {
    m_detecting = true;
    emit detectingChanged();
    m_worker->postDetect(true);
}

void Displays::turnOffDisplays() {
    closeFlyout();
    // The delay lets the key or button that asked for this be released first, or it wakes the screens again.
    QTimer::singleShot(700, this, [this] {
        const QString action = m_settings->text("turnOffAction");
        bool software = false;
        if (action != "hardware") software = shine_screens_off() == SHINE_OK;
        if (action == "software" && software) return;
        for (Monitor *monitor : std::as_const(m_monitors))
            if (monitor->supportsPower()) monitor->setPowered(false);
    });
}

void Displays::move(QObject *object, int direction) {
    auto *monitor = qobject_cast<Monitor *>(object);
    const qsizetype from = m_monitors.indexOf(monitor);
    const qsizetype to = from + (direction < 0 ? -1 : 1);
    if (from < 0 || to < 0 || to >= m_monitors.size()) return;
    m_monitors.swapItemsAt(from, to);
    for (int index = 0; index < m_monitors.size(); ++index) {
        m_monitors[index]->m_order = index;
        m_config.setValue("monitors/" + m_monitors[index]->id() + "/order", index);
    }
    emit monitorsChanged();
    emit summaryChanged();
}

void Displays::floatWindow(const QString &title, int width, int height) {
    shine_float_window(title.toUtf8().constData(), width, height);
}

void Displays::flash(Monitor *monitor, const QString &icon, double level, const QString &text) {
    if (!m_settings->flag("overlay") || m_flyoutOpen) return;
    m_overlayTitle = monitor ? monitor->name() : tr("All displays");
    m_overlayIcon = icon;
    m_overlayLevel = std::clamp(level, 0.0, 1.0);
    m_overlayText = text;
    m_overlayVisible = true;
    m_overlayTimer.start();
    emit overlayChanged();
}

void Displays::scroll(int steps) {
    const QList<Monitor *> shown = visible();
    if (steps == 0 || shown.isEmpty()) return;
    const int amount = steps * std::clamp(m_settings->number("scrollStep"), 1, 50);
    const bool linked = m_settings->flag("linked");
    for (Monitor *monitor : shown) {
        setBrightness(monitor, std::clamp(monitor->brightness() + amount, 0, 100), linked);
        if (linked) break;
    }
    Monitor *first = shown.first();
    flash(shown.size() == 1 ? first : nullptr, "brightness_high_regular", first->brightness() / 100.0,
          QStringLiteral("%1%").arg(first->brightness()));
}

void Displays::quit() { QCoreApplication::quit(); }

QList<Monitor *> Displays::targets(const QString &target) const {
    const QList<Monitor *> shown = visible();
    if (target.compare("all", Qt::CaseInsensitive) == 0) return shown;
    bool number = false;
    const int position = target.toInt(&number);
    if (number) return position >= 1 && position <= shown.size() ? QList<Monitor *>{shown[position - 1]} : QList<Monitor *>{};
    for (Monitor *monitor : m_monitors)
        if (monitor->connector().compare(target, Qt::CaseInsensitive) == 0) return {monitor};
    for (Monitor *monitor : m_monitors)
        if (monitor->name().compare(target, Qt::CaseInsensitive) == 0) return {monitor};
    QList<Monitor *> partial;
    for (Monitor *monitor : m_monitors)
        if (monitor->id().contains(target, Qt::CaseInsensitive) || monitor->name().contains(target, Qt::CaseInsensitive))
            partial.append(monitor);
    return partial.size() == 1 ? partial : QList<Monitor *>{};
}

QString Displays::list() const {
    if (!m_error.isEmpty()) return fail(m_error);
    QStringList lines;
    int position = 0;
    for (Monitor *monitor : m_monitors) {
        QStringList parts;
        parts << (monitor->m_hidden ? QStringLiteral("-") : QString::number(++position));
        parts << (monitor->connector().isEmpty() ? QStringLiteral("?") : monitor->connector()) << monitor->name();
        parts << QStringLiteral("brightness=%1").arg(monitor->brightness());
        if (monitor->known(Vcp::Contrast)) parts << QStringLiteral("contrast=%1").arg(monitor->contrast());
        if (monitor->known(Vcp::Volume)) parts << QStringLiteral("volume=%1").arg(monitor->volume());
        if (monitor->m_hdr) parts << QStringLiteral("sdr=%1").arg(monitor->m_sdr, 0, 'f', 2);
        if (monitor->m_hold) parts << QStringLiteral("hold");
        if (monitor->m_hidden) parts << QStringLiteral("hidden");
        parts << QStringLiteral("id=%1").arg(monitor->id());
        lines << parts.join('\t');
    }
    return lines.isEmpty() ? tr("No displays found.") : lines.join('\n');
}

QString Displays::handleCommand(const QStringList &arguments) {
    QStringList rest = arguments;
    rest.removeAll("--daemon");
    static const QStringList immediate = {"toggle", "open", "close", "settings", "quit", "refresh"};
    const QString command = rest.value(0);
    if (rest.isEmpty() && arguments.contains("--daemon")) return {};
    if (!m_ready && m_error.isEmpty() && !rest.isEmpty() && !immediate.contains(command)) {
        m_waiting.append(arguments);
        return {};
    }
    return run(arguments);
}

QString Displays::run(const QStringList &arguments) {
    QStringList rest = arguments;
    rest.removeAll("--daemon");
    const bool quiet = rest.removeAll("--quiet") > 0;
    const QString command = rest.isEmpty() ? QStringLiteral("toggle") : rest.takeFirst();

    if (command == "toggle" || command == "open") {
        if (command == "toggle" || !m_flyoutOpen) toggleFlyout();
        return {};
    }
    if (command == "close") {
        closeFlyout();
        closeSettings();
        return {};
    }
    if (command == "settings") {
        openSettings();
        return {};
    }
    if (command == "quit") {
        // Answer the caller first, then leave.
        QTimer::singleShot(0, this, &Displays::quit);
        return {};
    }
    if (command == "refresh") {
        refresh();
        return {};
    }
    if (command == "list") return list();
    if (command == "off") {
        turnOffDisplays();
        return {};
    }

    static const QStringList known = {"set", "offset", "contrast", "volume", "sdr", "vcp", "input", "power", "hold", "drift"};
    if (!known.contains(command)) return fail(tr("unknown command \"%1\", see shineshift --help").arg(command));
    if (rest.size() < 2) return fail(tr("%1 needs a display and a value").arg(command));
    const QString target = rest.takeFirst();
    const QList<Monitor *> chosen = targets(target);
    if (chosen.isEmpty()) return fail(tr("no display matches \"%1\", see shineshift list").arg(target));
    const QString argument = rest.takeFirst();
    Monitor *const subject = chosen.size() == 1 ? chosen.first() : nullptr;
    QString icon, text;
    double level = 0;
    const auto show = [&](const char *name, double fraction, const QString &label) {
        if (!icon.isEmpty()) return;
        icon = QString::fromLatin1(name);
        level = fraction;
        text = label;
    };

    for (Monitor *monitor : chosen) {
        if (command == "set" || command == "offset") {
            int amount = 0;
            if (!parseInt(argument, amount)) return fail(tr("\"%1\" is not a number").arg(argument));
            const bool relative = command == "offset" || isRelative(argument);
            const int percent = std::clamp(relative ? monitor->brightness() + amount : amount, 0, 100);
            // A command names its displays itself, so linked levels do not widen it.
            setBrightness(monitor, percent, false);
            show("brightness_high_regular", percent / 100.0, QStringLiteral("%1%").arg(percent));
        } else if (command == "contrast" || command == "volume") {
            const int feature = command == "contrast" ? Vcp::Contrast : Vcp::Volume;
            int amount = 0;
            if (!parseInt(argument, amount)) return fail(tr("\"%1\" is not a number").arg(argument));
            if (isRelative(argument) && !monitor->known(feature)) {
                read(monitor, feature);
                return fail(tr("%1 has not reported its %2 yet, try again").arg(monitor->name(), command));
            }
            const int percent = std::clamp(isRelative(argument) ? monitor->scaled(feature) + amount : amount, 0, 100);
            if (!monitor->known(feature)) monitor->m_values[feature].maximum = 100;
            monitor->setScaled(feature, percent);
            show(feature == Vcp::Contrast ? "circle_half_fill_regular" : "speaker_2_regular", percent / 100.0, QStringLiteral("%1%").arg(percent));
        } else if (command == "sdr") {
            bool ok = false;
            const double amount = argument.toDouble(&ok);
            if (!ok) return fail(tr("\"%1\" is not a number").arg(argument));
            if (!monitor->m_hdr || !monitor->sdrAdjustable()) {
                if (chosen.size() > 1) continue;
                return fail(tr("%1 has no SDR brightness to set here").arg(monitor->name()));
            }
            const double value = std::clamp(isRelative(argument) ? monitor->m_sdr + amount : amount, SdrMinimum, SdrMaximum);
            applySdr(monitor, value, true);
            show("hdr_regular", (value - SdrMinimum) / (SdrMaximum - SdrMinimum), QString::number(monitor->m_sdr, 'f', 2));
        } else if (command == "vcp") {
            int feature = 0, value = 0;
            if (!parseInt(argument, feature) || feature < 0 || feature > 0xff) return fail(tr("\"%1\" is not a VCP code").arg(argument));
            if (rest.isEmpty() || !parseInt(rest.first(), value)) return fail(tr("vcp needs a display, a code and a value"));
            write(monitor, feature, value);
            if (monitor->known(feature)) {
                monitor->m_values[feature].value = value;
                if (feature == Vcp::Brightness) {
                    monitor->m_remembered = value;
                    saveState(monitor);
                }
                emit monitor->valuesChanged();
                emit monitor->featuresChanged();
            }
        } else if (command == "input") {
            int value = -1;
            for (const QVariant &entry : monitor->m_inputs) {
                const QVariantMap option = entry.toMap();
                const QString label = option.value("label").toString();
                if (label.compare(argument, Qt::CaseInsensitive) == 0 || QString(label).remove(' ').compare(argument, Qt::CaseInsensitive) == 0)
                    value = option.value("value").toInt();
            }
            if (value < 0 && !parseInt(argument, value)) return fail(tr("%1 has no input called \"%2\"").arg(monitor->name(), argument));
            monitor->setInput(value);
        } else if (command == "power") {
            if (argument != "on" && argument != "off") return fail(tr("power takes on or off"));
            monitor->setPowered(argument == "on");
        } else if (command == "hold") {
            if (argument != "on" && argument != "off") return fail(tr("hold takes on or off"));
            monitor->setHold(argument == "on");
        } else if (command == "drift") {
            int feature = 0, value = 0;
            if (!shine_is_fake()) return fail(tr("drift only exists for fake displays"));
            if (!parseInt(argument, feature) || rest.isEmpty() || !parseInt(rest.first(), value)) return fail(tr("drift needs a code and a value"));
            shine_drift(monitor->m_index, feature, value);
        }
    }
    if (!quiet && !icon.isEmpty()) flash(subject, icon, level, text);
    return {};
}
