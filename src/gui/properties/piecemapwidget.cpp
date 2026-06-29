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
    // Gap between cells in pixels
    constexpr int GAP = 1;
    // Target cell size; actual size is clamped to fill available width evenly
    constexpr int TARGET_CELL = 9;
    constexpr int MIN_CELL = 4;
}

PieceMapWidget::PieceMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(40);
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

int PieceMapWidget::cellSize() const
{
    const int n = m_downloaded.size();
    if (n <= 0 || width() <= 0)
        return TARGET_CELL;

    // Find largest cell size where all pieces fit in the available width.
    // cells_per_row * (cell + gap) - gap <= width
    // cells_per_row = ceil(n / rows) where rows = ceil(n / cells_per_row)
    // Simpler: pick cells_per_row from width and compute resulting cell size.
    // Start from TARGET_CELL and shrink if needed.
    for (int cell = TARGET_CELL; cell >= MIN_CELL; --cell)
    {
        const int cols = (width() + GAP) / (cell + GAP);
        if (cols > 0)
            return cell;
    }
    return MIN_CELL;
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

    const int cell = cellSize();
    const int step = cell + GAP;
    const int cols = std::max(1, (width() + GAP) / step);

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
        const int x = col * step;
        const int y = row * step;
        const QRect cellRect(x, y, cell, cell);

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
