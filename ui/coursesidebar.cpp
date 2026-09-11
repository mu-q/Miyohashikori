#include "coursesidebar.h"

#include "../core/data/schedulerepository.h"
#include "../core/schedule/courseremindercontroller.h"
#include "../core/schedule/spreadsheetscheduleimporter.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const QStringList kWeekdays = {QStringLiteral("周一"), QStringLiteral("周二"),
                               QStringLiteral("周三"), QStringLiteral("周四"),
                               QStringLiteral("周五"), QStringLiteral("周六"),
                               QStringLiteral("周日")};

class SemesterDialog : public QDialog
{
public:
    explicit SemesterDialog(QWidget *parent, const Semester *semester = nullptr)
        : QDialog(parent), name_(new QLineEdit(this)), start_(new QDateEdit(this)),
          weeks_(new QSpinBox(this))
    {
        setWindowTitle(semester ? QStringLiteral("编辑学期") : QStringLiteral("新建学期"));
        setMinimumWidth(340);
        start_->setCalendarPopup(true);
        weeks_->setRange(1, 40);
        if (semester) {
            name_->setText(semester->name);
            start_->setDate(semester->startDate);
            weeks_->setValue(semester->totalWeeks);
        } else {
            const QDate today = QDate::currentDate();
            name_->setText(QStringLiteral("%1 年%2季学期")
                               .arg(today.year()).arg(today.month() < 7 ? QStringLiteral("春") : QStringLiteral("秋")));
            start_->setDate(today.addDays(1 - today.dayOfWeek()));
            weeks_->setValue(20);
        }
        auto *form = new QFormLayout(this);
        form->addRow(QStringLiteral("学期名称"), name_);
        form->addRow(QStringLiteral("第一周周一"), start_);
        form->addRow(QStringLiteral("总周数"), weeks_);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (name_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("无法保存"), QStringLiteral("请填写学期名称。"));
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }
    QString name() const { return name_->text().trimmed(); }
    QDate startDate() const { return start_->date(); }
    int totalWeeks() const { return weeks_->value(); }
private:
    QLineEdit *name_;
    QDateEdit *start_;
    QSpinBox *weeks_;
};

