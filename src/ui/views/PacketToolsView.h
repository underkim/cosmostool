#pragma once

#include "viewmodels/packettools/PacketToolsViewModel.h"

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>

namespace OpenC3::UI::Views {

/// Packet Tools — browse and inspect COSMOS packet log files.
class PacketToolsView final : public QWidget {
    Q_OBJECT
public:
    explicit PacketToolsView(ViewModels::PacketToolsViewModel& vm,
                             QWidget*                          parent = nullptr);

private slots:
    void onLogFileSelected(QListWidgetItem* item);
    void onAnalyzeClicked();

private:
    void setupUi();
    void bindViewModel();

    ViewModels::PacketToolsViewModel& vm_;

    QLabel*       connectionHint_{nullptr};
    QPushButton*  refreshBtn_{nullptr};
    QListWidget*  logList_{nullptr};
    QTextEdit*    contentView_{nullptr};
    QLineEdit*    filterEdit_{nullptr};
    QPushButton*  analyzeBtn_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel*       statusLabel_{nullptr};
    QLabel*       analysisLabel_{nullptr};
};

} // namespace OpenC3::UI::Views
