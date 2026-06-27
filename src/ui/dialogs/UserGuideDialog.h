#pragma once

#include <QDialog>
#include <QTextBrowser>
#include <QLineEdit>

namespace OpenC3::UI::Dialogs {

/// In-app user guide shown via Help → User Guide (F1).
/// Content is loaded from the embedded Qt resource :/help/userguide.html.
class UserGuideDialog final : public QDialog {
    Q_OBJECT
public:
    explicit UserGuideDialog(QWidget* parent = nullptr);

private slots:
    void onSearch(const QString& text);

private:
    QTextBrowser* browser_{nullptr};
    QLineEdit*    searchEdit_{nullptr};
};

} // namespace OpenC3::UI::Dialogs
