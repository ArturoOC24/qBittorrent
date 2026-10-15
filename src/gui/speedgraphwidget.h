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

#pragma once

#include <QList>
#include <QWidget>

// Steam-style mini graph: network download rate as a filled area chart with
// the disk write activity overlaid as a second series.
class SpeedGraphWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SpeedGraphWidget)

public:
    explicit SpeedGraphWidget(QWidget *parent = nullptr);

    void addSample(qint64 downloadRate, qint64 uploadRate, qint64 diskWriteQueue);

private:
    void paintEvent(QPaintEvent *event) override;

    struct Sample
    {
        qint64 download = 0;
        qint64 upload = 0;
        qint64 disk = 0;
    };

    static const int MAX_SAMPLES = 90;
    QList<Sample> m_samples;
};
