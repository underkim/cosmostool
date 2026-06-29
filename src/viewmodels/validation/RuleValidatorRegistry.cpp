#include "RuleValidatorRegistry.h"

#include "CommandValidator.h"
#include "TelemetryValidator.h"
#include "ScreenValidator.h"
#include "ProtocolValidator.h"
#include "InterfaceValidator.h"
#include "PluginValidator.h"
#include "TargetConfigValidator.h"

namespace OpenC3::ViewModels::Validation::RuleValidatorRegistry {

const QVector<const IRuleValidator*>& all()
{
    // Function-local statics: constructed once, owned for the program lifetime.
    static const CommandValidator      cmd;
    static const TelemetryValidator    tlm;
    static const ScreenValidator       screen;
    static const ProtocolValidator     protocol;
    static const InterfaceValidator    iface;
    static const PluginValidator       plugin;
    static const TargetConfigValidator target;

    static const QVector<const IRuleValidator*> kAll = {
        &cmd, &tlm, &screen, &protocol, &iface, &plugin, &target,
    };
    return kAll;
}

const IRuleValidator* byId(const QString& id)
{
    for (const IRuleValidator* v : all()) {
        if (v->id() == id)
            return v;
    }
    return nullptr;
}

} // namespace OpenC3::ViewModels::Validation::RuleValidatorRegistry
