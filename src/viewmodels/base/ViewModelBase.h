#pragma once

#include <QObject>
#include <QString>

namespace OpenC3::ViewModels {

/// Base class for all ViewModels.
///
/// Design contract:
/// - ViewModels own the business state that views display.
/// - They call C++ Services on worker threads (via QtConcurrent) and emit
///   Qt signals back on the main thread so Views can update safely.
/// - They never touch Qt widgets directly.
/// - The loading/error state pattern is standardised here so every View
///   gets consistent busy-indicator and error-display behaviour.
class ViewModelBase : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool    loading  READ isLoading  NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMsg READ errorMsg   NOTIFY errorMsgChanged)

public:
    explicit ViewModelBase(QObject* parent = nullptr);
    ~ViewModelBase() override = default;

    [[nodiscard]] bool    isLoading() const noexcept;
    [[nodiscard]] QString errorMsg()  const noexcept;

signals:
    void loadingChanged();
    void errorMsgChanged();

    /// Emitted when the ViewModel's data has fully refreshed.
    void refreshed();

protected:
    void setLoading(bool loading);
    void setError(const QString& msg);
    void clearError();

private:
    bool    loading_{false};
    QString errorMsg_;
};

} // namespace OpenC3::ViewModels
