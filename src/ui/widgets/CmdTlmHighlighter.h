#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

namespace OpenC3::UI::Widgets {

/// Syntax highlighter for OpenC3 COSMOS command/telemetry definition files,
/// with a secondary mode for procedures/*.rb scripts (Ruby keywords + the
/// COSMOS script API's cmd()/tlm()/wait_check()/... calls) so script authors
/// get the same "write details well" support as the COSMOS DSL editors do,
/// instead of plain unstyled text.  Highlights block keywords, item keywords,
/// data types, strings, and comments using VS-Code-Dark-style colours that
/// read well in the app's dark theme.
class CmdTlmHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit CmdTlmHighlighter(QTextDocument* parent = nullptr);

    /// Switches between the COSMOS cmd_tlm/plugin.txt/screen ruleset (false,
    /// the default) and the Ruby script ruleset (true), then re-runs
    /// highlighting over the whole document so the switch takes effect
    /// immediately on the currently-open file.
    void setRubyMode(bool on);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    QVector<Rule> rules_;
    QVector<Rule> rubyRules_;
    bool rubyMode_{false};
};

} // namespace OpenC3::UI::Widgets
