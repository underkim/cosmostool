#pragma once

#include <QDialog>
#include <QSet>
#include <QString>

class QComboBox;
class QLineEdit;

namespace OpenC3::UI::Dialogs {

// Add-a-modifier-line dialog for the Manifest tab (PROTOCOL, ENV, SECRET,
// MAP_TARGET, OPTION, ... - one line nested under a TARGET/INTERFACE/ROUTER/
// MICROSERVICE/TOOL/WIDGET block in plugin.txt). The keyword combo is
// populated from the current block kind's modifier set
// (PluginKeywords::interfaceModifiers() etc.) but stays editable, since
// plugin.txt does carry keywords outside those curated lists on occasion.
class PluginManifestModifierDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PluginManifestModifierDialog(const QSet<QString>& suggestedKeywords,
                                          QWidget* parent = nullptr);

    // "  <KEYWORD> <args...>\n", ready to insert as a new line.
    [[nodiscard]] QString generatedLine() const;

private:
    QComboBox* keywordCombo_{nullptr};
    QLineEdit* argsEdit_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
