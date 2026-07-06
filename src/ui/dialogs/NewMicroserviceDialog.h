#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

namespace OpenC3::UI::Dialogs {

/// Guided plugin.txt "New Microservice" creation - the Microservice
/// counterpart to PluginManifestInterfaceDialog's Interface/Router flow.
/// Generates a MICROSERVICE block with a real folder/name/cmd instead of
/// the placeholder NEW_FOLDER/NEW_MICROSERVICE snippet the "New Block" menu
/// previously inserted verbatim.
class NewMicroserviceDialog final : public QDialog {
    Q_OBJECT
public:
    explicit NewMicroserviceDialog(QWidget* parent = nullptr);

    [[nodiscard]] QString generatedBlock() const;

private:
    QLineEdit* folderEdit_{nullptr};
    QLineEdit* nameEdit_{nullptr};
    QLineEdit* cmdEdit_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
