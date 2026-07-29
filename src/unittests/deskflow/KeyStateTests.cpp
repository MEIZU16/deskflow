/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2011 Nick Bolton
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "KeyStateTests.h"
#include "base/EventQueue.h"
#include "deskflow/KeyboardState.h"
#include "deskflow/KeyMap.h"

#include "MockEventQueue.h"
#include "MockKeyMap.h"
#include "MockKeyState.h"

#include <algorithm>
#include <vector>

namespace {

using Keystroke = deskflow::KeyMap::Keystroke;

class RecordingKeyState : public KeyState
{
public:
  RecordingKeyState(IEventQueue *events, deskflow::KeyMap &keyMap)
      : KeyState(events, keyMap, {"en"}, true)
  {
  }

  void setInjectionAvailable(bool available)
  {
    m_injectionAvailable = available;
  }

  void failNextInjection()
  {
    m_failNextInjection = true;
  }

  void setPlatformModifiers(KeyModifierMask modifiers)
  {
    m_platformModifiers = modifiers;
  }

  const std::vector<Keystroke> &injected() const
  {
    return m_injected;
  }

  void clearInjected()
  {
    m_injected.clear();
  }

  KeyModifierMask pollActiveModifiers() const override
  {
    return m_platformModifiers;
  }

  int32_t pollActiveGroup() const override
  {
    return 0;
  }

  void pollPressedKeys(KeyButtonSet &) const override
  {
  }

  bool fakeCtrlAltDel() override
  {
    return false;
  }

protected:
  void getKeyMap(deskflow::KeyMap &) override
  {
  }

  bool fakeKey(const Keystroke &keystroke) override
  {
    if (m_failNextInjection) {
      m_failNextInjection = false;
      return false;
    }

    m_injected.push_back(keystroke);
    if (keystroke.m_type != Keystroke::KeyType::Button) {
      return true;
    }

    const auto modifier = modifierForButton(keystroke.m_data.m_button.m_button);
    if ((modifier & deskflow::kLockModifierMask) != 0) {
      if (keystroke.m_data.m_button.m_press) {
        m_platformModifiers ^= modifier;
      }
    } else if (keystroke.m_data.m_button.m_press) {
      m_platformModifiers |= modifier;
    } else {
      m_platformModifiers &= ~modifier;
    }
    return true;
  }

  bool isKeyInjectionAvailable() const override
  {
    return m_injectionAvailable;
  }

private:
  static KeyModifierMask modifierForButton(KeyButton button)
  {
    switch (button) {
    case 10:
      return KeyModifierShift;
    case 11:
      return KeyModifierControl;
    case 12:
      return KeyModifierCapsLock;
    default:
      return 0;
    }
  }

  bool m_injectionAvailable = true;
  bool m_failNextInjection = false;
  KeyModifierMask m_platformModifiers = 0;
  std::vector<Keystroke> m_injected;
};

deskflow::KeyMap keyboardSessionKeyMap()
{
  deskflow::KeyMap keyMap;
  const auto add = [&](KeyID id, KeyButton button) {
    deskflow::KeyMap::KeyItem item{};
    item.m_id = id;
    item.m_group = 0;
    item.m_button = button;
    deskflow::KeyMap::initModifierKey(item);
    keyMap.addKeyEntry(item);
  };
  add(kKeyShift_L, 10);
  add(kKeyControl_L, 11);
  add(kKeyCapsLock, 12);
  add(static_cast<KeyID>('a'), 30);
  add(static_cast<KeyID>('b'), 31);
  keyMap.finish();
  return keyMap;
}

std::size_t countButtonEvents(const RecordingKeyState &state, KeyButton button, bool press)
{
  return std::ranges::count_if(state.injected(), [button, press](const auto &keystroke) {
    return keystroke.m_type == Keystroke::KeyType::Button &&
           keystroke.m_data.m_button.m_button == button && keystroke.m_data.m_button.m_press == press;
  });
}

} // namespace

void KeyStateTests::initTestCase()
{
  m_arch.init();
}

