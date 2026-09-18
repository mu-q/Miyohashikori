#pragma once

#include "../core/data/models.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;
class ScheduleRepository;
class WeeklyScheduleCanvas;

class ScheduleWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ScheduleWindow(ScheduleRepository *repository, QWidget *parent = nullptr);
    void applyTheme();

public slots:
    void refresh();

private:
    void changeWeek(int offset);
    void showCurrentWeek();
    void updateView();

    ScheduleRepository *repository_ = nullptr;
    Semester semester_;
    QVector<Course> courses_;
    int displayedWeek_ = 1;
    QLabel *titleLabel_ = nullptr;
    QLabel *dateLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *previousButton_ = nullptr;
    QPushButton *currentButton_ = nullptr;
    QPushButton *nextButton_ = nullptr;
    WeeklyScheduleCanvas *canvas_ = nullptr;
};
