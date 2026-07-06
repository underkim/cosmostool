#pragma once

#include <QDialog>

class QComboBox;
class QLineEdit;
class QStackedWidget;

namespace OpenC3::UI::Dialogs {

// Guided "Add Widget" dialog for the Workspace Preview tab - lets a
// first-time user add a common screen widget (Title, Label, a
// telemetry-bound Value, or a Button) by filling in a plain-language form,
// instead of hand-typing COSMOS screen DSL syntax (TITLE/LABEL/VALUE/BUTTON
// lines) directly in the Source tab. New widgets are appended at the end of
// the file (top-level, outside any container) - a deliberately simple first
// step; nesting a widget inside a VERTICAL/HORIZONTAL box still requires
// hand-editing in this phase.
class ScreenWidgetDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ScreenWidgetDialog(QWidget* parent = nullptr);

    // The generated COSMOS screen line(s), ready to append.
    [[nodiscard]] QString generatedLine() const;

private:
    void updateFieldsForKind(int kindIndex);

    QComboBox*      kindCombo_{nullptr};
    QStackedWidget* fieldStack_{nullptr};

    // Title / Label
    QLineEdit* textEdit_{nullptr};

    // Value (telemetry-bound)
    QLineEdit* targetEdit_{nullptr};
    QLineEdit* packetEdit_{nullptr};
    QLineEdit* itemEdit_{nullptr};

    // Button
    QLineEdit* buttonLabelEdit_{nullptr};
    QLineEdit* buttonCommandEdit_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