void KeyStateTests::keyDown()
{
  deskflow::KeyMap keyMap;
  EventQueue eventQueue;
  MockKeyState keyState(eventQueue, keyMap);

  keyState.onKey(1, true, KeyModifierAlt);

  QVERIFY(keyState.getKeyState(1));
}

void KeyStateTests::keyUp()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);
  QVERIFY(!keyState.getKeyState(1));
}

void KeyStateTests::invalidKey()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  keyState.onKey(0, true, KeyModifierAlt);

  QVERIFY(!keyState.getKeyState(0));
}

void KeyStateTests::onKey_aKeyDown_keyStateOne()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  keyState.onKey(1, true, KeyModifierAlt);

  QVERIFY(keyState.getKeyState(1));
}

void KeyStateTests::onKey_aKeyUp_keyStateZero()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  keyState.onKey(1, false, KeyModifierAlt);

  QVERIFY(!keyState.getKeyState(1));
}

void KeyStateTests::onKey_invalidKey_keyStateZero()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  keyState.onKey(0, true, KeyModifierAlt);

  QVERIFY(!keyState.getKeyState(0));
}

void KeyStateTests::updateKeyState_pollDoesNothing_keyNotSet()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  keyState.updateKeyState();

  QVERIFY(!keyState.isKeyDown(1));
}

void KeyStateTests::updateKeyState_activeModifiers_maskNotSet()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  keyState.updateKeyState();

  QCOMPARE(0, keyState.getActiveModifiers());
}

void KeyStateTests::fakeKeyRepeat_invalidKey_returnsFalse()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  QVERIFY(!keyState.fakeKeyRepeat(0, 0, 0, 0, "en"));
}

void KeyStateTests::fakeKeyUp_buttonNotDown_returnsFalse()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  QVERIFY(!keyState.fakeKeyUp(0));
}

void KeyStateTests::isKeyDown_noKeysDown_returnsFalse()
{
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, m_keymap);

  QVERIFY(!keyState.isKeyDown(1));
}

void KeyStateTests::isKeyDown_keyDown_retrunsTrue()
{
  MockKeyMap keyMap;
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, keyMap);

  deskflow::KeyMap::KeyItem key;
  key.m_button = 1;
  keyState.fakeKeyDown(1, 0, 1, "en");

  QVERIFY(keyState.isKeyDown(1));
}

void KeyStateTests::updateKeyState_pollInsertsSingleKey_keyIsDown()
{
  MockKeyMap keyMap;
  MockEventQueue eventQueue;
  MockKeyState keyState(eventQueue, keyMap);

  deskflow::KeyMap::KeyItem key;
  key.m_button = 1;
  keyState.fakeKeyDown(1, 0, 1, "en");

  keyState.updateKeyState();
  QVERIFY(keyState.isKeyDown(1));
}

void KeyStateTests::keyboardState_projectionSeparatesMomentaryLocksAndUnsupportedBits()
{
  const deskflow::KeyboardModifierState state{
      static_cast<KeyModifierMask>(KeyModifierShift | KeyModifierLevel5Lock), KeyModifierControl,
      static_cast<KeyModifierMask>(KeyModifierAlt | KeyModifierCapsLock | KeyModifierNumLock), 5, true, true
  };

  QCOMPARE(
      deskflow::effectiveMomentaryModifiers(state),
      static_cast<KeyModifierMask>(KeyModifierShift | KeyModifierControl | KeyModifierAlt)
  );
  QCOMPARE(
      deskflow::effectiveLockModifiers(state),
      static_cast<KeyModifierMask>(KeyModifierCapsLock | KeyModifierNumLock)
  );
  QCOMPARE(
      deskflow::effectiveKeyboardModifiers(state),
      static_cast<KeyModifierMask>(
          KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierCapsLock | KeyModifierNumLock
      )
  );
}

