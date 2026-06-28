#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace OpenC3::UI::Dialogs {

class CmdTlmFieldDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CmdTlmFieldDialog(QWidget* parent = nullptr);

    [[nodiscard]] QString generatedLine() const;

private:
    void updateMode();

    QComboBox* modeCombo_{nullptr};
    QCheckBox* idFieldCheck_{nullptr};
    QLineEdit* nameEdit_{nullptr};
    QSpinBox* bitSizeSpin_{nullptr};
    QComboBox* typeCombo_{nullptr};
    QLineEdit* minEdit_{nullptr};
    QLineEdit* maxEdit_{nullptr};
    QLineEdit* defaultEdit_{nullptr};
    QLineEdit* descriptionEdit_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
