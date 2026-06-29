#include "ValidatorViewModel.h"

#include "viewmodels/validation/RuleValidatorRegistry.h"

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

QVector<QPair<QString, QString>> ValidatorViewModel::availableValidators() const
{
    QVector<QPair<QString, QString>> out;
    for (const auto* v : Validation::RuleValidatorRegistry::all())
        out.append({ v->id(), v->label() });
    return out;
}

void ValidatorViewModel::validateTextWith(const QString& validatorId,
                                          const QString& content)
{
    if (validatorId.isEmpty()) {
        // Auto-detect the file kind from the content.
        const auto kind = Validation::ConfigValidator::classify(QString(), content);
        report_ = Validation::ConfigValidator::validateContent(kind, content);
    } else if (const auto* v = Validation::RuleValidatorRegistry::byId(validatorId)) {
        report_ = v->validate(content);
    } else {
        report_ = Validation::ValidationReport{};
    }

    lastSource_ = QStringLiteral("(pasted)");
    emit reportReady();
}

void ValidatorViewModel::checkContent(const QString& content)
{
    validateTextWith(QString(), content);
}

void ValidatorViewModel::clear()
{
    report_.diagnostics.clear();
    lastSource_.clear();
    emit reportReady();
}

} // namespace OpenC3::ViewModels
