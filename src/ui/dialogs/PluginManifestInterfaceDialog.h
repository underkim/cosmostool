#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLineEdit;

namespace OpenC3::UI::Dialogs {

// Guided "New Interface" / "New Router" dialog for the Manifest tab -
// mirrors PluginWizard's Interface step (connection-type dropdown +
// Host:Port fields) instead of dumping a raw, unexplained
// "tcpip_client_interface.rb host 8080 8081 10 nil BURST"-style line on the
// user, which a beginner has no way to interpret. Reuses
// PluginTemplateEngine::buildInterfaceArgs() so the generated line matches
// exactly what the initial plugin-creation wizard would have written.
class PluginManifestInterfaceDialog final : public QDialog {
    Q_OBJECT

public:
    // isRouter: true builds a ROUTER block, false an INTERFACE block.
    // knownTargets: names of TARGET blocks already declared in this
    // plugin.txt, offered as MAP_TARGET choices (falls back to free text if
    // empty - a plugin.txt with no TARGET yet is still a valid, if unusual,
    // state to add an interface to).
    explicit PluginManifestInterfaceDialog(
        bool isRouter, const QStringList& knownTargets, QWidget* parent = nullptr);

    // The full block, ready to append: "INTERFACE|ROUTER <name> <args...>\n
    // MAP_TARGET <target>\n"
    [[nodiscard]] QString generatedBlock() const;

private:
    void updateHostPortPlaceholders(int ifaceTypeIndex);

    bool isRouter_{false};
    QLineEdit* nameEdit_{nullptr};
    QComboBox* targetCombo_{nullptr};
    QComboBox* ifaceTypeCombo_{nullptr};
    QLineEdit* hostEdit_{nullptr};
    QLineEdit* portEdit_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
