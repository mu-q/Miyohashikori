#pragma once

#include "../core/data/models.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QToolButton;
class ScheduleRepository;

class CourseSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit CourseSidebar(ScheduleRepository *repository, QWidget *parent = nullptr);

public slots:
    void refresh();

signals:
    void scheduleChanged();
    void statusMessage(const QString &message, bool isError);

private:
    void addSemester();
    void editSemester();
    void deleteSemester();
    void addCourse();
    void editSelectedCourse();
    void deleteSelectedCourse();
    void importSpreadsheet();
    void refreshCourses();
    qint64 selectedSemesterId() const;
    int selectedWeekday() const;

    ScheduleRepository *repository_ = nullptr;
    QComboBox *semesterBox_ = nullptr;
    QComboBox *dayBox_ = nullptr;
    QLabel *weekLabel_ = nullptr;
    QLabel *nextCourseLabel_ = nullptr;
    QListWidget *courseList_ = nullptr;
    QToolButton *semesterMenuButton_ = nullptr;
    QVector<Semester> semesters_;
    QVector<Course> visibleCourses_;
};