class CourseDialog : public QDialog
{
public:
    CourseDialog(qint64 semesterId, int totalWeeks, QWidget *parent, const Course *course = nullptr)
        : QDialog(parent), course_(course ? *course : Course{})
    {
        course_.semesterId = semesterId;
        setWindowTitle(course ? QStringLiteral("编辑课程") : QStringLiteral("添加课程"));
        setMinimumWidth(380);
        name_ = new QLineEdit(course_.name, this);
        teacher_ = new QLineEdit(course_.teacher, this);
        room_ = new QLineEdit(course_.room, this);
        weekday_ = new QComboBox(this);
        weekday_->addItems(kWeekdays);
        weekday_->setCurrentIndex(qBound(0, course_.weekday - 1, 6));
        start_ = new QTimeEdit(course_.startTime.isValid() ? course_.startTime : QTime(8, 0), this);
        end_ = new QTimeEdit(course_.endTime.isValid() ? course_.endTime : QTime(9, 40), this);
        start_->setDisplayFormat(QStringLiteral("HH:mm"));
        end_->setDisplayFormat(QStringLiteral("HH:mm"));
        startWeek_ = new QSpinBox(this);
        endWeek_ = new QSpinBox(this);
        startWeek_->setRange(1, totalWeeks);
        endWeek_->setRange(1, totalWeeks);
        startWeek_->setValue(course ? course_.startWeek : 1);
        endWeek_->setValue(course ? qMin(course_.endWeek, totalWeeks) : totalWeeks);
        pattern_ = new QComboBox(this);
        pattern_->addItems({QStringLiteral("每周"), QStringLiteral("单周"), QStringLiteral("双周")});
        pattern_->setCurrentIndex(static_cast<int>(course_.weekPattern));

        auto *form = new QFormLayout(this);
        form->addRow(QStringLiteral("课程名称"), name_);
        form->addRow(QStringLiteral("教师"), teacher_);
        form->addRow(QStringLiteral("教室"), room_);
        form->addRow(QStringLiteral("星期"), weekday_);
        auto *times = new QHBoxLayout;
        times->addWidget(start_); times->addWidget(new QLabel(QStringLiteral("至"))); times->addWidget(end_);
        form->addRow(QStringLiteral("时间"), times);
        auto *weeks = new QHBoxLayout;
        weeks->addWidget(startWeek_); weeks->addWidget(new QLabel(QStringLiteral("至"))); weeks->addWidget(endWeek_);
        form->addRow(QStringLiteral("周次"), weeks);
        form->addRow(QStringLiteral("重复"), pattern_);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (name_->text().trimmed().isEmpty() || start_->time() >= end_->time()
                || startWeek_->value() > endWeek_->value()) {
                QMessageBox::warning(this, QStringLiteral("无法保存"), QStringLiteral("请检查课程名称、时间与周次。"));
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }
    Course course() const
    {
        Course result = course_;
        result.name = name_->text().trimmed(); result.teacher = teacher_->text().trimmed();
        result.room = room_->text().trimmed(); result.weekday = weekday_->currentIndex() + 1;
        result.startTime = start_->time(); result.endTime = end_->time();
        result.startWeek = startWeek_->value(); result.endWeek = endWeek_->value();
        result.weekPattern = static_cast<CourseWeekPattern>(pattern_->currentIndex());
        return result;
    }
private:
    Course course_;
    QLineEdit *name_; QLineEdit *teacher_; QLineEdit *room_;
    QComboBox *weekday_; QTimeEdit *start_; QTimeEdit *end_;
    QSpinBox *startWeek_; QSpinBox *endWeek_; QComboBox *pattern_;
};

QString patternText(CourseWeekPattern pattern)
{
    if (pattern == CourseWeekPattern::OddWeeks) return QStringLiteral("单周");
    if (pattern == CourseWeekPattern::EvenWeeks) return QStringLiteral("双周");
    return QStringLiteral("每周");
}

} // namespace

