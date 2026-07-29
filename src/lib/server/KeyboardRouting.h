/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

namespace deskflow::server {

struct KeyboardRoute
{
  bool action = false;
  bool modifier = false;
  bool sourceSupported = false;
  bool destinationProtocol = false;
  bool destinationActive = false;
  bool authoritativeRoute = false;
};

constexpr bool suppressPhysicalModifierEdge(const KeyboardRoute &route)
{
  return !route.action && route.modifier && route.sourceSupported && route.destinationProtocol &&
         route.destinationActive && route.authoritativeRoute;
}

} // namespace deskflow::server