#include "shell.h"

#include "displays.h"
#include "settings.h"

#include <LayerShellQt/Window>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QScreen>
#include <QTimer>

#include <cstdio>

using LayerWindow = LayerShellQt::Window;

namespace {
constexpr int OverlayMargin = 72;

QQuickWindow *namedWindow(QObject *root, const char *property) {
    return qobject_cast<QQuickWindow *>(root->property(property).value<QObject *>());
}

// An invisible surface over another output that closes the flyout when clicked.
class Catcher final : public QRasterWindow {
public:
    Catcher(QScreen *screen, Displays *displays) : m_displays(displays) {
        QSurfaceFormat format = this->format();
        format.setAlphaBufferSize(8);
        setFormat(format);
        setFlag(Qt::FramelessWindowHint);
        auto *layer = LayerWindow::get(this);
        layer->setScope("shineshift-catcher");
        layer->setLayer(LayerWindow::LayerOverlay);
        layer->setAnchors({LayerWindow::AnchorTop, LayerWindow::AnchorBottom, LayerWindow::AnchorLeft, LayerWindow::AnchorRight});
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);
        layer->setActivateOnShow(false);
        layer->setScreen(screen);
        layer->setDesiredSize(QSize(0, 0));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(QRect(QPoint(), size()), Qt::transparent);
    }
    // Queued, because closing the flyout destroys this window.
    void mousePressEvent(QMouseEvent *) override {
        QMetaObject::invokeMethod(m_displays, [displays = m_displays] { displays->closeFlyout(); }, Qt::QueuedConnection);
    }

private:
    Displays *m_displays;
};

void followSize(QQuickWindow *window) {
    auto *layer = LayerWindow::get(window);
    layer->setDesiredSize(window->size());
    QObject::connect(window, &QWindow::widthChanged, window, [window, layer] { layer->setDesiredSize(window->size()); });
    QObject::connect(window, &QWindow::heightChanged, window, [window, layer] { layer->setDesiredSize(window->size()); });
}
}

Shell::Shell(Displays *displays, Settings *settings, QObject *root, QObject *parent)
    : QObject(parent), m_displays(displays), m_settings(settings) {
    m_flyout = namedWindow(root, "flyout");
    m_overlay = namedWindow(root, "overlay");
    if (!m_flyout || !m_overlay) {
        std::fprintf(stderr, "ShineShift could not find its windows\n");
        return;
    }

    if (m_displays->layered()) {
        // The flyout covers the whole output so a click anywhere beside the card closes it.
        auto *flyout = LayerWindow::get(m_flyout);
        flyout->setScope("shineshift");
        flyout->setLayer(LayerWindow::LayerOverlay);
        flyout->setAnchors({LayerWindow::AnchorTop, LayerWindow::AnchorBottom, LayerWindow::AnchorLeft, LayerWindow::AnchorRight});
        flyout->setExclusiveZone(0);
        flyout->setActivateOnShow(true);
        flyout->setWantsToBeOnActiveScreen(true);
        flyout->setDesiredSize(QSize(0, 0));

        auto *overlay = LayerWindow::get(m_overlay);
        overlay->setScope("shineshift-overlay");
        overlay->setLayer(LayerWindow::LayerOverlay);
        overlay->setExclusiveZone(-1);
        overlay->setActivateOnShow(false);
        overlay->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);
        overlay->setWantsToBeOnActiveScreen(true);
        followSize(m_overlay);
    }

    connect(m_displays, &Displays::flyoutOpenChanged, this, [this] { m_displays->flyoutOpen() ? showFlyout() : hideFlyout(); });
    if (m_displays->layered()) {
        const auto replace = [this] {
            if (m_displays->flyoutOpen()) placeCatchers();
        };
        connect(m_flyout, &QWindow::screenChanged, this, replace);
        connect(qGuiApp, &QGuiApplication::screenAdded, this, replace);
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, replace);
    }
    connect(m_displays, &Displays::overlayChanged, this, [this] {
        if (m_displays->overlayVisible()) showOverlay();
        else m_overlay->setVisible(false);
    });
}

QRect Shell::area() const {
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
}

// The compositor picks the flyout's output, so its screen is only known once it maps; screenChanged corrects a guess.
void Shell::placeCatchers() {
    m_catchers.clear();
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen == m_flyout->screen()) continue;
        auto catcher = std::make_unique<Catcher>(screen, m_displays);
        catcher->setVisible(true);
        m_catchers.push_back(std::move(catcher));
    }
}

void Shell::showFlyout() {
    if (m_displays->layered()) {
        // Catchers go first: compositors stack a later layer surface above an earlier one on the same output.
        placeCatchers();
        LayerWindow::get(m_flyout)->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityExclusive);
    } else {
        const QRect screen = area();
        const QString corner = m_settings->text("flyoutCorner");
        const QSize size = m_flyout->size();
        const int x = corner.endsWith("left") ? screen.left() : screen.right() - size.width() + 1;
        const int y = corner.startsWith("top") ? screen.top() : screen.bottom() - size.height() + 1;
        m_flyout->setPosition(x, y);
    }
    m_flyout->setVisible(true);
    m_flyout->requestActivate();
}

void Shell::hideFlyout() {
    if (!m_flyout->isVisible()) return;
    if (!m_displays->layered()) {
        m_flyout->setVisible(false);
        return;
    }
    m_catchers.clear();
    // Unmapping a surface that still holds exclusive keyboard focus leaves sway's pointer stuck on it.
    LayerWindow::get(m_flyout)->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);
    m_flyout->update();
    QTimer::singleShot(50, m_flyout, [this] {
        if (!m_displays->flyoutOpen()) m_flyout->setVisible(false);
    });
}

void Shell::showOverlay() {
    const bool top = m_settings->text("overlayPosition") == "top";
    if (m_displays->layered()) {
        auto *layer = LayerWindow::get(m_overlay);
        // A mapped layer surface keeps its old anchor, so a moved overlay has to be mapped again.
        const LayerWindow::Anchors anchors = top ? LayerWindow::AnchorTop : LayerWindow::AnchorBottom;
        if (layer->anchors() != anchors) m_overlay->setVisible(false);
        layer->setAnchors(anchors);
        layer->setMargins(top ? QMargins(0, OverlayMargin, 0, 0) : QMargins(0, 0, 0, OverlayMargin));
    } else {
        const QRect screen = area();
        const QSize size = m_overlay->size();
        m_overlay->setPosition(screen.center().x() - size.width() / 2,
                               top ? screen.top() + OverlayMargin : screen.bottom() - size.height() - OverlayMargin);
    }
    m_overlay->setVisible(true);
}