CourseSidebar::CourseSidebar(ScheduleRepository *repository, QWidget *parent)
    : QWidget(parent), repository_(repository)
{
    setObjectName(QStringLiteral("courseSidebar"));
    setMinimumWidth(290);
    setMaximumWidth(340);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 17, 18, 16);
    root->setSpacing(10);

    auto *heading = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("课程表"), this);
    title->setObjectName(QStringLiteral("sidebarTitle"));
    semesterMenuButton_ = new QToolButton(this);
    semesterMenuButton_->setText(QStringLiteral("学期 ···"));
    semesterMenuButton_->setPopupMode(QToolButton::InstantPopup);
    auto *semesterMenu = new QMenu(semesterMenuButton_);
    semesterMenu->addAction(QStringLiteral("新建学期"), this, &CourseSidebar::addSemester);
    semesterMenu->addAction(QStringLiteral("编辑当前学期"), this, &CourseSidebar::editSemester);
    semesterMenu->addAction(QStringLiteral("删除当前学期"), this, &CourseSidebar::deleteSemester);
    semesterMenuButton_->setMenu(semesterMenu);
    heading->addWidget(title); heading->addStretch(); heading->addWidget(semesterMenuButton_);
    root->addLayout(heading);

    semesterBox_ = new QComboBox(this);
    root->addWidget(semesterBox_);
    weekLabel_ = new QLabel(this);
    weekLabel_->setObjectName(QStringLiteral("scheduleMeta"));
    root->addWidget(weekLabel_);
    nextCourseLabel_ = new QLabel(QStringLiteral("添加学期后，下一节课会出现在这里。"), this);
    nextCourseLabel_->setObjectName(QStringLiteral("nextCourse"));
    nextCourseLabel_->setWordWrap(true);
    root->addWidget(nextCourseLabel_);

    dayBox_ = new QComboBox(this);
    dayBox_->addItem(QStringLiteral("今天"), 0);
    for (int day = 1; day <= 7; ++day)
        dayBox_->addItem(kWeekdays.at(day - 1), day);
    root->addWidget(dayBox_);
    courseList_ = new QListWidget(this);
    courseList_->setObjectName(QStringLiteral("courseList"));
    courseList_->setAlternatingRowColors(false);
    courseList_->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(courseList_, 1);

    auto *courseActions = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("＋ 课程"), this);
    auto *edit = new QPushButton(QStringLiteral("编辑"), this);
    auto *remove = new QPushButton(QStringLiteral("删除"), this);
    for (QPushButton *button : {add, edit, remove})
        button->setObjectName(QStringLiteral("smallCourseAction"));
    courseActions->addWidget(add); courseActions->addWidget(edit); courseActions->addWidget(remove);
    root->addLayout(courseActions);
    auto *import = new QPushButton(QStringLiteral("导入 Excel / CSV"), this);
    import->setObjectName(QStringLiteral("importSpreadsheet"));
    import->setToolTip(SpreadsheetScheduleImporter::columnHelp());
    root->addWidget(import);

    connect(semesterBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || !repository_) return;
        const auto result = repository_->setActiveSemester(selectedSemesterId());
        if (!result.success) emit statusMessage(result.error, true);
        refreshCourses(); emit scheduleChanged();
    });
    connect(dayBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { refreshCourses(); });
    connect(add, &QPushButton::clicked, this, &CourseSidebar::addCourse);
    connect(edit, &QPushButton::clicked, this, &CourseSidebar::editSelectedCourse);
    connect(remove, &QPushButton::clicked, this, &CourseSidebar::deleteSelectedCourse);
    connect(import, &QPushButton::clicked, this, &CourseSidebar::importSpreadsheet);
    connect(courseList_, &QListWidget::itemDoubleClicked, this, [this] { editSelectedCourse(); });
    auto *clock = new QTimer(this);
    clock->setInterval(60000);
    connect(clock, &QTimer::timeout, this, &CourseSidebar::refreshCourses);
    clock->start();
    refresh();
}

void CourseSidebar::refresh()
{
    semesterBox_->blockSignals(true);
    semesterBox_->clear(); semesters_.clear();
    const auto result = repository_ ? repository_->listSemesters() : DataResult<QVector<Semester>>::fail(QStringLiteral("课程表不可用"));
    if (result.success) {
        semesters_ = result.value;
        int activeIndex = -1;
        for (int i = 0; i < semesters_.size(); ++i) {
            semesterBox_->addItem(semesters_.at(i).name, semesters_.at(i).id);
            if (semesters_.at(i).active) activeIndex = i;
        }
        semesterBox_->setCurrentIndex(activeIndex >= 0 ? activeIndex : (semesters_.isEmpty() ? -1 : 0));
    }
    semesterBox_->blockSignals(false);
    refreshCourses();
}

