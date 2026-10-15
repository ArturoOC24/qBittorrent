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

#include "collapsiblesplitter.h"

#include <QResizeEvent>
#include <QToolButton>

using namespace Qt::Literals::StringLiterals;

CollapsibleSplitterHandle::CollapsibleSplitterHandle(const Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
{
    m_toggleButton = new QToolButton(this);
    m_toggleButton->setObjectName(u"propToggleHandle"_s);
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setFocusPolicy(Qt::NoFocus);
    m_toggleButton->setFixedSize(58, 15);
}

QToolButton *CollapsibleSplitterHandle::toggleButton() const
{
    return m_toggleButton;
}

void CollapsibleSplitterHandle::resizeEvent(QResizeEvent *event)
{
    QSplitterHandle::resizeEvent(event);
    // Center horizontally and align to the bottom so the handle sits flush
    // against the properties panel below, looking like a tab attached to it
    const int x = (width() - m_toggleButton->width()) / 2;
    const int y = height() - m_toggleButton->height();
    m_toggleButton->move(x, y);
}

CollapsibleSplitter::CollapsibleSplitter(const Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
}

QSplitterHandle *CollapsibleSplitter::createHandle()
{
    return new CollapsibleSplitterHandle(orientation(), this);
}
