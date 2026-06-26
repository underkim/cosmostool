#pragma once

#include <QDialog>

namespace OpenC3::UI::Dialogs {

class AboutDialog final : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace OpenC3::UI::Dialogs
