/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "deskflow/KeyboardState.h"
#include "server/KeyboardRouting.h"
#include "server/Server.h"

#include <cstdint>
#include <limits>

void ServerTests::SwitchToScreenInfo_alloc_screen()
{
  auto actual = new Server::SwitchToScreenInfo("test");
  QCOMPARE(actual->m_screen, "test");
  delete actual;
}

void ServerTests::KeyboardBroadcastInfo_alloc_stateAndSceens()
{
  auto info = new Server::KeyboardBroadcastInfo(Server::KeyboardBroadcastInfo::State::kOn, "test");
  QCOMPARE(info->m_state, Server::KeyboardBroadcastInfo::State::kOn);
  QCOMPARE(info->m_screens, "test");
  delete info;
}

void ServerTests::keyboardSessionSequence_skipsZeroOnWraparound()
{
  QCOMPARE(deskflow::nextKeyboardSessionSequence(0), 1u);
  QCOMPARE(deskflow::nextKeyboardSessionSequence(41), 42u);
  QCOMPARE(
      deskflow::nextKeyboardSessionSequence(std::numeric_limits<std::uint32_t>::max()),
      1u
  );
}

void ServerTests::keyboardRouting_suppressesOnlyCapableActivePhysicalModifiers()
{
  const deskflow::server::KeyboardRoute activePhysicalModifier{
      .action = false,
      .modifier = true,
      .sourceSupported = true,
      .destinationProtocol = true,
      .destinationActive = true,
      .authoritativeRoute = true,
  };
  QVERIFY(deskflow::server::suppressPhysicalModifierEdge(activePhysicalModifier));

  auto route = activePhysicalModifier;
  route.action = true;
  QVERIFY(!deskflow::server::suppressPhysicalModifierEdge(route));
  route = activePhysicalModifier;
  route.modifier = false;
  QVERIFY(!deskflow::server::suppressPhysicalModifierEdge(route));
  route = activePhysicalModifier;
  route.sourceSupported = false;
  QVERIFY(!deskflow::server::suppressPhysicalModifierEdge(route));
  route = activePhysicalModifier;
  route.destinationProtocol = false;
  QVERIFY(!deskflow::server::suppressPhysicalModifierEdge(route));
  route = activePhysicalModifier;
  route.destinationActive = false;
  QVERIFY(!deskflow::server::suppressPhysicalModifierEdge(route));
  route = activePhysicalModifier;
  route.authoritativeRoute = false;
  QVERIFY(!deskflow::server::suppressPhysicalModifierEdge(route));
}

QTEST_MAIN(ServerTests)
