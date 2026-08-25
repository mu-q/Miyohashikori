#pragma once

#include <QObject>
#include <QString>

class ConfigManager;
class QTimer;

class PomodoroController : public QObject
{
    Q_OBJECT
public:
    enum class Phase { Work, ShortBreak, LongBreak };

    explicit PomodoroController(ConfigManager *configManager, QObject *parent = nullptr);
    Phase phase() const { return phase_; }
    int remainingSeconds() const { return remainingSeconds_; }
    int completedCycles() const { return completedCycles_; }
    bool isRunning() const;
    QString phaseName() const;
    void start();
    void pause();
    void skip();
    void setDurations(int workMinutes, int shortBreakMinutes, int longBreakMinutes);

signals:
    void tick(int remainingSeconds);
    void phaseChanged(PomodoroController::Phase phase, const QString &name);
    void completed(PomodoroController::Phase completedPhase);
    void runningChanged(bool running);

private:
    int durationFor(Phase phase) const;
    void advancePhase();
    void persist() const;
    void resetDailyCount();
    static QString phaseKey(Phase phase);
    static Phase phaseFromKey(const QString &key);

    ConfigManager *configManager_ = nullptr;
    QTimer *timer_ = nullptr;
    Phase phase_ = Phase::Work;
    int remainingSeconds_ = 25 * 60;
    int completedCycles_ = 0;
};
