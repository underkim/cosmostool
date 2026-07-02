#pragma once

#include "viewmodels/logviewer/LogViewerViewModel.h"

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>

namespace OpenC3::UI::Views {

/// Log Viewer — real-time streaming of remote log files.
class LogViewerView final : public QWidget {
    Q_OBJECT
public:
    explicit LogViewerView(ViewModels::LogViewerViewModel& vm,
                           QWidget*                        parent = nullptr);

private slots:
    void onStartStopClicked();

private:
    void setupUi();
    void bindViewModel();
    void appendLine(const QString& line);

    ViewModels::LogViewerViewModel& vm_;

    QLabel*       connectionHint_{nullptr};
    QComboBox*    presetCombo_{nullptr};
    QLineEdit*    commandEdit_{nullptr};
    QPushButton*  startStopBtn_{nullptr};
    QPushButton*  clearBtn_{nullptr};
    QCheckBox*    autoScrollCheck_{nullptr};
    QTextEdit*    logView_{nullptr};
    QLabel*       statusLabel_{nullptr};
};

} // namespace OpenC3::UI::Views
