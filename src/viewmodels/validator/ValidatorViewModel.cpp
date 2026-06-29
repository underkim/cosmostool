#include "ValidatorViewModel.h"

namespace OpenC3::ViewModels {

ValidatorViewModel::ValidatorViewModel(QObject* parent)
    : ViewModelBase(parent)
{
}

const Validation::ValidationReport& ValidatorViewModel::report() const noexcept
{
    return report_;
}

QString ValidatorViewModel::lastSource() const noexcept
{
    return lastSource_;
}

void ValidatorViewModel::validateFile(const QString& path)
{
    report_     = Validation::ConfigValidator::validateFile(path);
    lastSource_ = path;
    emit reportReady();
}

void ValidatorViewModel::validateFolder(const QString& dir)
{
    report_     = Validation::ConfigValidator::validateFolder(dir);
    lastSource_ = dir;
    emit reportReady();
}

void ValidatorViewModel::validateText(const QString& content, int kindOrAuto)
{
    using FileKind = Validation::ConfigValidator::FileKind;

    const FileKind kind = (kindOrAuto < 0)
        ? Validation::ConfigValidator::classify(QString(), content)
        : static_cast<FileKind>(kindOrAuto);

    report_     = Validation::ConfigValidator::validateContent(kind, content);
    lastSource_ = QStringLiteral("(pasted)");
    emit reportReady();
}

void ValidatorViewModel::clear()
{
    report_.diagnostics.clear();
    lastSource_.clear();
    emit reportReady();
}

} // namespace OpenC3::ViewModels
