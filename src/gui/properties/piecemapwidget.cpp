/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent project
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

#include "piecemapwidget.h"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPalette>

#include "base/bittorrent/torrent.h"

namespace
{
    constexpr int GAP = 1;
    constexpr int MIN_CELL = 2;
}

PieceMapWidget::PieceMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(60);
}

void PieceMapWidget::setTorrent(BitTorrent::Torrent *torrent)
{
    m_torrent = torrent;
    if (!torrent)
        clear();
}

void PieceMapWidget::setProgress(const QBitArray &downloaded, const QBitArray &downloading)
{
    m_downloaded = downloaded;
    m_downloading = downloading;
    update();
}

void PieceMapWidget::setAvailability(const QList<int> &availability)
{
    m_availability = availability;
    m_maxAvailability = 1;
    for (const int v : availability)
        m_maxAvailability = std::max(m_maxAvailability, v);
    update();
}

void PieceMapWidget::clear()
{
    m_downloaded.clear();
    m_downloading.clear();
    m_availability.clear();
    m_maxAvailability = 1;
    update();
}

QSize PieceMapWidget::sizeHint() const
{
    return {200, 80};
}

void PieceMapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QPalette &pal = palette();

    // Background
    painter.fillRect(rect(), pal.color(QPalette::Base));

    const int n = m_downloaded.size();
    if (n <= 0)
        return;

    const int W = width();
    const int H = height();

    // Derive cols from target cell size, then stretch cw/ch to fill area exactly.
    // ideal square cell: area per piece = W*H/n → c = sqrt(W*H/n)
    const int idealCell = std::max(MIN_CELL,
        static_cast<int>(std::sqrt(static_cast<double>(W) * H / n)));
    const int cols = std::max(1, (W + GAP) / (idealCell + GAP));
    const int rows = (n + cols - 1) / cols;

    // Stretch cell dimensions so the grid fills the full widget area
    const int cw = std::max(1, (W + GAP) / cols - GAP);
    const int ch = std::max(1, (H + GAP) / rows - GAP);

    // Colors
    const QColor bgEmpty = pal.color(QPalette::Mid);
    const QColor colDone = QColor(0x2d, 0xa4, 0x4e);       // green
    const QColor colActive = QColor(0xf0, 0x88, 0x00);      // amber (downloading)
    const QColor colAvailLow = QColor(0xaa, 0xd0, 0xf5);    // pale blue
    const QColor colAvailHigh = QColor(0x09, 0x69, 0xda);   // saturated blue

    const bool hasAvail = !m_availability.isEmpty() && (m_availability.size() == n);
    const bool hasDownloading = !m_downloading.isEmpty() && (m_downloading.size() == n);

    for (int i = 0; i < n; ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        const int x = col * (cw + GAP);
        const int y = row * (ch + GAP);
        const QRect cellRect(x, y, cw, ch);

        QColor color;
        if (!m_downloaded.isEmpty() && m_downloaded.testBit(i))
        {
            color = colDone;
        }
        else if (hasDownloading && m_downloading.testBit(i))
        {
            color = colActive;
        }
        else if (hasAvail && m_availability[i] > 0)
        {
            const float ratio = static_cast<float>(m_availability[i]) / m_maxAvailability;
            // Lerp between pale and saturated blue
            const int r = static_cast<int>(colAvailLow.red()   + ratio * (colAvailHigh.red()   - colAvailLow.red()));
            const int g = static_cast<int>(colAvailLow.green() + ratio * (colAvailHigh.green() - colAvailLow.green()));
            const int b = static_cast<int>(colAvailLow.blue()  + ratio * (colAvailHigh.blue()  - colAvailLow.blue()));
            color = QColor(r, g, b);
        }
        else
        {
            color = bgEmpty;
        }

        painter.fillRect(cellRect, color);
    }
}
