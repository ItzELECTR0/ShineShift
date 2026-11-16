#include "displays.h"
#include "instance.h"
#include "settings.h"
#include "shell.h"
#include "themesource.h"
#include "tray.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include <cstdio>

InstanceService::InstanceService(Displays *displays, QObject *parent) : QObject(parent), m_displays(displays) {}

QString InstanceService::Activate(const QStringList &arguments) { return m_displays->handleCommand(arguments); }

static const char usage[] =
    "Usage: shineshift [command]\n"
    "\n"
    "A display is its number or connector from \"shineshift list\", its name, or \"all\".\n"
    "A value starting with + or - is relative. A comma list such as 0,50,100 cycles.\n"
    "\n"
    "Commands:\n"
    "  toggle                        Open or close the brightness flyout (default)\n"
    "  list                          List displays and their current levels\n"
    "  set DISPLAY PERCENT[,...]     Set brightness, or cycle through a list\n"
    "  offset DISPLAY AMOUNT         Change brightness by an amount, such as -10\n"
    "  contrast DISPLAY PERCENT      Set contrast\n"
    "  volume DISPLAY PERCENT        Set speaker volume\n"
    "  sdr DISPLAY VALUE             Set SDR brightness on an HDR display, 1.0 is the default\n"
    "  input DISPLAY NAME            Switch input, such as hdmi1\n"
    "  power DISPLAY on|standby|off  Send a power signal, or cycle through a list\n"
    "  hold DISPLAY on|off           Keep writing your brightness back when the display drifts\n"
    "  vcp DISPLAY CODE VALUE        Send a raw DDC/CI value, such as 0x14 5\n"
    "  off                           Turn off all displays\n"
    "  refresh                       Detect displays again\n"
    "  settings                      Open settings\n"
    "  close                         Close the flyout and settings\n"
    "  quit                          Quit the running instance\n"
    "\n"
    "  --daemon    Start without opening anything\n"
    "  --quiet     Change levels without showing the overlay\n"
    "  --version   Print the version\n";

static int print(const QString &reply) {
    if (reply.isEmpty()) return 0;
    const bool failed = reply.startsWith("error: ");
    std::fprintf(failed ? stderr : stdout, "%s\n", qPrintable(failed ? reply.mid(7) : reply));
    std::fflush(failed ? stderr : stdout);
    return failed ? 1 : 0;
}

int main(int argc, char *argv[]) {
    QCoreApplication::setApplicationName("ShineShift");
    QCoreApplication::setOrganizationName("ShineShift");
    QCoreApplication::setApplicationVersion(SHINESHIFT_VERSION);
    QGuiApplication::setDesktopFileName(ShineShiftService);
    QGuiApplication app(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    const QStringList arguments = app.arguments().mid(1);
    if (arguments.contains("--help") || arguments.contains("-h")) {
        std::fputs(usage, stdout);
        return 0;
    }
    if (arguments.contains("--version")) {
        std::puts("shineshift " SHINESHIFT_VERSION);
        return 0;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && !bus.registerService(ShineShiftService)) {
        QDBusMessage call = QDBusMessage::createMethodCall(ShineShiftService, ShineShiftObjectPath, ShineShiftService, "Activate");
        call << arguments;
        const QDBusMessage reply = bus.call(call, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            std::fprintf(stderr, "ShineShift is running but did not answer: %s\n", qPrintable(reply.errorMessage()));
            return 1;
        }
        return print(reply.arguments().value(0).toString());
    }

    Settings settings;
    ThemeSource themes(settings.configFolder());
    // Every control is drawn from Theme; Basic is the only style that never pulls in a platform look.
    QQuickStyle::setStyle("Basic");

    Displays displays(&settings);
    displays.setLayered(QGuiApplication::platformName().startsWith("wayland"));
    QObject::connect(&displays, &Displays::output, &app, [](const QString &text) { print(text); });
    qmlRegisterSingletonInstance("ShineShift.Core", 1, 0, "Displays", &displays);
    qmlRegisterSingletonInstance("ShineShift.Core", 1, 0, "Config", &settings);
    qmlRegisterSingletonInstance("ShineShift.Core", 1, 0, "Appearance", &themes);
    qmlRegisterUncreatableType<Monitor>("ShineShift.Core", 1, 0, "Monitor", "Monitors come from Displays");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) std::fprintf(stderr, "%s\n", qPrintable(warning.toString()));
    });
    engine.addImageProvider("icon", new DesktopIconProvider);
    engine.loadFromModule("ShineShift", "App");
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "ShineShift could not load its interface\n");
        return 1;
    }

    Shell shell(&displays, &settings, engine.rootObjects().constFirst());
    Tray tray(&displays, &settings, &themes);
    InstanceService service(&displays);
    if (bus.isConnected()) bus.registerObject(ShineShiftObjectPath, &service, QDBusConnection::ExportScriptableSlots);

    displays.start();
    print(displays.handleCommand(arguments));
    return app.exec();
}
