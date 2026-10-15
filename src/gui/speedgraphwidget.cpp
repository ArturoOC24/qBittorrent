/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent Modern UI fork
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "speedgraphwidget.h"

#include <algorithm>

#include <QPainter>
#include <QPainterPath>

#include "base/global.h"
#include "base/utils/misc.h"

SpeedGraphWidget::SpeedGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(210, 58);
    // Let clicks pass through to the torrent list underneath
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void SpeedGraphWidget::addSample(const qint64 downloadRate, const qint64 uploadRate, const qint64 diskWriteQueue)
{
    m_samples.append({downloadRate, uploadRate, diskWriteQueue});
    while (m_samples.size() > MAX_SAMPLES)
        m_samples.removeFirst();

    setToolTip(tr("Download: %1\nUpload: %2\nDisk write queue: %3")
        .arg(Utils::Misc::friendlyUnit(downloadRate, true)
            , Utils::Misc::friendlyUnit(uploadRate, true)
            , Utils::Misc::friendlyUnit(diskWriteQueue)));
    update();
}

void SpeedGraphWidget::paintEvent([[maybe_unused]] QPaintEvent *event)
{
    QPainter painter {this};
    painter.setRenderHint(QPainter::Antialiasing);

    // Card background: rounded, dark, subtle border
    const qreal cardRadius = 8;
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(255, 255, 255, 22), 1));
    painter.setBrush(QColor(0x24, 0x2B, 0x36, 240));
    painter.drawRoundedRect(card, cardRadius, cardRadius);

    // Header row: current download / disk legend
    const Sample latest = m_samples.isEmpty() ? Sample {} : m_samples.last();
    QFont headerFont = painter.font();
    headerFont.setPointSizeF(std::max<qreal>(7.5, headerFont.pointSizeF() - 1));
    painter.setFont(headerFont);
    painter.setPen(QColor(0x2F81F7));
    painter.drawText(QRectF(10, 5, card.width() - 20, 14), (Qt::AlignLeft | Qt::AlignVCenter)
        , tr("↓ %1").arg(Utils::Misc::friendlyUnit(latest.download, true)));
    painter.setPen(QColor(0x6BBF7B));
    painter.drawText(QRectF(10, 5, card.width() - 20, 14), (Qt::AlignRight | Qt::AlignVCenter)
        , tr("Disk %1").arg(Utils::Misc::friendlyUnit(latest.disk)));

    // Plot area below the header
    const QRectF plot = card.adjusted(8, 22, -8, -8);

    if (m_samples.size() < 2)
        return;

    qint64 maxRate = 1;
    qint64 maxDisk = 1;
    for (const Sample &sample : asConst(m_samples))
    {
        maxRate = std::max(maxRate, sample.download);
        maxDisk = std::max(maxDisk, sample.disk);
    }

    const qreal stepX = plot.width() / (MAX_SAMPLES - 1);
    const qreal bottom = plot.bottom();
    const qreal usableH = plot.height();
    const qreal offsetX = plot.right() - ((m_samples.size() - 1) * stepX);

    const auto seriesPath = [&](auto valueOf, const qint64 maxValue) -> QPainterPath
    {
        QPainterPath path;
        for (qsizetype i = 0; i < m_samples.size(); ++i)
        {
            const qreal x = offsetX + (i * stepX);
            const qreal y = bottom - (usableH * valueOf(m_samples[i]) / maxValue);
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        return path;
    };

    // Clip everything to the card's rounded shape so nothing spills past corners
    QPainterPath cardClip;
    cardClip.addRoundedRect(card.adjusted(1, 1, -1, -1), cardRadius - 1, cardRadius - 1);
    painter.setClipPath(cardClip);

    // Download rate: filled blue area (Steam network style)
    QPainterPath dlPath = seriesPath([](const Sample &s) { return s.download; }, maxRate);
    QPainterPath dlArea = dlPath;
    dlArea.lineTo(plot.right(), bottom);
    dlArea.lineTo(offsetX, bottom);
    dlArea.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(47, 129, 247, 70));
    painter.drawPath(dlArea);
    painter.setPen(QPen(QColor(0x2F81F7), 1.2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(dlPath);

    // Disk activity: green line on its own scale (Steam disk style)
    painter.setPen(QPen(QColor(0x6BBF7B), 1.0));
    painter.drawPath(seriesPath([](const Sample &s) { return s.disk; }, maxDisk));
}