void CourseSidebar::refreshCourses()
{
    courseList_->clear(); visibleCourses_.clear();
    const qint64 semesterId = selectedSemesterId();
    auto it = std::find_if(semesters_.cbegin(), semesters_.cend(), [semesterId](const Semester &s) { return s.id == semesterId; });
    if (it == semesters_.cend()) {
        weekLabel_->setText(QStringLiteral("尚未创建学期"));
        nextCourseLabel_->setText(QStringLiteral("先新建学期，再手动添加或导入课程。"));
        return;
    }
    const Semester semester = *it;
    const int currentWeek = CourseReminderController::weekForDate(semester, QDate::currentDate());
    weekLabel_->setText(currentWeek >= 1 && currentWeek <= semester.totalWeeks
                            ? QStringLiteral("第 %1 / %2 周 · 提前 30、20 分钟提醒").arg(currentWeek).arg(semester.totalWeeks)
                            : QStringLiteral("非教学周 · 共 %1 周").arg(semester.totalWeeks));
    const auto courses = repository_->listCourses(semester.id);
    if (!courses.success) { emit statusMessage(courses.error, true); return; }
    const int weekday = selectedWeekday();
    Course next;
    bool hasNext = false;
    for (const Course &course : courses.value) {
        if (course.weekday == QDate::currentDate().dayOfWeek()
            && CourseReminderController::occursInWeek(course, currentWeek)
            && course.endTime > QTime::currentTime()
            && (!hasNext || course.startTime < next.startTime)) { next = course; hasNext = true; }
        if (course.weekday != weekday) continue;
        visibleCourses_.append(course);
        const QString detail = QStringLiteral("%1–%2  %3\n%4%5 · 第 %6–%7 周 %8")
            .arg(course.startTime.toString(QStringLiteral("HH:mm")), course.endTime.toString(QStringLiteral("HH:mm")), course.name,
                 course.room.isEmpty() ? QStringLiteral("地点待定") : course.room,
                 course.teacher.isEmpty() ? QString() : QStringLiteral(" · %1").arg(course.teacher))
            .arg(course.startWeek).arg(course.endWeek).arg(patternText(course.weekPattern));
        auto *item = new QListWidgetItem(detail, courseList_);
        item->setData(Qt::UserRole, course.id);
        item->setSizeHint(QSize(0, 61));
    }
    if (visibleCourses_.isEmpty())
        courseList_->addItem(QStringLiteral("这一天还没有课程。"));
    if (hasNext) {
        const int minutes = qMax(0, QTime::currentTime().secsTo(next.startTime) / 60);
        nextCourseLabel_->setText(QStringLiteral("下一节 · %1\n%2  %3%4")
            .arg(next.name, next.startTime.toString(QStringLiteral("HH:mm")))
            .arg(minutes > 0 ? QStringLiteral("还有 %1 分钟").arg(minutes) : QStringLiteral("正在上课"),
                 next.room.isEmpty() ? QString() : QStringLiteral(" · %1").arg(next.room)));
    } else {
        nextCourseLabel_->setText(QStringLiteral("今天余下没有课程。"));
    }
}

void CourseSidebar::addSemester()
{
    SemesterDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = repository_->createSemester(dialog.name(), dialog.startDate(), dialog.totalWeeks(), true);
    if (!result.success) { emit statusMessage(result.error, true); return; }
    refresh(); emit scheduleChanged(); emit statusMessage(QStringLiteral("新学期已经建好了。"), false);
}

void CourseSidebar::editSemester()
{
    const qint64 id = selectedSemesterId();
    auto it = std::find_if(semesters_.cbegin(), semesters_.cend(), [id](const Semester &s) { return s.id == id; });
    if (it == semesters_.cend()) return;
    Semester semester = *it;
    SemesterDialog dialog(this, &semester);
    if (dialog.exec() != QDialog::Accepted) return;
    semester.name = dialog.name(); semester.startDate = dialog.startDate(); semester.totalWeeks = dialog.totalWeeks();
    const auto result = repository_->updateSemester(semester);
    if (!result.success) { emit statusMessage(result.error, true); return; }
    refresh(); emit scheduleChanged();
}

void CourseSidebar::deleteSemester()
{
    const qint64 id = selectedSemesterId();
    if (id < 0 || QMessageBox::question(this, QStringLiteral("删除学期"),
        QStringLiteral("删除后，该学期的全部课程与提醒记录也会清除。继续吗？")) != QMessageBox::Yes) return;
    const auto result = repository_->removeSemester(id);
    if (!result.success) { emit statusMessage(result.error, true); return; }
    refresh(); emit scheduleChanged();
}

