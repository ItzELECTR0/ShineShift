#pragma once

#include <QObject>
#include <QStringList>

inline constexpr auto ShineShiftService = "net.electris.ShineShift";
inline constexpr auto ShineShiftObjectPath = "/net/electris/ShineShift";

class Displays;

class InstanceService final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.electris.ShineShift")

public:
    explicit InstanceService(Displays *displays, QObject *parent = nullptr);

public slots:
    // The reply is what the command prints; a reply starting with "error: " makes the caller exit 1.
    Q_SCRIPTABLE QString Activate(const QStringList &arguments);

private:
    Displays *m_displays;
};
