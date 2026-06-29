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

#include <QPainter>
#include <QPalette>

#include "base/bittorrent/torrent.h"

namespace
{
    constexpr int CELL = 2;
    constexpr int STEP = CELL;
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

    // Fixed cell size; quantity fills the widget area
    const int cols  = std::max(1, width()  / STEP);
    const int rows  = std::max(1, height() / STEP);
    const int total = cols * rows;

    // Colors
    const QColor bgEmpty     = pal.color(QPalette::Mid);
    const QColor colDone     = QColor(0x2d, 0xa4, 0x4e);
    const QColor colActive   = QColor(0xf0, 0x88, 0x00);
    const QColor colAvailLow = QColor(0xaa, 0xd0, 0xf5);
    const QColor colAvailHigh= QColor(0x09, 0x69, 0xda);

    const bool hasAvail      = !m_availability.isEmpty() && (m_availability.size() == n);
    const bool hasDownloading= !m_downloading.isEmpty() && (m_downloading.size() == n);

    for (int k = 0; k < total; ++k)
    {
        // Map cell k → piece range [p0, p1) proportionally
        const int p0   = static_cast<int>(static_cast<double>(k)     * n / total);
        const int p1   = static_cast<int>(static_cast<double>(k + 1) * n / total);
        const int pEnd = std::min(n, std::max(p0 + 1, p1));

        // Aggregate state across the piece range
        int doneCount = 0;
        bool anyActive = false;
        int availSum = 0;
        for (int i = p0; i < pEnd; ++i)
        {
            if (!m_downloaded.isEmpty() && m_downloaded.testBit(i))
                ++doneCount;
            else if (hasDownloading && m_downloading.testBit(i))
                anyActive = true;
            if (hasAvail)
                availSum += m_availability[i];
        }

        const int span = pEnd - p0;
        QColor color;
        if (doneCount == span)
        {
            color = colDone;
        }
        else if (anyActive || doneCount > 0)
        {
            // Partially done or actively downloading
            color = colActive;
        }
        else if (hasAvail && availSum > 0)
        {
            const float ratio = static_cast<float>(availSum) / (span * m_maxAvailability);
            const int r = static_cast<int>(colAvailLow.red()   + ratio * (colAvailHigh.red()   - colAvailLow.red()));
            const int g = static_cast<int>(colAvailLow.green() + ratio * (colAvailHigh.green() - colAvailLow.green()));
            const int b = static_cast<int>(colAvailLow.blue()  + ratio * (colAvailHigh.blue()  - colAvailLow.blue()));
            color = QColor(r, g, b);
        }
        else
        {
            color = bgEmpty;
        }

        const int row = k % rows;
        const int col = k / rows;
        painter.fillRect(col * STEP, row * STEP, CELL, CELL, color);
    }
}
