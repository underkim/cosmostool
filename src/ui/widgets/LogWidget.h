#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QCheckBox>

namespace OpenC3::UI::Widgets {

/// A scrollable log viewer widget with auto-tail and clear controls.
class LogWidget final : public QWidget {
    Q_OBJECT
public:
    explicit LogWidget(QWidget* parent = nullptr);

    void appendLine(const QString& line);
    void setContent(const QString& content);
    void clear();

private:
    QPlainTextEdit* textEdit_{nullptr};
    QCheckBox*      tailCheckbox_{nullptr};
};

} // namespace OpenC3::UI::Widgets
