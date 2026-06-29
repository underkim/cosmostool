#pragma once

#include "viewmodels/packettools/PacketToolsViewModel.h"

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QComboBox>
#include <QSpinBox>
#include <QTableWidget>

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
    void onSimulatorStartClicked();
    void onSimulatorStopClicked();
    void onSimulatorSendClicked();

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
    QComboBox*    simulatorMode_{nullptr};
    QLineEdit*    simulatorBindAddress_{nullptr};
    QSpinBox*     simulatorBindPort_{nullptr};
    QLineEdit*    simulatorSendHost_{nullptr};
    QSpinBox*     simulatorSendPort_{nullptr};
    QLineEdit*    simulatorPayload_{nullptr};
    QPushButton*  simulatorStartBtn_{nullptr};
    QPushButton*  simulatorStopBtn_{nullptr};
    QPushButton*  simulatorSendBtn_{nullptr};
    QTableWidget* simulatorPackets_{nullptr};
};

} // namespace OpenC3::UI::Views
