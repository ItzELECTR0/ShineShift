#pragma once

#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

#include <deque>

struct DisplayRecord {
    QString id;
    QString mfg;
    QString model;
    QString serial;
    QString connector;
    QString capabilities;
};
Q_DECLARE_METATYPE(DisplayRecord)

// Owns every DDC call. Lives on its own thread; the post functions are safe to call from any thread.
class DdcWorker final : public QObject {
    Q_OBJECT

public:
    explicit DdcWorker(QObject *parent = nullptr);
    ~DdcWorker() override;

    void postDetect(bool rescan);
    // A queued write to the same control is replaced, so only the newest value reaches the monitor.
    void postWrite(int generation, int display, int feature, int value);
    void postRead(int generation, int display, int feature);

signals:
    void failed(const QString &error);
    void detected(int generation, const QList<DisplayRecord> &displays);
    void valueRead(int generation, int display, int feature, int value, int maximum, bool ok);
    void written(int generation, int display, int feature, int value, bool ok);

private:
    enum class Kind { Detect, Write, Read };
    struct Task {
        Kind kind;
        int generation = 0;
        int display = 0;
        int feature = 0;
        int value = 0;
        bool rescan = false;
    };

    Q_INVOKABLE void drain();
    void schedule();
    bool take(Task &task);
    void detect(bool rescan);

    QMutex m_mutex;
    std::deque<Task> m_tasks;
    bool m_scheduled = false;
    bool m_initialised = false;
    int m_generation = 0;
};
