#include "PluginValidator.h"
#include "PluginConfigParser.h"

#include <QFileInfo>

namespace OpenC3::ViewModels::Validation {

bool PluginValidator::appliesTo(const QString& path, const QString& content) const
{
    if (QFileInfo(path).fileName().compare(QStringLiteral("plugin.txt"),
                                           Qt::CaseInsensitive) == 0)
        return true;
    // plugin.txt files start with a top-level declaration; VARIABLE / TARGET /
    // INTERFACE / ROUTER / MICROSERVICE are the common openers.
    const QString upper = content.toUpper();
    return upper.contains(QStringLiteral("\nINTERFACE "))
        || upper.startsWith(QStringLiteral("INTERFACE "))
        || upper.contains(QStringLiteral("\nROUTER "))
        || upper.contains(QStringLiteral("\nMICROSERVICE "));
}

ValidationReport PluginValidator::validate(const QString& content) const
{
    return PluginConfigParser::parse(content).report;
}

} // namespace OpenC3::ViewModels::Validation
