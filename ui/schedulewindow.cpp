#include "schedulewindow.h"

#include "../core/data/schedulerepository.h"
#include "../core/schedule/courseremindercontroller.h"
#include "../core/theme.h"

#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kTimeAxisWidth = 76;
constexpr int kDayWidth = 148;
constexpr int kHeaderHeight = 58;
constexpr qreal kPixelsPerMinute = 1.08;

const QStringList kWeekdays = {
    QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
    QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
    QStringLiteral("周日")
};

const QVector<QColor> kCourseColors = {
    QColor(75, 125, 187), QColor(133, 105, 184), QColor(190, 103, 88),
    QColor(184, 137, 69), QColor(65, 142, 145), QColor(166, 92, 126)
};

int minutesOfDay(const QTime &time)
{
    return time.hour() * 60 + time.minute();
}

QColor courseColor(const QString &name)
{
    return kCourseColors.at(qHash(name) % static_cast<size_t>(kCourseColors.size()));
}

struct PositionedCourse
{
    Course course;
    int lane = 0;
    int laneCount = 1;
};

QVector<PositionedCourse> positionCourses(QVector<Course> courses)
{
    std::sort(courses.begin(), courses.end(), [](const Course &left, const Course &right) {
        if (left.startTime != right.startTime) return left.startTime < right.startTime;
        if (left.endTime != right.endTime) return left.endTime < right.endTime;
        return left.name < right.name;
    });
    QVector<PositionedCourse> positioned;
    for (int groupStart = 0; groupStart < courses.size();) {
        int groupEnd = groupStart + 1;
        QTime latestEnd = courses.at(groupStart).endTime;
        while (groupEnd < courses.size() && courses.at(groupEnd).startTime < latestEnd) {
            latestEnd = qMax(latestEnd, courses.at(groupEnd).endTime);
            ++groupEnd;
        }
        QVector<QTime> laneEnds;
        const int outputStart = positioned.size();
        for (int index = groupStart; index < groupEnd; ++index) {
            int lane = 0;
            while (lane < laneEnds.size() && laneEnds.at(lane) > courses.at(index).startTime)
                ++lane;
            if (lane == laneEnds.size()) laneEnds.append(courses.at(index).endTime);
            else laneEnds[lane] = courses.at(index).endTime;
            positioned.append({courses.at(index), lane, 1});
        }
        for (int index = outputStart; index < positioned.size(); ++index)
            positioned[index].laneCount = laneEnds.size();
        groupStart = groupEnd;
    }
    return positioned;
}

