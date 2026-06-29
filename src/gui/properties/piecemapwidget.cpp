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

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPalette>

#include "base/bittorrent/torrent.h"
#include "base/global.h"

#define PIECEMAP_KEY(name) (u"GUI/PieceMap/" name)

PieceMapWidget::PieceMapWidget(QWidget *parent)
    : QWidget(parent)
    , m_cellSize {PIECEMAP_KEY(u"CellSize"_s), 2}
    , m_gap      {PIECEMAP_KEY(u"Gap"_s),      0}
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(60);
    setContextMenuPolicy(Qt::DefaultContextMenu);
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
    const int step = m_cellSize + m_gap;
    // Suggest a height that is a clean multiple of the cell step (~20 rows).
    const int rows = std::max(1, height() > 0 ? height() / step : 20);
    return {200, rows * step};
}

void PieceMapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // --- Cell size submenu ---
    QMenu *sizeMenu = menu.addMenu(tr("Cell size"));
    auto *sizeGroup = new QActionGroup(sizeMenu);
    sizeGroup->setExclusive(true);
    const int currentCell = m_cellSize;
    for (const int s : {1, 2, 3, 4, 6, 8})
    {
        QAction *a = sizeMenu->addAction(tr("%1 px").arg(s));
        a->setCheckable(true);
        a->setChecked(s == currentCell);
        sizeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, s]
        {
            m_cellSize = s;
            updateGeometry();
            update();
        });
    }

    // --- Gap submenu ---
    QMenu *gapMenu = menu.addMenu(tr("Gap"));
    auto *gapGroup = new QActionGroup(gapMenu);
    gapGroup->setExclusive(true);
    const int currentGap = m_gap;
    for (const int g : {0, 1, 2})
    {
        QAction *a = (g == 0)
            ? gapMenu->addAction(tr("None"))
            : gapMenu->addAction(tr("%1 px").arg(g));
        a->setCheckable(true);
        a->setChecked(g == currentGap);
        gapGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, g]
        {
            m_gap = g;
            updateGeometry();
            update();
        });
    }

    menu.exec(event->globalPos());
}

void PieceMapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QPalette &pal = palette();
    const int cell = m_cellSize;
    const int gap  = m_gap;
    const int step = cell + gap;

    // Fill background with the empty-cell color so any partial row at the
    // bottom is invisible (no different-colored strip).
    const QColor bgEmpty      = pal.color(QPalette::Mid);
    painter.fillRect(rect(), bgEmpty);

    const int n = m_downloaded.size();
    if (n <= 0)
        return;

    const int cols  = std::max(1, (width()  + gap) / step);
    const int rows  = std::max(1, (height() + gap) / step);
    const int total = cols * rows;

    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QColor colDone      = dark ? QColor(0x34, 0x8a, 0x4a) : QColor(0x2d, 0xa4, 0x4e);
    const QColor colActive    = dark ? QColor(0xb0, 0x6a, 0x00) : QColor(0xf0, 0x88, 0x00);
    const QColor colAvailLow  = dark ? QColor(0x1a, 0x2e, 0x42) : QColor(0xaa, 0xd0, 0xf5);
    const QColor colAvailHigh = dark ? QColor(0x38, 0x8b, 0xfd) : QColor(0x09, 0x69, 0xda);

    const bool hasAvail       = !m_availability.isEmpty() && (m_availability.size() == n);
    const bool hasDownloading = !m_downloading.isEmpty() && (m_downloading.size() == n);

    for (int k = 0; k < total; ++k)
    {
        const int p0   = static_cast<int>(static_cast<double>(k)     * n / total);
        const int p1   = static_cast<int>(static_cast<double>(k + 1) * n / total);
        const int pEnd = std::min(n, std::max(p0 + 1, p1));

        int doneCount  = 0;
        bool anyActive = false;
        int availSum   = 0;
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

        // Column-major fill: first column top→bottom, then next column, etc.
        const int row = k % rows;
        const int col = k / rows;
        painter.fillRect(col * step, row * step, cell, cell, color);
    }
}
