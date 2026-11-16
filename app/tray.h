#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QObject>
#include <QVariantMap>

class Displays;
class ThemeSource;

struct IconPixmap {
    int width = 0;
    int height = 0;
    QByteArray data;
};
using IconPixmapList = QList<IconPixmap>;

struct ToolTip {
    QString icon;
    IconPixmapList pixmaps;
    QString title;
    QString text;
};

struct MenuLayout {
    int id = 0;
    QVariantMap properties;
    QList<MenuLayout> children;
};

struct MenuProperties {
    int id = 0;
    QVariantMap properties;
};
using MenuPropertiesList = QList<MenuProperties>;

struct MenuEvent {
    int id = 0;
    QString name;
    QDBusVariant data;
    uint timestamp = 0;
};
using MenuEventList = QList<MenuEvent>;

Q_DECLARE_METATYPE(IconPixmap)
Q_DECLARE_METATYPE(ToolTip)
Q_DECLARE_METATYPE(MenuLayout)
Q_DECLARE_METATYPE(MenuProperties)
Q_DECLARE_METATYPE(MenuEvent)

class Tray;

class TrayItemAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category READ category CONSTANT)
    Q_PROPERTY(QString Id READ id CONSTANT)
    Q_PROPERTY(QString Title READ id CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(uint WindowId READ windowId CONSTANT)
    Q_PROPERTY(QString IconThemePath READ empty CONSTANT)
    Q_PROPERTY(QString IconName READ iconName)
    Q_PROPERTY(IconPixmapList IconPixmap READ iconPixmap)
    Q_PROPERTY(QString OverlayIconName READ empty CONSTANT)
    Q_PROPERTY(IconPixmapList OverlayIconPixmap READ noPixmap CONSTANT)
    Q_PROPERTY(QString AttentionIconName READ empty CONSTANT)
    Q_PROPERTY(IconPixmapList AttentionIconPixmap READ noPixmap CONSTANT)
    Q_PROPERTY(QString AttentionMovieName READ empty CONSTANT)
    Q_PROPERTY(ToolTip ToolTip READ toolTip)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu READ menu CONSTANT)

public:
    explicit TrayItemAdaptor(Tray *tray);

    QString category() const { return QStringLiteral("Hardware"); }
    QString id() const { return QStringLiteral("ShineShift"); }
    QString status() const { return QStringLiteral("Active"); }
    uint windowId() const { return 0; }
    QString empty() const { return {}; }
    IconPixmapList noPixmap() const { return {}; }
    bool itemIsMenu() const { return false; }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/MenuBar")); }
    QString iconName() const;
    IconPixmapList iconPixmap() const;
    ToolTip toolTip() const;

public slots:
    void Activate(int x, int y);
    void SecondaryActivate(int x, int y);
    void ContextMenu(int x, int y);
    void Scroll(int delta, const QString &orientation);

signals:
    void NewTitle();
    void NewIcon();
    void NewToolTip();
    void NewStatus(const QString &status);

private:
    Tray *m_tray;
};

class TrayMenuAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(uint Version READ version CONSTANT)
    Q_PROPERTY(QString TextDirection READ textDirection CONSTANT)
    Q_PROPERTY(QString Status READ status CONSTANT)
    Q_PROPERTY(QStringList IconThemePath READ iconThemePath CONSTANT)

public:
    explicit TrayMenuAdaptor(Tray *tray);

    uint version() const { return 3; }
    QString textDirection() const { return QStringLiteral("ltr"); }
    QString status() const { return QStringLiteral("normal"); }
    QStringList iconThemePath() const { return {}; }

public slots:
    uint GetLayout(int parentId, int recursionDepth, const QStringList &propertyNames, MenuLayout &layout);
    MenuPropertiesList GetGroupProperties(const QList<int> &ids, const QStringList &propertyNames);
    QDBusVariant GetProperty(int id, const QString &name);
    void Event(int id, const QString &eventId, const QDBusVariant &data, uint timestamp);
    QList<int> EventGroup(const MenuEventList &events);
    bool AboutToShow(int id);
    QList<int> AboutToShowGroup(const QList<int> &ids, QList<int> &idErrors);

signals:
    void LayoutUpdated(uint revision, int parent);
    void ItemActivationRequested(int id, uint timestamp);

private:
    Tray *m_tray;
};

// A StatusNotifierItem with a small menu, spoken directly so scrolling on the icon reaches us.
class Tray final : public QObject {
    Q_OBJECT

public:
    Tray(Displays *displays, ThemeSource *theme, QObject *parent = nullptr);

    Displays *displays() const { return m_displays; }
    IconPixmapList pixmaps() const;
    QString iconName() const;
    void scroll(int delta);
    void activate(int id);
    QVariantMap properties(int id) const;
    QList<int> items() const { return {1, 2, 3, 4, 5, 6}; }

private:
    void announce();

    Displays *m_displays;
    ThemeSource *m_theme;
    QObject m_itemObject;
    QObject m_menuObject;
    TrayItemAdaptor *m_item;
    TrayMenuAdaptor *m_menu;
    QDBusServiceWatcher m_watcher;
    QString m_service;
    int m_scrolled = 0;
};
