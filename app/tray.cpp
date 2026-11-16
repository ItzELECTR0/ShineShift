#include "tray.h"

#include "displays.h"
#include "settings.h"
#include "themesource.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QtEndian>

#include <cmath>

namespace {
const char *WatcherService = "org.kde.StatusNotifierWatcher";
const char *SymbolicIcon = "display-brightness-symbolic";

enum Item { Root = 0, Open = 1, Refresh = 2, TurnOff = 3, Separator = 4, OpenSettings = 5, Quit = 6 };
}

QDBusArgument &operator<<(QDBusArgument &argument, const IconPixmap &pixmap) {
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, IconPixmap &pixmap) {
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.data;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ToolTip &tip) {
    argument.beginStructure();
    argument << tip.icon << tip.pixmaps << tip.title << tip.text;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ToolTip &tip) {
    argument.beginStructure();
    argument >> tip.icon >> tip.pixmaps >> tip.title >> tip.text;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const MenuLayout &layout) {
    argument.beginStructure();
    argument << layout.id << layout.properties;
    argument.beginArray(qMetaTypeId<QDBusVariant>());
    for (const MenuLayout &child : layout.children) argument << QDBusVariant(QVariant::fromValue(child));
    argument.endArray();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MenuLayout &layout) {
    argument.beginStructure();
    argument >> layout.id >> layout.properties;
    argument.beginArray();
    while (!argument.atEnd()) {
        QDBusVariant child;
        argument >> child;
        layout.children.append(qdbus_cast<MenuLayout>(child.variant()));
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const MenuProperties &item) {
    argument.beginStructure();
    argument << item.id << item.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MenuProperties &item) {
    argument.beginStructure();
    argument >> item.id >> item.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const MenuEvent &event) {
    argument.beginStructure();
    argument << event.id << event.name << event.data << event.timestamp;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, MenuEvent &event) {
    argument.beginStructure();
    argument >> event.id >> event.name >> event.data >> event.timestamp;
    argument.endStructure();
    return argument;
}

TrayItemAdaptor::TrayItemAdaptor(Tray *tray) : QDBusAbstractAdaptor(tray->findChild<QObject *>("item")), m_tray(tray) {}

QString TrayItemAdaptor::iconName() const { return m_tray->iconName(); }

IconPixmapList TrayItemAdaptor::iconPixmap() const { return m_tray->pixmaps(); }

ToolTip TrayItemAdaptor::toolTip() const {
    return {m_tray->iconName(), m_tray->pixmaps(), QStringLiteral("ShineShift"), m_tray->displays()->summary()};
}

void TrayItemAdaptor::Activate(int, int) { m_tray->displays()->toggleFlyout(); }

void TrayItemAdaptor::SecondaryActivate(int, int) { m_tray->displays()->openSettings(); }

void TrayItemAdaptor::ContextMenu(int, int) { m_tray->displays()->openSettings(); }

void TrayItemAdaptor::Scroll(int delta, const QString &orientation) {
    if (orientation.compare("horizontal", Qt::CaseInsensitive) != 0) m_tray->scroll(delta);
}

TrayMenuAdaptor::TrayMenuAdaptor(Tray *tray) : QDBusAbstractAdaptor(tray->findChild<QObject *>("menu")), m_tray(tray) {}

uint TrayMenuAdaptor::GetLayout(int parentId, int recursionDepth, const QStringList &, MenuLayout &layout) {
    layout.id = parentId;
    layout.properties = m_tray->properties(parentId);
    if (parentId == Root && recursionDepth != 0)
        for (int id : m_tray->items()) layout.children.append({id, m_tray->properties(id), {}});
    return 1;
}

MenuPropertiesList TrayMenuAdaptor::GetGroupProperties(const QList<int> &ids, const QStringList &) {
    MenuPropertiesList out;
    const QList<int> wanted = ids.isEmpty() ? QList<int>{Root} + m_tray->items() : ids;
    for (int id : wanted) out.append({id, m_tray->properties(id)});
    return out;
}

QDBusVariant TrayMenuAdaptor::GetProperty(int id, const QString &name) {
    return QDBusVariant(m_tray->properties(id).value(name, QString()));
}

void TrayMenuAdaptor::Event(int id, const QString &eventId, const QDBusVariant &, uint) {
    if (eventId == "clicked") m_tray->activate(id);
}

QList<int> TrayMenuAdaptor::EventGroup(const MenuEventList &events) {
    for (const MenuEvent &event : events)
        if (event.name == "clicked") m_tray->activate(event.id);
    return {};
}

bool TrayMenuAdaptor::AboutToShow(int) { return false; }

QList<int> TrayMenuAdaptor::AboutToShowGroup(const QList<int> &, QList<int> &idErrors) {
    idErrors.clear();
    return {};
}

Tray::Tray(Displays *displays, Settings *settings, ThemeSource *theme, QObject *parent)
    : QObject(parent), m_displays(displays), m_theme(theme) {
    qDBusRegisterMetaType<IconPixmap>();
    qDBusRegisterMetaType<IconPixmapList>();
    qDBusRegisterMetaType<ToolTip>();
    qDBusRegisterMetaType<MenuLayout>();
    qDBusRegisterMetaType<MenuProperties>();
    qDBusRegisterMetaType<MenuPropertiesList>();
    qDBusRegisterMetaType<MenuEvent>();
    qDBusRegisterMetaType<MenuEventList>();

    m_itemObject.setParent(this);
    m_itemObject.setObjectName("item");
    m_menuObject.setParent(this);
    m_menuObject.setObjectName("menu");
    m_item = new TrayItemAdaptor(this);
    m_menu = new TrayMenuAdaptor(this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return;
    m_service = QStringLiteral("org.kde.StatusNotifierItem-%1-1").arg(QCoreApplication::applicationPid());
    bus.registerObject("/StatusNotifierItem", &m_itemObject, QDBusConnection::ExportAdaptors);
    bus.registerObject("/MenuBar", &m_menuObject, QDBusConnection::ExportAdaptors);

    // Shells get restarted; every new watcher has to be told about us again.
    m_watcher.setConnection(bus);
    m_watcher.addWatchedService(WatcherService);
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
    connect(&m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &Tray::announce);
    connect(displays, &Displays::summaryChanged, m_item, &TrayItemAdaptor::NewToolTip);
    connect(theme, &ThemeSource::changed, m_item, &TrayItemAdaptor::NewIcon);
    connect(settings, &QQmlPropertyMap::valueChanged, this, [this](const QString &key, const QVariant &value) {
        if (key == "tray") setShown(value.toBool());
    });
    setShown(settings->flag("tray"));
}

// Hosts drop an item as soon as its bus name goes away, so releasing the name hides the icon.
void Tray::setShown(bool shown) {
    if (shown == m_shown) return;
    m_shown = shown;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!shown) {
        bus.unregisterService(m_service);
        return;
    }
    bus.registerService(m_service);
    announce();
}

void Tray::announce() {
    if (!m_shown) return;
    QDBusMessage call = QDBusMessage::createMethodCall(WatcherService, "/StatusNotifierWatcher", WatcherService, "RegisterStatusNotifierItem");
    call << m_service;
    QDBusConnection::sessionBus().asyncCall(call);
}

QString Tray::iconName() const { return QIcon::hasThemeIcon(SymbolicIcon) ? QString::fromLatin1(SymbolicIcon) : QString(); }

// A plain sun, for trays whose icon theme has no brightness icon to recolour.
IconPixmapList Tray::pixmaps() const {
    IconPixmapList out;
    const QColor color(m_theme->colors().value("on_surface").toString());
    for (int size : {22, 32, 48}) {
        QImage image(size, size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        const double centre = size / 2.0;
        painter.setPen(QPen(color, size / 11.0, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(centre, centre), size * 0.2, size * 0.2);
        for (int ray = 0; ray < 8; ++ray) {
            const double angle = ray * M_PI / 4.0;
            const QPointF direction(std::cos(angle), std::sin(angle));
            painter.drawLine(QPointF(centre, centre) + direction * size * 0.33, QPointF(centre, centre) + direction * size * 0.43);
        }
        painter.end();
        QByteArray data(size * size * 4, Qt::Uninitialized);
        for (int y = 0; y < size; ++y) {
            const auto *line = reinterpret_cast<const quint32 *>(image.constScanLine(y));
            for (int x = 0; x < size; ++x) qToBigEndian(line[x], data.data() + (y * size + x) * 4);
        }
        out.append({size, size, data});
    }
    return out;
}

// Hosts send either whole steps or wheel units of 120 per notch.
void Tray::scroll(int delta) {
    if (std::abs(delta) < 15) {
        m_displays->scroll(delta);
        return;
    }
    m_scrolled += delta;
    const int steps = m_scrolled / 120;
    m_scrolled -= steps * 120;
    m_displays->scroll(steps);
}

QVariantMap Tray::properties(int id) const {
    switch (id) {
    case Root: return {{"children-display", "submenu"}};
    case Open: return {{"label", tr("Adjust brightness")}};
    case Refresh: return {{"label", tr("Refresh displays")}};
    case TurnOff: return {{"label", tr("Turn off displays")}};
    case Separator: return {{"type", "separator"}};
    case OpenSettings: return {{"label", tr("Settings")}};
    case Quit: return {{"label", tr("Quit")}};
    default: return {};
    }
}

void Tray::activate(int id) {
    switch (id) {
    case Open: m_displays->toggleFlyout(); break;
    case Refresh: m_displays->refresh(); break;
    case TurnOff: m_displays->turnOffDisplays(); break;
    case OpenSettings: m_displays->openSettings(); break;
    case Quit: m_displays->quit(); break;
    default: break;
    }
}
