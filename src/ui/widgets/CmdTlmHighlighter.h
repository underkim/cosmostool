#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

namespace OpenC3::UI::Widgets {

/// Syntax highlighter for OpenC3 COSMOS command/telemetry definition files.
/// Highlights block keywords, item keywords, data types, strings, and comments
/// using VS-Code-Dark-style colours that read well in the app's dark theme.
class CmdTlmHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit CmdTlmHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    QVector<Rule> rules_;
};

} // namespace OpenC3::UI::Widgets
