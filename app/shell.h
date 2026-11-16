#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>

class Displays;
class Settings;

// Places the flyout and the overlay: layer-shell surfaces on Wayland, plain borderless windows on X11.
class Shell final : public QObject {
    Q_OBJECT

public:
    Shell(Displays *displays, Settings *settings, QObject *root, QObject *parent = nullptr);

private:
    void showFlyout();
    void hideFlyout();
    void showOverlay();
    QRect area() const;

    Displays *m_displays;
    Settings *m_settings;
    QPointer<QQuickWindow> m_flyout;
    QPointer<QQuickWindow> m_overlay;
};
