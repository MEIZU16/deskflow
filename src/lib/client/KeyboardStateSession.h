/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyboardState.h"

#include <cstdint>

namespace deskflow::client {

struct KeyboardStateSession
{
  std::uint32_t sequence = 0;
  KeyModifierMask legacyLockMask = 0;
  bool active = false;
  bool protocolCapable = false;
  bool authoritative = false;
  KeyboardModifierState state = unsupportedKeyboardModifierState();
};

constexpr KeyboardModifierState initialKeyboardState(bool authoritative, KeyModifierMask legacyMask)
{
  return authoritative
             ? neutralKeyboardModifierState(false)
             : KeyboardModifierState{0, 0, legacyMask & kLockModifierMask, 0, true, false};
}

inline bool beginKeyboardStateSession(
    KeyboardStateSession &session, std::uint32_t sequence, bool protocolCapable, KeyModifierMask legacyMask
)
{
  session.sequence = sequence;
  session.legacyLockMask = legacyMask & kLockModifierMask;
  session.active = sequence != 0;
  session.protocolCapable = protocolCapable;
  session.authoritative = protocolCapable;
  session.state = initialKeyboardState(protocolCapable, session.legacyLockMask);
  return session.active;
}

inline void endKeyboardStateSession(KeyboardStateSession &session)
{
  session = {};
}

inline bool acceptKeyboardState(
    KeyboardStateSession &session, std::uint32_t sequence, const KeyboardModifierState &state
)
{
  if (!session.active || !session.protocolCapable || sequence == 0 || sequence != session.sequence) {
    return false;
  }

  if (!state.supported) {
    session.authoritative = false;
    session.state = initialKeyboardState(false, session.legacyLockMask);
    return true;
  }

  session.authoritative = true;
  session.state = normalizedKeyboardModifierState(state);
  return true;
}

constexpr KeyModifierMask keyboardStateEventMask(
    const KeyboardStateSession &session, KeyModifierMask eventMask
)
{
  if (!session.active || !session.authoritative || !session.state.valid) {
    return eventMask;
  }

  return (eventMask & ~kSynchronizableModifierMask) | effectiveKeyboardModifiers(session.state);
}

} // namespace deskflow::client