QString scheduleStyle()
{
    QString style = QStringLiteral(R"(
        QWidget#ScheduleWindow { background: #171b21; color: #eef1f2; }
        QLabel#scheduleTitle { color: #f6f1e9; font-size: 24px; font-weight: 600; }
        QLabel#scheduleDate { color: #aeb7be; font-size: 13px; }
        QLabel#scheduleStatus { color: #81909a; font-size: 12px; }
        QPushButton { background: #252c34; color: #e9edef; border: 1px solid #46515d;
                      border-radius: 7px; padding: 7px 13px; }
        QPushButton:hover { background: #343e49; border-color: #7f919e; }
        QPushButton#currentWeek { color: #f2dec1; border-color: #8a7458; }
        QScrollArea { background: #171b21; border: 1px solid #343d47; border-radius: 9px; }
        QScrollBar:vertical { background: #171b21; width: 10px; }
        QScrollBar::handle:vertical { background: #47535e; border-radius: 5px; min-height: 36px; }
        QScrollBar:horizontal { background: #171b21; height: 10px; }
        QScrollBar::handle:horizontal { background: #47535e; border-radius: 5px; min-width: 36px; }
    )");
    if (ThemeManager::instance()->isLight()) {
        style += QStringLiteral(R"(
            QWidget#ScheduleWindow { background:#f4f8ff; color:#253552; }
            QLabel#scheduleTitle { color:#253552; }
            QLabel#scheduleDate { color:#687895; }
            QLabel#scheduleStatus { color:#75849d; }
            QPushButton { background:#e2ecfb; color:#253552; border-color:#b6c8e3; }
            QPushButton:hover { background:#d6e2f5; border-color:#8b78c5; }
            QPushButton#currentWeek { color:#6d5ba6; border-color:#8b78c5; }
            QScrollArea { background:#f4f8ff; border-color:#c1d0e6; }
            QScrollBar:vertical, QScrollBar:horizontal { background:#edf3fc; }
            QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background:#aebed7; }
        )");
    }
    return style;
}

} // namespace

class WeeklyScheduleCanvas : public QWidget
{
public:
    explicit WeeklyScheduleCanvas(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumWidth(kTimeAxisWidth + 7 * kDayWidth);
    }

    void setSchedule(const Semester &semester, const QVector<Course> &courses, int week)
    {
        semester_ = semester;
        courses_ = courses;
        week_ = week;
        startMinute_ = 8 * 60;
        endMinute_ = 21 * 60;
        for (const Course &course : courses_) {
            if (!CourseReminderController::occursInWeek(course, week_)) continue;
            startMinute_ = qMin(startMinute_, (minutesOfDay(course.startTime) / 30) * 30);
            endMinute_ = qMax(endMinute_, ((minutesOfDay(course.endTime) + 29) / 30) * 30);
        }
        startMinute_ = qMax(0, startMinute_ - 30);
        endMinute_ = qMin(24 * 60, endMinute_ + 30);
        setMinimumHeight(kHeaderHeight + qRound((endMinute_ - startMinute_) * kPixelsPerMinute));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const bool light = ThemeManager::instance()->isLight();
        painter.fillRect(rect(), light ? QColor(247, 250, 255) : QColor(23, 27, 33));
        const qreal usableWidth = qMax<qreal>(7 * kDayWidth, width() - kTimeAxisWidth);
        const qreal dayWidth = usableWidth / 7.0;
        const QDate monday = semester_.startDate.addDays((week_ - 1) * 7);

        painter.fillRect(QRectF(0, 0, width(), kHeaderHeight),
                         light ? QColor(229, 237, 249) : QColor(29, 35, 42));
        painter.setPen(light ? QColor(187, 202, 224) : QColor(59, 69, 79));
        painter.drawLine(kTimeAxisWidth, 0, kTimeAxisWidth, height());
        for (int day = 0; day < 7; ++day) {
            const qreal left = kTimeAxisWidth + day * dayWidth;
            const QDate date = monday.addDays(day);
            if (date == QDate::currentDate())
                painter.fillRect(QRectF(left + 1, 0, dayWidth - 2, height()), QColor(67, 82, 91, 38));
            painter.setPen(light ? QColor(187, 202, 224) : QColor(59, 69, 79));
            painter.drawLine(QPointF(left, 0), QPointF(left, height()));
            painter.setPen(date == QDate::currentDate()
                               ? (light ? QColor(109, 91, 166) : QColor(242, 222, 193))
                               : (light ? QColor(43, 59, 87) : QColor(215, 222, 226)));
            QFont dayFont = font();
            dayFont.setPointSize(10);
            dayFont.setWeight(date == QDate::currentDate() ? QFont::DemiBold : QFont::Normal);
            painter.setFont(dayFont);
            painter.drawText(QRectF(left + 4, 7, dayWidth - 8, 23), Qt::AlignCenter,
                             kWeekdays.at(day));
            painter.setPen(light ? QColor(103, 120, 148) : QColor(139, 151, 159));
            painter.drawText(QRectF(left + 4, 29, dayWidth - 8, 20), Qt::AlignCenter,
                             date.toString(QStringLiteral("M/d")));
        }

        QFont timeFont = font();
        timeFont.setPointSize(9);
        painter.setFont(timeFont);
        for (int minute = startMinute_; minute <= endMinute_; minute += 30) {
            const qreal y = kHeaderHeight + (minute - startMinute_) * kPixelsPerMinute;
            const bool wholeHour = minute % 60 == 0;
            painter.setPen(light ? (wholeHour ? QColor(196, 208, 226) : QColor(222, 230, 242))
                                 : (wholeHour ? QColor(66, 76, 87) : QColor(49, 57, 66)));
            painter.drawLine(QPointF(kTimeAxisWidth, y), QPointF(width(), y));
            if (wholeHour) {
                painter.setPen(light ? QColor(103, 120, 148) : QColor(132, 143, 151));
                painter.drawText(QRectF(6, y - 10, kTimeAxisWidth - 13, 20),
                                 Qt::AlignRight | Qt::AlignVCenter,
                                 QTime(minute / 60 % 24, 0).toString(QStringLiteral("HH:mm")));
            }
        }

        bool hasCourse = false;
        for (int day = 1; day <= 7; ++day) {
            QVector<Course> dayCourses;
            for (const Course &course : courses_) {
                if (course.weekday == day && CourseReminderController::occursInWeek(course, week_))
                    dayCourses.append(course);
            }
            for (const PositionedCourse &positioned : positionCourses(dayCourses)) {
                hasCourse = true;
                const qreal columnLeft = kTimeAxisWidth + (day - 1) * dayWidth;
                const qreal laneWidth = (dayWidth - 7) / positioned.laneCount;
                const qreal x = columnLeft + 4 + positioned.lane * laneWidth;
                const qreal y = kHeaderHeight
                    + (minutesOfDay(positioned.course.startTime) - startMinute_) * kPixelsPerMinute + 2;
                const qreal cardHeight = qMax<qreal>(28,
                    positioned.course.startTime.secsTo(positioned.course.endTime) / 60.0
                        * kPixelsPerMinute - 4);
                const QRectF cardRect(x, y, laneWidth - 3, cardHeight);
                QColor color = courseColor(positioned.course.name);
                painter.setPen(QPen(color.lighter(145), 1.1));
                painter.setBrush(color);
                painter.drawRoundedRect(cardRect, 7, 7);
                painter.save();
                painter.setClipRect(cardRect.adjusted(1, 1, -1, -1));
                painter.setPen(QColor(250, 250, 248));
                QFont courseFont = font();
                courseFont.setPointSize(9);
                courseFont.setWeight(QFont::DemiBold);
                painter.setFont(courseFont);
                QString text = positioned.course.name;
                if (!positioned.course.room.isEmpty()) text += QStringLiteral("\n@%1").arg(positioned.course.room);
                if (!positioned.course.teacher.isEmpty()) text += QStringLiteral("\n%1").arg(positioned.course.teacher);
                if (cardHeight >= 82)
                    text += QStringLiteral("\n%1–%2")
                        .arg(positioned.course.startTime.toString(QStringLiteral("HH:mm")),
                             positioned.course.endTime.toString(QStringLiteral("HH:mm")));
                painter.drawText(cardRect.adjusted(7, 5, -5, -4),
                                 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
                painter.restore();
            }
        }

        const QDate today = QDate::currentDate();
        if (today >= monday && today <= monday.addDays(6)) {
            const int nowMinute = minutesOfDay(QTime::currentTime());
            if (nowMinute >= startMinute_ && nowMinute <= endMinute_) {
                const qreal y = kHeaderHeight + (nowMinute - startMinute_) * kPixelsPerMinute;
                painter.setPen(QPen(QColor(232, 116, 103), 1.5));
                painter.drawLine(QPointF(kTimeAxisWidth, y), QPointF(width(), y));
                painter.setBrush(QColor(232, 116, 103));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(kTimeAxisWidth, y), 4, 4);
            }
        }
        if (!hasCourse) {
            painter.setPen(light ? QColor(103, 120, 148) : QColor(128, 139, 148));
            QFont emptyFont = font();
            emptyFont.setPointSize(12);
            painter.setFont(emptyFont);
            painter.drawText(QRectF(kTimeAxisWidth, kHeaderHeight, width() - kTimeAxisWidth,
                                    height() - kHeaderHeight),
                             Qt::AlignCenter, QStringLiteral("这一周没有课程安排"));
        }
    }

private:
    Semester semester_;
    QVector<Course> courses_;
    int week_ = 1;
    int startMinute_ = 450;
    int endMinute_ = 1290;
};

ScheduleWindow::ScheduleWindow(ScheduleRepository *repository, QWidget *parent)
    : QWidget(parent, Qt::Window), repository_(repository)
{
    setObjectName(QStringLiteral("ScheduleWindow"));
    setWindowTitle(QStringLiteral("冰织 · 周课表"));
    resize(1180, 760);
    setMinimumSize(760, 520);
    setStyleSheet(scheduleStyle());
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ScheduleWindow::applyTheme);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 20);
    root->setSpacing(12);
    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("scheduleTitle"));
    dateLabel_ = new QLabel(this);
    dateLabel_->setObjectName(QStringLiteral("scheduleDate"));
    titles->addWidget(titleLabel_);
    titles->addWidget(dateLabel_);
    header->addLayout(titles, 1);
    previousButton_ = new QPushButton(QStringLiteral("‹ 上一周"), this);
    currentButton_ = new QPushButton(QStringLiteral("本周"), this);
    currentButton_->setObjectName(QStringLiteral("currentWeek"));
    nextButton_ = new QPushButton(QStringLiteral("下一周 ›"), this);
    header->addWidget(previousButton_);
    header->addWidget(currentButton_);
    header->addWidget(nextButton_);
    root->addLayout(header);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("scheduleStatus"));
    root->addWidget(statusLabel_);
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    canvas_ = new WeeklyScheduleCanvas(scrollArea);
    scrollArea->setWidget(canvas_);
    root->addWidget(scrollArea, 1);

    connect(previousButton_, &QPushButton::clicked, this, [this] { changeWeek(-1); });
    connect(currentButton_, &QPushButton::clicked, this, &ScheduleWindow::showCurrentWeek);
    connect(nextButton_, &QPushButton::clicked, this, [this] { changeWeek(1); });
    auto *clock = new QTimer(canvas_);
    clock->setInterval(60000);
    connect(clock, &QTimer::timeout, canvas_, qOverload<>(&QWidget::update));
    clock->start();
    refresh();
}

void ScheduleWindow::applyTheme()
{
    setStyleSheet(scheduleStyle());
    if (canvas_)
        canvas_->update();
}

void ScheduleWindow::refresh()
{
    if (!repository_) return;
    const auto semesterResult = repository_->activeSemester();
    if (!semesterResult.success || !semesterResult.value.has_value()) {
        titleLabel_->setText(QStringLiteral("尚未创建学期"));
        dateLabel_->clear();
        statusLabel_->setText(semesterResult.success ? QStringLiteral("先在专注页创建学期。")
                                                     : semesterResult.error);
        previousButton_->setEnabled(false);
        currentButton_->setEnabled(false);
        nextButton_->setEnabled(false);
        canvas_->hide();
        return;
    }
    const qint64 previousSemesterId = semester_.id;
    semester_ = semesterResult.value.value();
    const auto courseResult = repository_->listCourses(semester_.id);
    courses_ = courseResult.success ? courseResult.value : QVector<Course>();
    if (previousSemesterId != semester_.id || displayedWeek_ < 1
        || displayedWeek_ > semester_.totalWeeks)
        showCurrentWeek();
    else
        updateView();
    if (!courseResult.success) statusLabel_->setText(courseResult.error);
}

void ScheduleWindow::changeWeek(int offset)
{
    displayedWeek_ = qBound(1, displayedWeek_ + offset, semester_.totalWeeks);
    updateView();
}

void ScheduleWindow::showCurrentWeek()
{
    const int currentWeek = CourseReminderController::weekForDate(semester_, QDate::currentDate());
    displayedWeek_ = qBound(1, currentWeek, semester_.totalWeeks);
    updateView();
}

void ScheduleWindow::updateView()
{
    canvas_->show();
    const QDate monday = semester_.startDate.addDays((displayedWeek_ - 1) * 7);
    const QDate sunday = monday.addDays(6);
    titleLabel_->setText(QStringLiteral("第 %1 周 · %2").arg(displayedWeek_).arg(semester_.name));
    dateLabel_->setText(QStringLiteral("%1 — %2")
                            .arg(monday.toString(QStringLiteral("yyyy/M/d")),
                                 sunday.toString(QStringLiteral("yyyy/M/d"))));
    statusLabel_->setText(QStringLiteral("仅显示本周实际开课的课程 · 红线表示当前时间"));
    previousButton_->setEnabled(displayedWeek_ > 1);
    nextButton_->setEnabled(displayedWeek_ < semester_.totalWeeks);
    currentButton_->setEnabled(true);
    canvas_->setSchedule(semester_, courses_, displayedWeek_);
}
