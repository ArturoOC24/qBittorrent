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

#pragma once

#include <QBitArray>
#include <QList>
#include <QWidget>

#include "base/settingvalue.h"

namespace BitTorrent
{
    class Torrent;
}

// 2D grid map of torrent pieces.
// Each cell represents one or more pieces. Color encodes download state + availability.
// Cell size (granularity) and gap are configurable via right-click context menu.
class PieceMapWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PieceMapWidget)

public:
    explicit PieceMapWidget(QWidget *parent = nullptr);

    void setTorrent(BitTorrent::Torrent *torrent);
    void setProgress(const QBitArray &downloaded, const QBitArray &downloading);
    void setAvailability(const QList<int> &availability);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    QSize sizeHint() const override;

private:
    BitTorrent::Torrent *m_torrent = nullptr;
    QBitArray m_downloaded;
    QBitArray m_downloading;
    QList<int> m_availability;
    int m_maxAvailability = 1;

    CachedSettingValue<int> m_cellSize;
    CachedSettingValue<int> m_gap;
};
