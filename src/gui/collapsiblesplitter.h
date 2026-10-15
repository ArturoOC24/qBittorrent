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

#include <QSplitter>
#include <QSplitterHandle>

class QToolButton;

// A splitter handle carrying a centered pill button. Because the handle sits
// between the two widgets and always stays visible, its button remains usable
// even when the bottom pane is fully collapsed.
class CollapsibleSplitterHandle final : public QSplitterHandle
{
    Q_OBJECT

public:
    CollapsibleSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

    QToolButton *toggleButton() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QToolButton *m_toggleButton = nullptr;
};

class CollapsibleSplitter final : public QSplitter
{
    Q_OBJECT

public:
    CollapsibleSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    QSplitterHandle *createHandle() override;
};
