#pragma once

#include "viewmodels/cmdtlm/CmdTlmViewModel.h"

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

namespace OpenC3::UI::Views {

/// CMD/TLM Editor — browse and edit remote COSMOS command/telemetry definition files.
class CmdTlmView final : public QWidget {
    Q_OBJECT
public:
    explicit CmdTlmView(ViewModels::CmdTlmViewModel& vm,
                        QWidget*                     parent = nullptr);

private slots:
    void onListClicked();
    void onFileItemDoubleClicked(QListWidgetItem* item);
    void onSaveClicked();

private:
    void setupUi();
    void bindViewModel();

    ViewModels::CmdTlmViewModel& vm_;

    QLabel*       connectionHint_{nullptr};
    QLineEdit*    pathEdit_{nullptr};
    QPushButton*  listBtn_{nullptr};
    QListWidget*  fileList_{nullptr};
    QLabel*       fileLabel_{nullptr};
    QTextEdit*    editor_{nullptr};
    QPushButton*  saveBtn_{nullptr};
    QLabel*       statusLabel_{nullptr};

    QString currentDir_;
    QString currentFile_;
};

} // namespace OpenC3::UI::Views
