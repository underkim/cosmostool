#include "ViewModelBase.h"

namespace OpenC3::ViewModels {

ViewModelBase::ViewModelBase(QObject* parent)
    : QObject(parent)
{}

bool ViewModelBase::isLoading() const noexcept { return loading_; }
QString ViewModelBase::errorMsg()  const noexcept { return errorMsg_; }

void ViewModelBase::setLoading(bool loading)
{
    if (loading_ == loading) return;
    loading_ = loading;
    emit loadingChanged();
}

void ViewModelBase::setError(const QString& msg)
{
    errorMsg_ = msg;
    emit errorMsgChanged();
}

void ViewModelBase::clearError()
{
    if (errorMsg_.isEmpty()) return;
    errorMsg_.clear();
    emit errorMsgChanged();
}

} // namespace OpenC3::ViewModels
