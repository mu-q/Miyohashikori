#pragma once

#include <QDialog>

class ConfigManager;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ConfigManager *configManager, QWidget *parent = nullptr);

private:
    void chooseReferenceAudio();
    void saveSettings();

    ConfigManager *configManager_ = nullptr;
    QLineEdit *llmEndpoint_ = nullptr;
    QLineEdit *llmApiKey_ = nullptr;
    QLineEdit *llmModel_ = nullptr;
    QCheckBox *voiceEnabled_ = nullptr;
    QSpinBox *volumePercent_ = nullptr;
    QCheckBox *ttsEnabled_ = nullptr;
    QLineEdit *ttsEndpoint_ = nullptr;
    QLineEdit *ttsReferenceAudio_ = nullptr;
    QLineEdit *ttsReferenceText_ = nullptr;
    QDoubleSpinBox *ttsSpeed_ = nullptr;
};
