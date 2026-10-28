/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IComputer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace deskflow::server {

using MonitorRect = IComputer::MonitorRect;

inline bool monitorContains(const MonitorRect &m, int32_t x, int32_t y)
{
  return x >= m.x && x < m.x + m.w && y >= m.y && y < m.y + m.h;
}

//! True if (x, y) lies on any monitor
inline bool anyMonitorContains(const std::vector<MonitorRect> &monitors, int32_t x, int32_t y)
{
  for (const auto &m : monitors) {
    if (monitorContains(m, x, y)) {
      return true;
    }
  }
  return false;
}

//! Exposed edges under the cursor
/*!
An edge is "exposed" where the pixel just past it does not belong to another
monitor of the same computer. Unlike the bounding-box test, this finds inner
edges such as the bottom of a top-left monitor that has nothing below it.
Fills \\c left/right/top/bottom and the monitor that contains the cursor.
Returns false if the cursor is not on any monitor.
*/
struct ExposedEdges
{
  bool left = false;
  bool right = false;
  bool top = false;
  bool bottom = false;
  MonitorRect monitor{};
};

inline bool exposedEdgesAt(
    const std::vector<MonitorRect> &monitors, int32_t x, int32_t y, int32_t zone, ExposedEdges &out
)
{
  for (const auto &m : monitors) {
    if (!monitorContains(m, x, y)) {
      continue;
    }
    out = ExposedEdges{};
    out.monitor = m;
    out.left = x < m.x + zone && !anyMonitorContains(monitors, m.x - 1, y);
    out.right = x >= m.x + m.w - zone && !anyMonitorContains(monitors, m.x + m.w, y);
    out.top = y < m.y + zone && !anyMonitorContains(monitors, x, m.y - 1);
    out.bottom = y >= m.y + m.h - zone && !anyMonitorContains(monitors, x, m.y + m.h);
    return true;
  }
  return false;
}

//! Move a point that is off every monitor onto the nearest one
/*!
Points already on a monitor are left alone. If a travel direction is given,
the point slides that way onto the first monitor in its path. Otherwise the
point is clamped
into the nearest monitor, kept \c inset pixels away from its edges (or to
its centre, for a monitor smaller than twice the inset).
*/
inline void snapToMonitors(
    const std::vector<MonitorRect> &monitors, int32_t inset, int32_t &x, int32_t &y, int32_t dirX = 0, int32_t dirY = 0
)
{
  if (monitors.empty() || anyMonitorContains(monitors, x, y)) {
    return;
  }

  // first, keep travelling in the direction the cursor was moving and take
  // the first monitor it reaches, so entering from the left lands on the
  // monitor whose left edge is exposed at that height, not whichever is
  // closest as the crow flies.
  if (dirX != 0 || dirY != 0) {
    const MonitorRect *hit = nullptr;
    int64_t hitDist = 0;
    for (const auto &m : monitors) {
      int64_t d = -1;
      if (dirX > 0 && y >= m.y && y < m.y + m.h && m.x > x) {
        d = m.x - x;
      } else if (dirX < 0 && y >= m.y && y < m.y + m.h && m.x + m.w <= x) {
        d = x - (m.x + m.w - 1);
      } else if (dirY > 0 && x >= m.x && x < m.x + m.w && m.y > y) {
        d = m.y - y;
      } else if (dirY < 0 && x >= m.x && x < m.x + m.w && m.y + m.h <= y) {
        d = y - (m.y + m.h - 1);
      }
      if (d >= 0 && (hit == nullptr || d < hitDist)) {
        hit = &m;
        hitDist = d;
      }
    }
    if (hit != nullptr) {
      const int32_t pad = inset;
      if (dirX > 0) {
        x = hit->x + std::min(pad, hit->w / 2);
      } else if (dirX < 0) {
        x = hit->x + hit->w - 1 - std::min(pad, hit->w / 2);
      } else if (dirY > 0) {
        y = hit->y + std::min(pad, hit->h / 2);
      } else {
        y = hit->y + hit->h - 1 - std::min(pad, hit->h / 2);
      }
      return;
    }
  }

  const auto clampAxis = [](int32_t v, int32_t lo, int32_t len, int32_t pad) {
    if (len <= 2 * pad) {
      return lo + len / 2;
    }
    if (v < lo + pad) {
      return lo + pad;
    }
    if (v > lo + len - 1 - pad) {
      return lo + len - 1 - pad;
    }
    return v;
  };

  int64_t best = -1;
  int32_t bx = x;
  int32_t by = y;
  for (const auto &m : monitors) {
    const int32_t cx = clampAxis(x, m.x, m.w, inset);
    const int32_t cy = clampAxis(y, m.y, m.h, inset);
    const int64_t ddx = static_cast<int64_t>(cx) - x;
    const int64_t ddy = static_cast<int64_t>(cy) - y;
    const int64_t d = ddx * ddx + ddy * ddy;
    if (best < 0 || d < best) {
      best = d;
      bx = cx;
      by = cy;
    }
  }
  x = bx;
  y = by;
}

} // namespace deskflow::server
