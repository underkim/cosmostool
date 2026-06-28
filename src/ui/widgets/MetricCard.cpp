#include "MetricCard.h"

#include <QVBoxLayout>
#include <QFont>

namespace OpenC3::UI::Widgets {

MetricCard::MetricCard(const QString& title, const QString& unit, QWidget* parent)
    : QWidget(parent)
    , unit_(unit)
{
    setObjectName("MetricCard");
    // Required for the QSS background-color / border / border-radius on a plain
    // QWidget subclass to actually paint; without it the card renders flat.
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(160);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    titleLabel_ = new QLabel(title, this);
    QFont tf = titleLabel_->font();
    tf.setPointSize(9);
    tf.setCapitalization(QFont::AllUppercase);
    titleLabel_->setFont(tf);
    titleLabel_->setObjectName("MetricCardTitle");

    valueLabel_ = new QLabel("--", this);
    QFont vf = valueLabel_->font();
    vf.setPointSize(22);
    vf.setBold(true);
    valueLabel_->setFont(vf);
    valueLabel_->setObjectName("MetricCardValue");

    bar_ = new QProgressBar(this);
    bar_->setRange(0, 100);
    bar_->setValue(0);
    bar_->setTextVisible(false);
    bar_->setFixedHeight(6);
    bar_->setObjectName("MetricBar");

    layout->addWidget(titleLabel_);
    layout->addWidget(valueLabel_);
    layout->addWidget(bar_);
}

void MetricCard::setValue(double percent, const QString& label)
{
    const int pct = static_cast<int>(percent);
    bar_->setValue(pct);
    valueLabel_->setText(label.isEmpty()
        ? QString::number(pct) + unit_
        : label);

    // Color coding: green → amber → red
    QString barColor;
    if (percent < 70.0)      barColor = "#4ec94e";
    else if (percent < 85.0) barColor = "#cd9100";
    else                     barColor = "#f1444c";

    bar_->setStyleSheet(
        QString("QProgressBar::chunk { background: %1; border-radius: 3px; }")
            .arg(barColor));
}

} // namespace OpenC3::UI::Widgets
