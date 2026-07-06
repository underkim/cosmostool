#include "PluginTemplateEngine.h"

namespace OpenC3::ViewModels {

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace {

QMap<QString, QString> buildCmdTlmFiles(
    const QString& tgt, int templateType, const QString& pluginNamespace)
{
    QMap<QString, QString> files;

    const QString cmdPath = "targets/" + tgt + "/cmd_tlm/" + tgt.toLower() + "_cmds.txt";
    const QString tlmPath = "targets/" + tgt + "/cmd_tlm/" + tgt.toLower() + "_tlm.txt";
    const QString nsComment =
        pluginNamespace.isEmpty() ? QString() : "# Namespace: " + pluginNamespace + "\n";

    if (templateType == 1) {
        // CCSDS satellite layout
        files[cmdPath] = QString(
            "# %1 Commands\n\n"
            "COMMAND %1 NOOP BIG_ENDIAN 'No operation'\n"
            "  APPEND_ID_PARAMETER CCSDS_STREAMID 16 UINT MIN MAX 0x1800 'Stream ID'\n"
            "    FORMAT_STRING '0x%04X'\n"
            "  APPEND_PARAMETER CCSDS_SEQUENCE 16 UINT MIN MAX 0xC000 'Sequence Control'\n"
            "  APPEND_PARAMETER CCSDS_LENGTH   16 UINT MIN MAX 0      'Data Length'\n"
            "  APPEND_PARAMETER CCSDS_FUNCCODE  8 UINT MIN MAX 0      'Function Code'\n"
            "  APPEND_PARAMETER CCSDS_CHECKSUM  8 UINT MIN MAX 0      'Checksum'\n\n"
            "COMMAND %1 RESET BIG_ENDIAN 'Software Reset'\n"
            "  APPEND_ID_PARAMETER CCSDS_STREAMID 16 UINT MIN MAX 0x1801 'Stream ID'\n"
            "    FORMAT_STRING '0x%04X'\n"
            "  APPEND_PARAMETER CCSDS_SEQUENCE 16 UINT MIN MAX 0xC000 'Sequence Control'\n"
            "  APPEND_PARAMETER CCSDS_LENGTH   16 UINT MIN MAX 0      'Data Length'\n"
            "  APPEND_PARAMETER CCSDS_FUNCCODE  8 UINT MIN MAX 1      'Function Code'\n"
            "  APPEND_PARAMETER CCSDS_CHECKSUM  8 UINT MIN MAX 0      'Checksum'\n"
        ).arg(tgt);

        files[tlmPath] = QString(
            "# %1 Telemetry\n\n"
            "TELEMETRY %1 HK BIG_ENDIAN 'Housekeeping'\n"
            "  APPEND_ID_ITEM CCSDS_STREAMID  16 UINT 0x0800 'Stream ID'\n"
            "    FORMAT_STRING '0x%04X'\n"
            "  APPEND_ITEM CCSDS_SEQUENCE     16 UINT 'Sequence Control'\n"
            "  APPEND_ITEM CCSDS_LENGTH       16 UINT 'Packet Length'\n"
            "  APPEND_ITEM CCSDS_SECONDS      32 UINT 'Time Seconds'\n"
            "  APPEND_ITEM CCSDS_SUBSECONDS   16 UINT 'Time Subseconds'\n"
            "  APPEND_ITEM MODE                8 UINT 'Operating Mode'\n"
            "    STATES SAFE 0 NOMINAL 1 FAULT 2\n"
            "  APPEND_ITEM TEMP               16 INT  'Temperature (x0.1 °C)'\n"
            "    UNITS Celsius C\n"
        ).arg(tgt);
    } else {
        // Generic target (templateType 0 or 2)
        files[cmdPath] = QString(
            "# %1 Commands\n\n"
            "COMMAND %1 PING BIG_ENDIAN 'Ping command'\n"
            "  APPEND_PARAMETER CMD_ID 8 UINT 0 255 0x01 'Command ID'\n"
            "  APPEND_PARAMETER LENGTH 8 UINT 0 255 0    'Payload Length'\n\n"
            "COMMAND %1 RESET BIG_ENDIAN 'Reset command'\n"
            "  APPEND_PARAMETER CMD_ID 8 UINT 0 255 0x02 'Command ID'\n"
            "  APPEND_PARAMETER LENGTH 8 UINT 0 255 0    'Payload Length'\n"
        ).arg(tgt);

        files[tlmPath] = QString(
            "# %1 Telemetry\n\n"
            "TELEMETRY %1 STATUS BIG_ENDIAN 'Status packet'\n"
            "  APPEND_ITEM TLM_ID  8 UINT 'Telemetry ID'\n"
            "  APPEND_ITEM LENGTH  8 UINT 'Length'\n"
            "  APPEND_ITEM STATUS  8 UINT 'Status byte'\n"
            "    STATES IDLE 0 RUNNING 1 ERROR 2\n"
            "  APPEND_ITEM COUNTER 16 UINT 'Packet counter'\n"
        ).arg(tgt);
    }

    if (!nsComment.isEmpty()) {
        files[cmdPath].prepend(nsComment);
        files[tlmPath].prepend(nsComment);
    }

    return files;
}

QMap<QString, QString> buildScreenAndProcedures(const QString& tgt)
{
    QMap<QString, QString> files;

    files["targets/" + tgt + "/screens/" + tgt.toLower() + ".txt"] = QString(
        "SCREEN AUTO AUTO 1.0\n\n"
        "TITLE '%1 Status'\n\n"
        "VERTICALBOX\n"
        "  LABELVALUE %1 STATUS STATUS\n"
        "  LABELVALUE %1 STATUS COUNTER\n"
        "END\n"
    ).arg(tgt);

    files["targets/" + tgt + "/procedures/" + tgt.toLower() + "_check.rb"] = QString(
        "# %1 checkout procedure\n\n"
        "def %2_check\n"
        "  puts 'Starting %1 checkout...'\n"
        "  cmd('%1 PING')\n"
        "  wait_check('%1 STATUS STATUS == \"RUNNING\"', 5)\n"
        "  puts '%1 checkout PASSED'\n"
        "rescue => e\n"
        "  puts \"FAILED: #{e.message}\"\n"
        "  raise\n"
        "end\n\n"
        "%2_check\n"
    ).arg(tgt, tgt.toLower());

    return files;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

QMap<QString, QString> PluginTemplateEngine::buildFiles(
    const QString& pluginName,
    const QString& targetName,
    const QString& description,
    int            templateType,
    int            ifaceType,
    const QString& ifaceHost,
    const QString& ifacePort,
    const QString& pluginNamespace)
{
    const QString tgt    = targetName.toUpper();
    const QString gem    = "cosmos-" + pluginName;
    const QString varPfx = pluginName.toLower().replace('-', '_');

    // -1 means "no explicit interface picked" - derive it from the CMD/TLM
    // template like this function originally did (GSE => TCP/IP server).
    const int effectiveIfaceType =
        (ifaceType >= 0) ? ifaceType : (templateType == 2 ? 1 : 0);

    QString ifaceLine;
    switch (effectiveIfaceType) {
    case 1: // TCP/IP Server
        ifaceLine = QString("tcpip_server_interface.rb %1 %1 10 nil BURST").arg(ifacePort);
        break;
    case 2: // UDP
        ifaceLine = QString("udp_interface.rb %1 %2 %2 nil 10 nil").arg(ifaceHost, ifacePort);
        break;
    case 3: // Serial - host field holds the device path (e.g. /dev/ttyUSB0)
        ifaceLine = QString("serial_interface.rb %1 %2 NONE 1 10 nil").arg(ifaceHost, ifacePort);
        break;
    default: // TCP/IP Client
        ifaceLine = QString("tcpip_client_interface.rb %1 %2 %2 10 nil BURST").arg(ifaceHost, ifacePort);
    }

    QMap<QString, QString> files;

    files["plugin.txt"] = QString(
        "# OpenC3 COSMOS Plugin: %1\n"
        "# Description: %2\n\n"
        "VARIABLE %3_target_name %4\n\n"
        "TARGET %4 <%= %3_target_name %>\n"
        "INTERFACE <%= %3_target_name %>_INT %5\n"
        "  MAP_TARGET <%= %3_target_name %>\n"
        "\n"
        "# Additional plugin.txt block kinds, editable via the Manifest tab -\n"
        "# uncomment (and adjust) whichever this plugin needs:\n"
        "\n"
        "# ROUTER %4_ROUTER generic_router.rb\n"
        "#   MAP_TARGET %4\n"
        "\n"
        "# MICROSERVICE %4_MICRO %4_HEALTH_CHECK\n"
        "#   CMD ruby health_check.rb\n"
        "\n"
        "# TOOL %4_tool\n"
        "#   URL /tools/%4_tool\n"
        "\n"
        "# WIDGET %4_widget\n"
        "#   URL /widgets/%4_widget\n"
    ).arg(pluginName, description, varPfx, tgt, ifaceLine);

    files[gem + ".gemspec"] = QString(
        "# encoding: ascii-8bit\n\n"
        "Gem::Specification.new do |s|\n"
        "  s.name        = '%1'\n"
        "  s.version     = '0.0.1'\n"
        "  s.platform    = Gem::Platform::RUBY\n"
        "  s.summary     = '%2'\n"
        "  s.description = '%2'\n"
        "  s.authors     = ['']\n"
        "  s.email       = ['']\n"
        "  s.license     = 'Nonstandard'\n\n"
        "  s.files       = Dir['{targets,lib,spec}/**/*', 'plugin.txt']\n"
        "  s.require_paths = ['lib']\n"
        "end\n"
    ).arg(gem, description);

    files["Gemfile"] =
        "# frozen_string_literal: true\n\n"
        "source 'https://rubygems.org'\n"
        "gemspec\n";

    files.insert(buildTargetFiles(targetName, templateType, pluginNamespace));

    return files;
}

QMap<QString, QString> PluginTemplateEngine::buildTargetFiles(
    const QString& targetName,
    int            templateType,
    const QString& pluginNamespace)
{
    const QString tgt = targetName.toUpper();
    QMap<QString, QString> files;
    files.insert(buildCmdTlmFiles(tgt, templateType, pluginNamespace));
    files.insert(buildScreenAndProcedures(tgt));
    return files;
}

} // namespace OpenC3::ViewModels
