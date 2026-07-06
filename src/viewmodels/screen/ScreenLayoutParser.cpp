#include "ScreenLayoutParser.h"

#include "viewmodels/validation/TextTokenizer.h"

#include <QSet>
#include <utility>

namespace OpenC3::ViewModels {

namespace {

// Mirrors ScreenParser's own containerWidgets() (validation/ScreenParser.cpp)
// - duplicated locally rather than shared, matching this codebase's existing
// convention of each parser owning its own keyword tables (CmdTlmParser,
// PluginManifestParser).
const QSet<QString>& containerKeywords()
{
    static const QSet<QString> kContainers = {
        "VERTICAL", "VERTICALBOX",
        "HORIZONTAL", "HORIZONTALBOX",
        "MATRIXBYCOLUMNS",
        "SCROLLWINDOW",
        "TABBOOK", "TABITEM",
        "CANVAS",
        "RADIOGROUP",
    };
    return kContainers;
}

const QSet<QString>& settingKeywords()
{
    static const QSet<QString> kSettings = {
        "SETTING", "SUBSETTING",
        "GLOBAL_SETTING", "GLOBAL_SUBSETTING",
    };
    return kSettings;
}

// Appends a new child to the current container (top of stack) or to `roots`
// when the stack is empty, and returns a pointer to the just-appended node -
// safe to hold only until the *next* append to that same vector (matching
// the pointer-into-QVector pattern CmdTlmParser/PluginManifestParser already
// use: a stack entry is only ever read/pushed-under-further while nothing
// else is appended to the vector it lives in).
ScreenWidgetNode* appendChild(QVector<ScreenWidgetNode*>& stack,
                              QVector<ScreenWidgetNode>& roots,
                              ScreenWidgetNode node)
{
    if (stack.isEmpty()) {
        roots.append(std::move(node));
        return &roots.last();
    }
    ScreenWidgetNode* parent = stack.last();
    parent->children.append(std::move(node));
    return &parent->children.last();
}

} // namespace

ScreenLayoutResult ScreenLayoutParser::parse(const QString& content)
{
    ScreenLayoutResult result;
    const QStringList lines = content.split('\n');

    QVector<ScreenWidgetNode*> stack;

    for (int i = 0; i < lines.size(); ++i) {
        const int     lineNo  = i + 1;
        const QString trimmed = lines[i].trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        const QStringList toks = Validation::tokenizeConfigLine(trimmed);
        if (toks.isEmpty()) continue;

        const QString kw = toks[0].toUpper();

        if (kw == "SCREEN") {
            result.hasHeader = true;
            if (toks.size() > 1) result.screenWidth  = toks[1];
            if (toks.size() > 2) result.screenHeight = toks[2];
            continue;
        }

        if (kw == "END") {
            if (!stack.isEmpty())
                stack.removeLast();
            // A stray END (no open container) is silently ignored - this
            // parser isn't a validator, ScreenParser already reports that.
            continue;
        }

        if (settingKeywords().contains(kw))
            continue; // modifiers don't become their own preview node

        QString keyword    = kw;
        QString namedName;
        int     argOffset  = 1;

        if (kw == "NAMED_WIDGET" && toks.size() >= 3) {
            namedName = toks[1];
            keyword   = toks[2].toUpper();
            argOffset = 3;
        }

        ScreenWidgetNode node;
        node.keyword         = keyword;
        node.namedWidgetName = namedName;
        node.lineNumber      = lineNo;
        node.isContainer     = containerKeywords().contains(keyword);
        for (int t = argOffset; t < toks.size(); ++t)
            node.args << toks[t];

        ScreenWidgetNode* appended = appendChild(stack, result.roots, std::move(node));
        if (appended->isContainer)
            stack.append(appended);
    }

    return result;
}

} // namespace OpenC3::ViewModels
