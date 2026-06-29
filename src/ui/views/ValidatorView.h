#pragma once

#include "viewmodels/validator/ValidatorViewModel.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

namespace OpenC3::UI::Views {

/// Configuration Validator — offline static checks for COSMOS CMD/TLM, screen,
/// and plugin.txt files. Lets the user validate a folder, a single file, or a
/// pasted snippet, and shows diagnostics grouped by file with line, severity,
/// cause, and a suggested fix.
class ValidatorView final : public QWidget {
    Q_OBJECT
public:
    explicit ValidatorView(ViewModels::ValidatorViewModel& vm,
                           QWidget*                        parent = nullptr);

private slots:
    void onValidateFolder();
    void onValidateFile();
    void onCheckPasted();
    void onReportReady();
    void onSelectionChanged();

private:
    void setupUi();
    void bindViewModel();

    ViewModels::ValidatorViewModel& vm_;

    QPushButton*    folderBtn_{nullptr};
    QPushButton*    fileBtn_{nullptr};
    QComboBox*      kindCombo_{nullptr};
    QPushButton*    checkBtn_{nullptr};
    QPlainTextEdit* pasteEdit_{nullptr};
    QTreeWidget*    resultsTree_{nullptr};
    QLabel*         summaryLabel_{nullptr};
    QLabel*         detailLabel_{nullptr};
};

} // namespace OpenC3::UI::Views
