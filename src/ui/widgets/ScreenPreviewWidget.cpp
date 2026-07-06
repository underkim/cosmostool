#include "ScreenPreviewWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace OpenC3::UI::Widgets {

using ViewModels::ScreenWidgetNode;
using ViewModels::ScreenLayoutResult;

namespace {

QString joinedArgs(const ScreenWidgetNode& node)
{
    return node.args.join(' ');
}

} // namespace

ScreenPreviewWidget::ScreenPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    rootLayout_ = new QVBoxLayout(this);
    rootLayout_->setAlignment(Qt::AlignTop);
}

void ScreenPreviewWidget::setLayoutTree(const ScreenLayoutResult& result)
{
    QLayoutItem* item;
    while ((item = rootLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (const auto& node : result.roots)
        rootLayout_->addWidget(buildNode(node));
}

QWidget* ScreenPreviewWidget::buildNode(const ScreenWidgetNode& node)
{
    return node.isContainer ? buildContainer(node) : buildLeaf(node);
}

QWidget* ScreenPreviewWidget::buildContainer(const ScreenWidgetNode& node)
{
    auto* frame = new QFrame(this);
    frame->setObjectName("ScreenPreviewContainer");
    frame->setFrameShape(QFrame::StyledPanel);

    // VERTICAL(BOX)/HORIZONTAL(BOX) get the real COSMOS layout direction;
    // every other container kind this app recognises (MATRIXBYCOLUMNS,
    // SCROLLWINDOW, TABBOOK/TABITEM, CANVAS, RADIOGROUP) falls back to a
    // plain vertical stack - structurally present, without its own
    // COSMOS-specific layout behavior (e.g. TABBOOK doesn't render as real
    // tabs yet).
    QBoxLayout* layout = (node.keyword == "HORIZONTAL" || node.keyword == "HORIZONTALBOX")
        ? static_cast<QBoxLayout*>(new QHBoxLayout(frame))
        : static_cast<QBoxLayout*>(new QVBoxLayout(frame));
    layout->setAlignment(Qt::AlignTop);

    for (const auto& child : node.children)
        layout->addWidget(buildNode(child));

    return frame;
}

QWidget* ScreenPreviewWidget::buildLeaf(const ScreenWidgetNode& node)
{
    const QString& kw = node.keyword;

    if (kw == "TITLE") {
        auto* label = new QLabel(joinedArgs(node), this);
        label->setWordWrap(true);
        QFont f = label->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 4);
        label->setFont(f);
        return label;
    }
    if (kw == "LABEL") {
        auto* label = new QLabel(joinedArgs(node), this);
        label->setWordWrap(true);
        return label;
    }
    if (kw == "VALUE" || kw == "LABELVALUE") {
        auto* box = new QFrame(this);
        auto* layout = new QHBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* nameLabel = new QLabel(joinedArgs(node), box);
        // Real COSMOS target/packet/item names can run long (this is a
        // three-token join, e.g. "MYSAT VERY_LONG_PACKET_NAME
        // VERY_LONG_ITEM_NAME") - wrap instead of silently widening the
        // whole Preview tab and forcing horizontal scrolling.
        nameLabel->setWordWrap(true);
        QFont f = nameLabel->font();
        f.setItalic(true);
        nameLabel->setFont(f);
        auto* valueLabel = new QLabel(QStringLiteral("—"), box); // em dash placeholder
        valueLabel->setObjectName("ScreenPreviewValuePlaceholder");
        valueLabel->setFrameShape(QFrame::Panel);
        valueLabel->setFrameShadow(QFrame::Sunken);
        valueLabel->setMinimumWidth(48);
        valueLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(nameLabel);
        layout->addWidget(valueLabel);
        return box;
    }
    if (kw == "BUTTON") {
        return new QPushButton(node.args.value(0), this);
    }
    if (kw == "CHECKBUTTON") {
        return new QCheckBox(node.args.value(0), this);
    }
    if (kw == "RADIOBUTTON") {
        return new QRadioButton(node.args.value(0), this);
    }
    if (kw == "COMBOBOX") {
        auto* combo = new QComboBox(this);
        combo->addItems(node.args);
        return combo;
    }
    if (kw == "TEXTFIELD") {
        auto* edit = new QLineEdit(this);
        edit->setPlaceholderText(tr("Text field"));
        return edit;
    }
    if (kw == "TEXTBOX") {
        auto* edit = new QPlainTextEdit(this);
        edit->setPlaceholderText(tr("Text box"));
        edit->setMaximumHeight(60);
        return edit;
    }
    if (kw == "SPACER") {
        auto* spacer = new QWidget(this);
        spacer->setFixedSize(16, 16);
        return spacer;
    }
    if (kw == "HORIZONTALLINE") {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        return line;
    }

    return buildPlaceholder(node);
}

QWidget* ScreenPreviewWidget::buildPlaceholder(const ScreenWidgetNode& node)
{
    // Covers every other widget ScreenParser::knownWidgets() recognises
    // (LABELVALUEDESC, LIMITSBAR, PROGRESSBAR, SPARKLINE, LINEGRAPH, IMAGE,
    // LED, ...) as well as any keyword this app doesn't recognise at all
    // (typo, or a widget kind not yet modeled) - rendered visibly rather
    // than silently dropped or crashing.
    const QString text = node.args.isEmpty()
        ? QString("[%1]").arg(node.keyword)
        : QString("[%1 %2]").arg(node.keyword, joinedArgs(node));
    auto* label = new QLabel(text, this);
    label->setObjectName("ScreenPreviewPlaceholder");
    label->setWordWrap(true);
    label->setFrameShape(QFrame::Box);
    label->setStyleSheet("QLabel#ScreenPreviewPlaceholder { "
                          "border: 1px dashed palette(mid); "
                          "color: palette(mid); padding: 4px; }");
    return label;
}

} // namespace OpenC3::UI::Widgets