void KeyStateTests::beginKeyboardSession_invalidInitialStateDefersOrdinaryKeys()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.setPlatformModifiers(KeyModifierCapsLock);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState(false));
  QCOMPARE(keyState.pollActiveModifiers() & deskflow::kLockModifierMask, KeyModifierCapsLock);
  QVERIFY(keyState.injected().empty());
  QVERIFY(!keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState(false)));
  QCOMPARE(keyState.pollActiveModifiers() & deskflow::kLockModifierMask, KeyModifierCapsLock);
  QVERIFY(keyState.injected().empty());

  keyState.fakeKeyDown(static_cast<KeyID>('a'), 0, 20, "en");
  QVERIFY(keyState.injected().empty());

  QVERIFY(keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState()));
  QCOMPARE(countButtonEvents(keyState, 12, true), static_cast<std::size_t>(1));
  QCOMPARE(countButtonEvents(keyState, 30, true), static_cast<std::size_t>(1));
}

void KeyStateTests::beginKeyboardSession_unsupportedSourceUsesLegacyEventModifiers()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState(false));

  const deskflow::KeyboardModifierState fallback{
      0, 0, KeyModifierCapsLock, 0, true, false
  };
  QVERIFY(keyState.reconcileKeyboardState(fallback));
  keyState.clearInjected();

  keyState.fakeKeyDown(static_cast<KeyID>('a'), KeyModifierShift, 20, "en");
  QCOMPARE(countButtonEvents(keyState, 10, true), static_cast<std::size_t>(1));
  QCOMPARE(countButtonEvents(keyState, 30, true), static_cast<std::size_t>(1));
}

void KeyStateTests::reconcileKeyboardState_isIdempotentAndReleasesMomentaryModifiers()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());
  keyState.clearInjected();

  const deskflow::KeyboardModifierState shift{KeyModifierShift, 0, 0, 0, true, true};
  QVERIFY(keyState.reconcileKeyboardState(shift));
  QCOMPARE(countButtonEvents(keyState, 10, true), static_cast<std::size_t>(1));
  QCOMPARE(keyState.getActiveModifiers(), KeyModifierShift);

  keyState.clearInjected();
  QVERIFY(keyState.reconcileKeyboardState(shift));
  QVERIFY(keyState.injected().empty());

  QVERIFY(keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState()));
  QCOMPARE(countButtonEvents(keyState, 10, false), static_cast<std::size_t>(1));
  QCOMPARE(keyState.getActiveModifiers(), static_cast<KeyModifierMask>(0));
}

void KeyStateTests::reconcileKeyboardState_invalidSnapshotClearsOwnershipAndKeepsKeysDeferred()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());
  QVERIFY(keyState.reconcileKeyboardState(
      deskflow::KeyboardModifierState{KeyModifierShift, 0, 0, 0, true, true}
  ));

  keyState.clearInjected();
  QVERIFY(!keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState(false)));
  QCOMPARE(countButtonEvents(keyState, 10, false), static_cast<std::size_t>(1));

  keyState.clearInjected();
  keyState.fakeKeyDown(static_cast<KeyID>('a'), 0, 20, "en");
  QVERIFY(keyState.injected().empty());

  QVERIFY(keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState()));
  QCOMPARE(countButtonEvents(keyState, 30, true), static_cast<std::size_t>(1));
}

void KeyStateTests::reconcileKeyboardState_injectionFailureDoesNotCommitLedger()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());
  keyState.clearInjected();

  keyState.failNextInjection();
  QVERIFY(!keyState.reconcileKeyboardState(
      deskflow::KeyboardModifierState{KeyModifierShift, 0, 0, 0, true, true}
  ));
  QCOMPARE(keyState.getActiveModifiers(), static_cast<KeyModifierMask>(0));
  QVERIFY(!keyState.hasSyntheticKeys());

  QVERIFY(keyState.restoreKeyboardSession());
  QCOMPARE(countButtonEvents(keyState, 10, true), static_cast<std::size_t>(1));
  QCOMPARE(keyState.getActiveModifiers(), KeyModifierShift);
}

