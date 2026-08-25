#include "pomodorocontroller.h"

#include "config/appconfig.h"
#include "config/configmanager.h"

#include <QDate>
#include <QTimer>

PomodoroController::PomodoroController(ConfigManager *configManager, QObject *parent)
    : QObject(parent), configManager_(configManager), timer_(new QTimer(this))
{
    const AppConfig &config = configManager_->config();
    phase_ = phaseFromKey(config.pomodoroPhase);
    remainingSeconds_ = config.pomodoroRemainingSeconds;
    completedCycles_ = config.pomodoroCompletedCycles;
    resetDailyCount();
    if (remainingSeconds_ <= 0)
        remainingSeconds_ = durationFor(phase_);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, [this] {
        if (remainingSeconds_ > 0)
            --remainingSeconds_;
        persist();
        emit tick(remainingSeconds_);
        if (remainingSeconds_ == 0) {
            const Phase finished = phase_;
            emit completed(finished);
            advancePhase();
        }
    });
}

bool PomodoroController::isRunning() const { return timer_->isActive(); }

QString PomodoroController::phaseName() const
{
    switch (phase_) {
    case Phase::Work: return QStringLiteral("专注时间");
    case Phase::ShortBreak: return QStringLiteral("短暂休息");
    case Phase::LongBreak: return QStringLiteral("长休息");
    }
    return {};
}

void PomodoroController::start() { if (!timer_->isActive()) { timer_->start(); emit runningChanged(true); } }
void PomodoroController::pause() { if (timer_->isActive()) { timer_->stop(); persist(); emit runningChanged(false); } }
void PomodoroController::skip() { const Phase finished = phase_; emit completed(finished); advancePhase(); }

void PomodoroController::setDurations(int workMinutes, int shortBreakMinutes, int longBreakMinutes)
{
    AppConfig config = configManager_->config();
    config.pomodoroWorkMinutes = qBound(1, workMinutes, 180);
    config.pomodoroShortBreakMinutes = qBound(1, shortBreakMinutes, 60);
    config.pomodoroLongBreakMinutes = qBound(1, longBreakMinutes, 120);
    if (!isRunning())
        remainingSeconds_ = durationFor(phase_);
    configManager_->setConfig(config);
    persist();
    emit tick(remainingSeconds_);
}

int PomodoroController::durationFor(Phase phase) const
{
    const AppConfig &config = configManager_->config();
    if (phase == Phase::ShortBreak) return config.pomodoroShortBreakMinutes * 60;
    if (phase == Phase::LongBreak) return config.pomodoroLongBreakMinutes * 60;
    return config.pomodoroWorkMinutes * 60;
}

void PomodoroController::advancePhase()
{
    resetDailyCount();
    if (phase_ == Phase::Work) {
        ++completedCycles_;
        phase_ = (completedCycles_ % 4 == 0) ? Phase::LongBreak : Phase::ShortBreak;
    } else {
        phase_ = Phase::Work;
    }
    remainingSeconds_ = durationFor(phase_);
    persist();
    emit phaseChanged(phase_, phaseName());
    emit tick(remainingSeconds_);
}

void PomodoroController::persist() const
{
    AppConfig config = configManager_->config();
    config.pomodoroPhase = phaseKey(phase_);
    config.pomodoroRemainingSeconds = remainingSeconds_;
    config.pomodoroCompletedCycles = completedCycles_;
    config.pomodoroCompletedDate = QDate::currentDate().toString(Qt::ISODate);
    configManager_->setConfig(config);
    configManager_->save();
}

void PomodoroController::resetDailyCount()
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    if (configManager_->config().pomodoroCompletedDate == today)
        return;
    completedCycles_ = 0;
    persist();
}

QString PomodoroController::phaseKey(Phase phase)
{
    if (phase == Phase::ShortBreak) return QStringLiteral("shortBreak");
    if (phase == Phase::LongBreak) return QStringLiteral("longBreak");
    return QStringLiteral("work");
}

PomodoroController::Phase PomodoroController::phaseFromKey(const QString &key)
{
    if (key == QStringLiteral("shortBreak")) return Phase::ShortBreak;
    if (key == QStringLiteral("longBreak")) return Phase::LongBreak;
    return Phase::Work;
}
