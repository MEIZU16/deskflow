/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ServerProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void handleKeepAliveAlarm_timeout_queuesDisconnectRequest();
  void handleData_incompleteMessage_queuesDisconnectRequest();
  void parseHandshakeMessage_protocolError_queuesRefusalRequest();
  void keyboardStateSession_initialStateSeparatesAuthoritativeAndLegacyEntry();
  void keyboardStateSession_rejectsInactiveZeroAndStaleSequences();
  void keyboardStateSession_acceptsCurrentSequenceAndNormalizesState();
  void keyboardStateSession_invalidSnapshotKeepsAuthoritativeSessionWaiting();
  void keyboardStateSession_unsupportedSourceFallsBackToLegacyState();
  void keyboardStateProtocol_roundTripsAllWireFields();

private:
  Log m_log;
};
