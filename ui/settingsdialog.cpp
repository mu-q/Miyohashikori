#include "settingsdialog.h"

#include "../core/config/appconfig.h"
#include "../core/config/configmanager.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

namespace {

bool isHttpEndpoint(const QString &text)
{
    const QUrl url(text.trimmed());
    return url.isValid() && !url.host().isEmpty()
           && (url.scheme() == QStringLiteral("http")
               || url.scheme() == QStringLiteral("https"));
}

} // namespace

SettingsDialog::SettingsDialog(ConfigManager *configManager, QWidget *parent)
    : QDialog(parent), configManager_(configManager)
{
    setWindowTitle(QStringLiteral("冰织设置"));
    setMinimumWidth(560);
    const AppConfig &config = configManager_->config();

    auto *root = new QVBoxLayout(this);
    auto *hint = new QLabel(QStringLiteral("API Key 只保存在本机 ~/.hyori/config.json。"), this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *llmGroup = new QGroupBox(QStringLiteral("AI 对话"), this);
    auto *llmForm = new QFormLayout(llmGroup);
    llmEndpoint_ = new QLineEdit(config.llmEndpoint, llmGroup);
    llmModel_ = new QLineEdit(config.llmModel, llmGroup);
    llmApiKey_ = new QLineEdit(config.llmApiKey, llmGroup);
    llmApiKey_->setEchoMode(QLineEdit::Password);
    auto *keyRow = new QWidget(llmGroup);
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(llmApiKey_, 1);
    auto *showKey = new QCheckBox(QStringLiteral("显示"), keyRow);
    keyLayout->addWidget(showKey);
    connect(showKey, &QCheckBox::toggled, this, [this](bool shown) {
        llmApiKey_->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
    });
    llmForm->addRow(QStringLiteral("接口地址"), llmEndpoint_);
    llmForm->addRow(QStringLiteral("模型"), llmModel_);
    llmForm->addRow(QStringLiteral("API Key"), keyRow);
    root->addWidget(llmGroup);

    auto *voiceGroup = new QGroupBox(QStringLiteral("语音"), this);
    auto *voiceForm = new QFormLayout(voiceGroup);
    voiceEnabled_ = new QCheckBox(QStringLiteral("播放回复语音"), voiceGroup);
    voiceEnabled_->setChecked(config.voiceEnabled);
    volumePercent_ = new QSpinBox(voiceGroup);
    volumePercent_->setRange(0, 100);
    volumePercent_->setSuffix(QStringLiteral("%"));
    volumePercent_->setValue(qRound(config.volume * 100.0));
    ttsEnabled_ = new QCheckBox(QStringLiteral("优先使用 GPT-SoVITS"), voiceGroup);
    ttsEnabled_->setChecked(config.ttsEnabled);
    ttsEndpoint_ = new QLineEdit(config.ttsEndpoint, voiceGroup);
    ttsReferenceAudio_ = new QLineEdit(config.ttsReferenceAudioPath, voiceGroup);
    auto *audioRow = new QWidget(voiceGroup);
    auto *audioLayout = new QHBoxLayout(audioRow);
    audioLayout->setContentsMargins(0, 0, 0, 0);
    audioLayout->addWidget(ttsReferenceAudio_, 1);
    auto *browse = new QPushButton(QStringLiteral("浏览…"), audioRow);
    audioLayout->addWidget(browse);
    ttsReferenceText_ = new QLineEdit(config.ttsReferenceText, voiceGroup);
    ttsSpeed_ = new QDoubleSpinBox(voiceGroup);
    ttsSpeed_->setRange(0.5, 2.0);
    ttsSpeed_->setSingleStep(0.05);
    ttsSpeed_->setValue(config.ttsSpeedFactor);
    voiceForm->addRow(voiceEnabled_);
    voiceForm->addRow(QStringLiteral("音量"), volumePercent_);
    voiceForm->addRow(ttsEnabled_);
    voiceForm->addRow(QStringLiteral("TTS 接口"), ttsEndpoint_);
    voiceForm->addRow(QStringLiteral("参考音频"), audioRow);
    voiceForm->addRow(QStringLiteral("参考台词"), ttsReferenceText_);
    voiceForm->addRow(QStringLiteral("语速"), ttsSpeed_);
    root->addWidget(voiceGroup);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    root->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, &SettingsDialog::chooseReferenceAudio);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::chooseReferenceAudio()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 GPT-SoVITS 参考音频"), ttsReferenceAudio_->text(),
        QStringLiteral("音频文件 (*.wav *.ogg *.mp3 *.flac);;所有文件 (*.*)"));
    if (!path.isEmpty())
        ttsReferenceAudio_->setText(path);
}

void SettingsDialog::saveSettings()
{
    if (!isHttpEndpoint(llmEndpoint_->text())) {
        QMessageBox::warning(this, QStringLiteral("设置有误"),
                             QStringLiteral("请填写有效的 HTTP 或 HTTPS AI 接口地址。"));
        llmEndpoint_->setFocus();
        return;
    }
    if (llmModel_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("设置有误"),
                             QStringLiteral("模型名称不能为空。"));
        llmModel_->setFocus();
        return;
    }
    if (ttsEnabled_->isChecked() && !isHttpEndpoint(ttsEndpoint_->text())) {
        QMessageBox::warning(this, QStringLiteral("设置有误"),
                             QStringLiteral("启用 GPT-SoVITS 时需要有效的 TTS 接口地址。"));
        ttsEndpoint_->setFocus();
        return;
    }

    AppConfig config = configManager_->config();
    config.llmEndpoint = llmEndpoint_->text().trimmed();
    config.llmApiKey = llmApiKey_->text().trimmed();
    config.llmModel = llmModel_->text().trimmed();
    config.voiceEnabled = voiceEnabled_->isChecked();
    config.volume = volumePercent_->value() / 100.0;
    config.ttsEnabled = ttsEnabled_->isChecked();
    config.ttsEndpoint = ttsEndpoint_->text().trimmed();
    config.ttsReferenceAudioPath = ttsReferenceAudio_->text().trimmed();
    config.ttsReferenceText = ttsReferenceText_->text().trimmed();
    config.ttsSpeedFactor = ttsSpeed_->value();
    configManager_->setConfig(config);
    if (!configManager_->save()) {
        QMessageBox::critical(this, QStringLiteral("保存失败"),
                              QStringLiteral("无法写入本地配置文件，请检查目录权限。"));
        return;
    }
    accept();
}