void CourseSidebar::addCourse()
{
    const qint64 id = selectedSemesterId();
    auto it = std::find_if(semesters_.cbegin(), semesters_.cend(), [id](const Semester &s) { return s.id == id; });
    if (it == semesters_.cend()) { emit statusMessage(QStringLiteral("请先新建一个学期。"), true); return; }
    CourseDialog dialog(id, it->totalWeeks, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = repository_->createCourse(dialog.course());
    if (!result.success) { emit statusMessage(result.error, true); return; }
    refreshCourses(); emit scheduleChanged(); emit statusMessage(QStringLiteral("课程已经加入课表。"), false);
}

void CourseSidebar::editSelectedCourse()
{
    if (!courseList_->currentItem()) return;
    const qint64 courseId = courseList_->currentItem()->data(Qt::UserRole).toLongLong();
    auto courseIt = std::find_if(visibleCourses_.cbegin(), visibleCourses_.cend(), [courseId](const Course &c) { return c.id == courseId; });
    if (courseIt == visibleCourses_.cend()) return;
    auto semesterIt = std::find_if(semesters_.cbegin(), semesters_.cend(), [this](const Semester &s) { return s.id == selectedSemesterId(); });
    if (semesterIt == semesters_.cend()) return;
    Course course = *courseIt;
    CourseDialog dialog(course.semesterId, semesterIt->totalWeeks, this, &course);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = repository_->updateCourse(dialog.course());
    if (!result.success) { emit statusMessage(result.error, true); return; }
    refreshCourses(); emit scheduleChanged();
}

void CourseSidebar::deleteSelectedCourse()
{
    if (!courseList_->currentItem()) return;
    const qint64 id = courseList_->currentItem()->data(Qt::UserRole).toLongLong();
    if (id < 1 || QMessageBox::question(this, QStringLiteral("删除课程"), QStringLiteral("确定删除选中的课程吗？")) != QMessageBox::Yes) return;
    const auto result = repository_->removeCourse(id);
    if (!result.success) { emit statusMessage(result.error, true); return; }
    refreshCourses(); emit scheduleChanged();
}

void CourseSidebar::importSpreadsheet()
{
    const qint64 semesterId = selectedSemesterId();
    const auto semesterIt = std::find_if(semesters_.cbegin(), semesters_.cend(),
        [semesterId](const Semester &semester) { return semester.id == semesterId; });
    if (semesterIt == semesters_.cend()) {
        QMessageBox::information(this, QStringLiteral("请先新建学期"),
            QStringLiteral("Excel/CSV 中只保存课程行。请先新建并选择课程所属的学期。"));
        return;
    }
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("导入课程表"), {},
        QStringLiteral("课程表 (*.xlsx *.xls *.csv *.tsv);;Excel 工作簿 (*.xlsx *.xls);;CSV/TSV 文件 (*.csv *.tsv);;所有文件 (*)"));
    if (file.isEmpty()) return;
    const auto parsed = SpreadsheetScheduleImporter::parseFile(file, semesterIt->totalWeeks);
    if (!parsed.success) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("%1\n\n%2").arg(parsed.error, SpreadsheetScheduleImporter::columnHelp()));
        return;
    }
    const QString summary = QStringLiteral("将向“%1”添加 %2 条上课安排。\n\n现有课程会保留，是否继续？")
        .arg(semesterIt->name).arg(parsed.value.size());
    if (QMessageBox::question(this, QStringLiteral("确认导入"), summary) != QMessageBox::Yes) return;
    const auto imported = repository_->importCourses(semesterId, parsed.value);
    if (!imported.success) { QMessageBox::warning(this, QStringLiteral("导入失败"), imported.error); return; }
    refreshCourses(); emit scheduleChanged();
    emit statusMessage(QStringLiteral("已从表格导入 %1 条课程安排。").arg(imported.value), false);
}

qint64 CourseSidebar::selectedSemesterId() const
{
    return semesterBox_->currentIndex() < 0 ? -1 : semesterBox_->currentData().toLongLong();
}

int CourseSidebar::selectedWeekday() const
{
    const int selected = dayBox_->currentData().toInt();
    return selected == 0 ? QDate::currentDate().dayOfWeek() : selected;
}
