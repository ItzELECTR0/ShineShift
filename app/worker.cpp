#include "worker.h"

#include "core.h"

#include <QMetaObject>
#include <QMutexLocker>

#include <algorithm>

DdcWorker::DdcWorker(QObject *parent) : QObject(parent) { qRegisterMetaType<QList<DisplayRecord>>(); }

DdcWorker::~DdcWorker() {
    if (m_initialised) shine_shutdown();
}

void DdcWorker::schedule() {
    if (m_scheduled) return;
    m_scheduled = true;
    QMetaObject::invokeMethod(this, "drain", Qt::QueuedConnection);
}

void DdcWorker::postDetect(bool rescan) {
    QMutexLocker lock(&m_mutex);
    // Display numbers change with a detect, so anything still queued is stale.
    m_tasks.clear();
    m_tasks.push_back({Kind::Detect, 0, 0, 0, 0, rescan});
    schedule();
}

void DdcWorker::postWrite(int generation, int display, int feature, int value) {
    QMutexLocker lock(&m_mutex);
    for (Task &task : m_tasks) {
        if (task.kind == Kind::Write && task.display == display && task.feature == feature) {
            task.value = value;
            task.generation = generation;
            return;
        }
    }
    m_tasks.push_back({Kind::Write, generation, display, feature, value, false});
    schedule();
}

void DdcWorker::postRead(int generation, int display, int feature) {
    QMutexLocker lock(&m_mutex);
    for (const Task &task : m_tasks)
        if (task.kind == Kind::Read && task.display == display && task.feature == feature) return;
    m_tasks.push_back({Kind::Read, generation, display, feature, 0, false});
    schedule();
}

bool DdcWorker::take(Task &task) {
    QMutexLocker lock(&m_mutex);
    if (m_tasks.empty()) {
        m_scheduled = false;
        return false;
    }
    // Writes go first so a slider never waits behind a round of reads.
    auto next = std::find_if(m_tasks.begin(), m_tasks.end(), [](const Task &entry) { return entry.kind != Kind::Read; });
    if (next == m_tasks.end()) next = m_tasks.begin();
    task = *next;
    m_tasks.erase(next);
    return true;
}

void DdcWorker::detect(bool rescan) {
    if (!m_initialised) {
        char error[160] = "";
        if (shine_init(error, sizeof error) != SHINE_OK) {
            emit failed(QString::fromUtf8(error));
            return;
        }
        m_initialised = true;
        rescan = false;
    }
    ShineDisplay found[SHINE_MAX_DISPLAYS];
    const int count = shine_detect(found, SHINE_MAX_DISPLAYS, rescan ? 1 : 0);
    QList<DisplayRecord> displays;
    for (int index = 0; index < count; ++index) {
        char capabilities[2048] = "";
        shine_capabilities(index, capabilities, sizeof capabilities);
        displays.append({QString::fromUtf8(found[index].id), QString::fromUtf8(found[index].mfg),
                         QString::fromUtf8(found[index].model), QString::fromUtf8(found[index].serial),
                         QString::fromUtf8(found[index].connector), QString::fromLatin1(capabilities)});
    }
    emit detected(++m_generation, displays);
}

void DdcWorker::drain() {
    Task task;
    while (take(task)) {
        if (task.kind == Kind::Detect) {
            detect(task.rescan);
            continue;
        }
        if (task.generation != m_generation) continue;
        if (task.kind == Kind::Write) {
            const bool ok = shine_set(task.display, task.feature, task.value) == SHINE_OK;
            emit written(task.generation, task.display, task.feature, task.value, ok);
        } else {
            int value = 0, maximum = 0;
            const bool ok = shine_get(task.display, task.feature, &value, &maximum) == SHINE_OK;
            emit valueRead(task.generation, task.display, task.feature, value, maximum, ok);
        }
    }
}
