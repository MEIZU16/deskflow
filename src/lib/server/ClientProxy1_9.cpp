/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events)
{
}

void ClientProxy1_9::keyboardState(const deskflow::KeyboardModifierState &state)
{
  const auto snapshot = deskflow::normalizedKeyboardModifierState(state);

  LOG_VERBOSE(
      "send keyboard state to \"%s\" seq=%u supported=%d valid=%d depressed=0x%04x latched=0x%04x locked=0x%04x group=%u",
      getName().c_str(), enterSequenceNumber(), snapshot.supported, snapshot.valid, snapshot.depressed,
      snapshot.latched, snapshot.locked, snapshot.group
  );
  ProtocolUtil::writef(
      getStream(), kMsgDKeyboardState, enterSequenceNumber(), snapshot.supported ? 1 : 0, snapshot.valid ? 1 : 0,
      snapshot.depressed, snapshot.latched, snapshot.locked, snapshot.group
  );
}