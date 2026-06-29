#include "PluginKeywords.h"

namespace OpenC3::ViewModels::Validation::PluginKeywords {

const QSet<QString>& topLevel()
{
    static const QSet<QString> kTop = {
        "VARIABLE", "NEEDS_DEPENDENCIES",
        "TARGET", "INTERFACE", "ROUTER", "MICROSERVICE",
        "TOOL", "WIDGET", "SCRIPT_ENGINE",
    };
    return kTop;
}

const QSet<QString>& interfaceModifiers()
{
    static const QSet<QString> kMods = {
        "MAP_TARGET", "MAP_CMD_TARGET", "MAP_TLM_TARGET",
        "PROTOCOL", "OPTION", "SECRET", "ENV", "WORK_DIR",
        "DONT_CONNECT", "DONT_RECONNECT", "RECONNECT_DELAY",
        "DISABLE_DISCONNECT", "LOG_RAW", "PORT", "CMD",
        "CONTAINER", "ROUTE_PREFIX", "SHUTDOWN_DELAY",
        "CONNECT_ON_STARTUP", "AUTO_RECONNECT",
    };
    return kMods;
}

const QSet<QString>& microserviceModifiers()
{
    static const QSet<QString> kMods = {
        "ENV", "WORK_DIR", "CMD", "OPTION", "TOPIC", "TARGET_NAME",
        "SECRET", "PORT", "CONTAINER", "ROUTE_PREFIX", "DISABLE_ERB",
        "SHARD", "STOPPED",
    };
    return kMods;
}

const QSet<QString>& targetModifiers()
{
    static const QSet<QString> kMods = {
        "CMD_BUFFER_DEPTH", "CMD_LOG_CYCLE_TIME", "CMD_LOG_CYCLE_SIZE",
        "CMD_LOG_RETAIN_TIME", "CMD_DECOM_LOG_CYCLE_TIME",
        "CMD_DECOM_LOG_CYCLE_SIZE", "CMD_DECOM_LOG_RETAIN_TIME",
        "TLM_BUFFER_DEPTH", "TLM_LOG_CYCLE_TIME", "TLM_LOG_CYCLE_SIZE",
        "TLM_LOG_RETAIN_TIME", "TLM_DECOM_LOG_CYCLE_TIME",
        "TLM_DECOM_LOG_CYCLE_SIZE", "TLM_DECOM_LOG_RETAIN_TIME",
        "REDUCED_MINUTE_LOG_RETAIN_TIME", "REDUCED_HOUR_LOG_RETAIN_TIME",
        "REDUCED_DAY_LOG_RETAIN_TIME", "LOG_RETAIN_TIME",
        "REDUCER_DISABLE", "REDUCER_MAX_CPU_UTILIZATION",
        "TARGET_MICROSERVICE", "PACKET", "DISABLE_ERB", "SHARD",
    };
    return kMods;
}

const QSet<QString>& toolModifiers()
{
    static const QSet<QString> kMods = {
        "URL", "INLINE_URL", "ICON", "CATEGORY", "SHOWN",
        "POSITION", "WINDOW", "DISABLE_ERB", "IMPORT_MAP_ITEM",
    };
    return kMods;
}

} // namespace OpenC3::ViewModels::Validation::PluginKeywords