void KeyStateTests::modifierOwnership_actionAndAuthoritativeLayersShareCanonicalButton()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());
  QVERIFY(keyState.reconcileKeyboardState(
      deskflow::KeyboardModifierState{KeyModifierShift, 0, 0, 0, true, true}
  ));

  keyState.clearInjected();
  keyState.fakeKeyDown(kKeySetModifiers, KeyModifierShift, 0, "en");
  QVERIFY(keyState.injected().empty());

  QVERIFY(keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState()));
  QVERIFY(keyState.injected().empty());
  QCOMPARE(keyState.getActiveModifiers(), KeyModifierShift);

  keyState.fakeKeyDown(kKeyClearModifiers, KeyModifierShift, 0, "en");
  QCOMPARE(countButtonEvents(keyState, 10, false), static_cast<std::size_t>(1));
  QCOMPARE(keyState.getActiveModifiers(), static_cast<KeyModifierMask>(0));
}

void KeyStateTests::keyboardRestorationQueue_replaysInArrivalOrder()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.setInjectionAvailable(false);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());

  keyState.fakeKeyDown(static_cast<KeyID>('a'), 0, 20, "en");
  keyState.fakeKeyDown(static_cast<KeyID>('b'), 0, 21, "en");
  keyState.fakeKeyUp(20);
  keyState.fakeKeyUp(21);
  QVERIFY(keyState.injected().empty());

  keyState.setInjectionAvailable(true);
  QVERIFY(keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState()));
  QCOMPARE(keyState.injected().size(), static_cast<std::size_t>(4));
  QCOMPARE(keyState.injected()[0].m_data.m_button.m_button, static_cast<KeyButton>(30));
  QVERIFY(keyState.injected()[0].m_data.m_button.m_press);
  QCOMPARE(keyState.injected()[1].m_data.m_button.m_button, static_cast<KeyButton>(31));
  QVERIFY(keyState.injected()[1].m_data.m_button.m_press);
  QCOMPARE(keyState.injected()[2].m_data.m_button.m_button, static_cast<KeyButton>(30));
  QVERIFY(!keyState.injected()[2].m_data.m_button.m_press);
  QCOMPARE(keyState.injected()[3].m_data.m_button.m_button, static_cast<KeyButton>(31));
  QVERIFY(!keyState.injected()[3].m_data.m_button.m_press);
}

void KeyStateTests::keyboardRestorationQueue_overflowDiscardsWholeTransaction()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.setInjectionAvailable(false);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());

  for (std::size_t i = 0; i < 1025; ++i) {
    keyState.fakeKeyDown(static_cast<KeyID>('a'), 0, static_cast<KeyButton>(i + 1), "en");
  }

  keyState.setInjectionAvailable(true);
  QVERIFY(keyState.reconcileKeyboardState(deskflow::neutralKeyboardModifierState()));
  QVERIFY(keyState.injected().empty());
}

void KeyStateTests::endKeyboardSession_restoresLocalLockBaseline()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.setPlatformModifiers(KeyModifierCapsLock);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());
  QCOMPARE(keyState.pollActiveModifiers() & deskflow::kLockModifierMask, static_cast<KeyModifierMask>(0));

  keyState.clearInjected();
  keyState.endKeyboardSession();
  QCOMPARE(countButtonEvents(keyState, 12, true), static_cast<std::size_t>(1));
  QCOMPARE(keyState.pollActiveModifiers() & deskflow::kLockModifierMask, KeyModifierCapsLock);
}

void KeyStateTests::resetKeyboardSession_discardsLedgerWithoutSyntheticRelease()
{
  MockEventQueue events;
  auto keyMap = keyboardSessionKeyMap();
  RecordingKeyState keyState(&events, keyMap);
  keyState.beginKeyboardSession(deskflow::neutralKeyboardModifierState());
  QVERIFY(keyState.reconcileKeyboardState(
      deskflow::KeyboardModifierState{KeyModifierShift, 0, 0, 0, true, true}
  ));

  keyState.clearInjected();
  keyState.setPlatformModifiers(0);
  keyState.resetKeyboardSession();
  QVERIFY(keyState.injected().empty());
  QVERIFY(!keyState.hasSyntheticKeys());
  QCOMPARE(keyState.getActiveModifiers(), static_cast<KeyModifierMask>(0));
}

QTEST_MAIN(KeyStateTests)
