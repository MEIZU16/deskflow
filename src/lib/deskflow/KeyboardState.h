/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <cstdint>
#include <cstdlib>
#include <limits>

namespace deskflow {

inline constexpr KeyModifierMask kMomentaryModifierMask =
    KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierMeta | KeyModifierSuper | KeyModifierAltGr;
inline constexpr KeyModifierMask kLockModifierMask =
    KeyModifierCapsLock | KeyModifierNumLock | KeyModifierScrollLock;
inline constexpr KeyModifierMask kSynchronizableModifierMask = kMomentaryModifierMask | kLockModifierMask;
inline constexpr KeyModifierMask kKnownModifierMask = kSynchronizableModifierMask | KeyModifierLevel5Lock;

struct KeyboardModifierState
{
  KeyModifierMask depressed;
  KeyModifierMask latched;
  KeyModifierMask locked;
  std::uint32_t group;
  bool valid;
  bool supported = true;

  constexpr bool operator==(const KeyboardModifierState &) const = default;
};

constexpr std::uint32_t nextKeyboardSessionSequence(std::uint32_t current)
{
  return current == std::numeric_limits<std::uint32_t>::max() ? 1u : current + 1u;
}

constexpr KeyboardModifierState unsupportedKeyboardModifierState()
{
  return KeyboardModifierState{0, 0, 0, 0, false, false};
}

constexpr KeyboardModifierState neutralKeyboardModifierState(bool valid = true)
{
  return KeyboardModifierState{0, 0, 0, 0, valid, true};
}

constexpr KeyboardModifierState normalizedKeyboardModifierState(KeyboardModifierState state)
{
  if (!state.supported) {
    state.depressed = 0;
    state.latched = 0;
    state.locked &= kLockModifierMask;
    state.group = 0;
    return state;
  }
  if (!state.valid) {
    return neutralKeyboardModifierState(false);
  }

  state.depressed &= kKnownModifierMask;
  state.latched &= kKnownModifierMask;
  state.locked &= kKnownModifierMask;
  return state;
}

constexpr KeyModifierMask effectiveMomentaryModifiers(const KeyboardModifierState &state)
{
  return (state.depressed | state.latched | state.locked) & kMomentaryModifierMask;
}

constexpr KeyModifierMask effectiveLockModifiers(const KeyboardModifierState &state)
{
  return state.locked & kLockModifierMask;
}

constexpr KeyModifierMask effectiveKeyboardModifiers(const KeyboardModifierState &state)
{
  return effectiveMomentaryModifiers(state) | effectiveLockModifiers(state);
}

constexpr bool isMomentaryModifierKey(KeyID id)
{
  switch (id) {
  case kKeyShift_L:
  case kKeyShift_R:
  case kKeyControl_L:
  case kKeyControl_R:
  case kKeyAlt_L:
  case kKeyAlt_R:
  case kKeyMeta_L:
  case kKeyMeta_R:
  case kKeySuper_L:
  case kKeySuper_R:
  case kKeyAltGr:
    return true;

  default:
    return false;
  }
}

constexpr bool isLockModifierKey(KeyID id)
{
  switch (id) {
  case kKeyCapsLock:
  case kKeyNumLock:
  case kKeyScrollLock:
    return true;

  default:
    return false;
  }
}

constexpr bool isKeyboardModifierKey(KeyID id)
{
  return isMomentaryModifierKey(id) || isLockModifierKey(id);
}

inline KeyboardModifierState *allocKeyboardModifierState(const KeyboardModifierState &state)
{
  auto *data = static_cast<KeyboardModifierState *>(std::malloc(sizeof(KeyboardModifierState)));
  if (data != nullptr) {
    *data = state;
  }
  return data;
}

} // namespace deskflow