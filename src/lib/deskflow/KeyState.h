/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IKeyState.h"
#include "deskflow/KeyMap.h"

#include <array>
#include <deque>

//! Core key state
/*!
This class provides key state services.  Subclasses must implement a few
platform specific methods.
*/
class KeyState : public IKeyState
{
public:
  KeyState(IEventQueue *events, std::vector<std::string> layouts, bool isLangSyncEnabled);
  KeyState(IEventQueue *events, deskflow::KeyMap &keyMap, std::vector<std::string> layouts, bool isLangSyncEnabled);
  ~KeyState() override;

  //! @name manipulators
  //@{

  //! Handle key event
  /*!
  Sets the state of \p button to down or up and updates the current
  modifier state to \p newState.  This method should be called by
  primary screens only in response to local events.  For auto-repeat
  set \p down to \c true.  Overrides must forward to the superclass.
  */
  virtual void onKey(KeyButton button, bool down, KeyModifierMask newState);

  //! Post a key event
  /*!
  Posts a key event.  This may adjust the event or post additional
  events in some circumstances.  If this is overridden it must forward
  to the superclass.
  */
  virtual void sendKeyEvent(
      void *target, bool press, bool isAutoRepeat, KeyID key, KeyModifierMask mask, int32_t count, KeyButton button
  );

  //@}
  //! @name accessors
  //@{

  //@}

  void updateKeyMap(deskflow::KeyMap *existing);
  // IKeyState overrides
  void updateKeyMap() override
  {
    this->updateKeyMap(nullptr);
  }
  void updateKeyState() override;
  void setHalfDuplexMask(KeyModifierMask) override;
  void fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang) override;
  bool fakeKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang) override;
  bool fakeKeyUp(KeyButton button) override;
  void fakeAllKeysUp() override;
  void beginKeyboardSession(const deskflow::KeyboardModifierState &initialState) override;
  bool reconcileKeyboardState(const deskflow::KeyboardModifierState &state) override;
  void endKeyboardSession() override;
  void resetKeyboardSession() override;
  bool restoreKeyboardSession() override;
  bool fakeMediaKey(KeyID id) override;

  bool isKeyDown(KeyButton) const override;
  KeyModifierMask getActiveModifiers() const override;
  bool hasSyntheticKeys() const;
  // Left abstract
  bool fakeCtrlAltDel() override = 0;
  KeyModifierMask pollActiveModifiers() const override = 0;
  int32_t pollActiveGroup() const override = 0;
  void pollPressedKeys(KeyButtonSet &pressedKeys) const override = 0;

  int32_t getKeyState(KeyButton keyButton) const
  {
    return m_keys[keyButton];
  }

protected:
  using Keystroke = deskflow::KeyMap::Keystroke;

  //! @name protected manipulators
  //@{

  //! Get the keyboard map
  /*!
  Fills \p keyMap with the current keyboard map.
  */
  virtual void getKeyMap(deskflow::KeyMap &keyMap) = 0;

  //! Fake a key event
  /*!
  Synthesize an event for \p keystroke. Returns false when the platform
  cannot accept the event, so the synthetic ledger is not committed.
  */
  virtual bool fakeKey(const Keystroke &keystroke) = 0;

  //! Test whether platform injection is currently available
  virtual bool isKeyInjectionAvailable() const
  {
    return true;
  }

  //! Get the active modifiers
  /*!
  Returns the modifiers that are currently active according to our
  shadowed state.  The state may be modified.
  */
  virtual KeyModifierMask &getActiveModifiersRValue();

  //@}
  //! @name protected accessors
  //@{

  //! Compute a group number
  /*!
  Returns the number of the group \p offset groups after group \p group.
  */
  int32_t getEffectiveGroup(int32_t group, int32_t offset) const;

  //! Check if key is ignored
  /*!
  Returns \c true if and only if the key should always be ignored.
  The default returns \c true only for the toggle keys.
  */
  virtual bool isIgnoredKey(KeyID key, KeyModifierMask mask) const;

  //! Get button for a KeyID
  /*!
  Return the button mapped to key \p id in group \p group if any,
  otherwise returns 0.
  */
  KeyButton getButton(KeyID id, int32_t group) const;

  //@}

private:
  using Keystrokes = deskflow::KeyMap::Keystrokes;
  using ModifierToKeys = deskflow::KeyMap::ModifierToKeys;

public:
  struct AddActiveModifierContext
  {
  public:
    AddActiveModifierContext(int32_t group, KeyModifierMask mask, ModifierToKeys &activeModifiers);

  public:
    int32_t m_activeGroup;
    KeyModifierMask m_mask;
    ModifierToKeys &m_activeModifiers;

  private:
    // not implemented
    AddActiveModifierContext(const AddActiveModifierContext &);
    AddActiveModifierContext &operator=(const AddActiveModifierContext &);
  };

private:
  // not implemented
  KeyState(const KeyState &);
  KeyState &operator=(const KeyState &);

  // called by all ctors.
  void init();

  // adds alias key sequences.  these are sequences that are equivalent
  // to other sequences.
  void addAliasEntries();

  // adds non-keypad key sequences for keypad KeyIDs
  void addKeypadEntries();

  // adds key sequences for combination KeyIDs (those built using
  // dead keys)
  void addCombinationEntries();

  // synthesize key events.  synthesize auto-repeat events count times.
  bool fakeKeys(const Keystrokes &, uint32_t count);

  enum class PendingKeyEventType
  {
    Down,
    Repeat,
    Up
  };

  struct PendingKeyEvent
  {
    PendingKeyEventType type;
    KeyID id = kKeyNone;
    KeyModifierMask mask = 0;
    int32_t count = 1;
    KeyButton button = 0;
    std::string lang;
  };

  enum class KeyEventResult
  {
    Consumed,
    Injected,
    Retry
  };

  bool deferKeyEvent(PendingKeyEvent event);
  bool replayPendingKeyEvents();
  void clearPendingKeyEvents();
  KeyEventResult fakeKeyDownNow(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang);
  KeyEventResult fakeKeyRepeatNow(
      KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang
  );
  KeyEventResult fakeKeyUpNow(KeyButton button);

  struct ModifierReconcileResult
  {
    bool injected = false;
    bool complete = false;
  };

  void clearSyntheticState();
  KeyModifierMask keyEventModifierMask(KeyModifierMask eventMask) const;
  ModifierReconcileResult reconcileModifierLayer(
      ModifierToKeys &layer, KeyModifierMask &layerMask, KeyModifierMask desiredMask, bool includeLocks,
      const char *layerName
  );
  void replaceModifierLayer(
      ModifierToKeys &combined, const ModifierToKeys &oldLayer, const ModifierToKeys &newLayer
  ) const;
  void applyModifierReferenceDelta(
      const ModifierToKeys &oldModifiers, const ModifierToKeys &newModifiers, KeyButton excludedButton1 = 0,
      KeyButton excludedButton2 = 0
  );
  bool eraseClientModifier(ModifierToKeys &modifiers, KeyButton button) const;
  void refreshClientModifierLayer();
  void retainActionModifiers(KeyModifierMask mask);
  void releaseActionModifiers(KeyModifierMask mask);
  void recomputeActiveModifierMask();

  // active modifiers collection callback
  static void addActiveModifierCB(KeyID id, int32_t group, deskflow::KeyMap::KeyItem &keyItem, void *vcontext);

private:
  // must be declared before m_keyMap. used when this class owns the key map.
  deskflow::KeyMap *m_keyMapPtr;

  // the keyboard map
  deskflow::KeyMap &m_keyMap;

  // current modifier state
  KeyModifierMask m_mask;

  // Modifier ownership is split by source. The combined map is used by
  // KeyMap, while each layer decides whether a physical button may be
  // released when one source drops its reference.
  ModifierToKeys m_activeModifiers;
  ModifierToKeys m_authoritativeModifiers;
  ModifierToKeys m_actionModifiers;
  ModifierToKeys m_clientModifiers;
  KeyModifierMask m_authoritativeMask = 0;
  KeyModifierMask m_actionModifierMask = 0;
  KeyModifierMask m_actionModifierRefs = 0;
  std::array<std::uint32_t, kKeyModifierNumBits> m_actionModifierRefCounts{};

  // current keyboard state (> 0 if pressed, 0 otherwise).  this is
  // initialized to the keyboard state according to the system then
  // it tracks synthesized events.
  int32_t m_keys[s_numButtons];

  // synthetic keyboard state (> 0 if pressed, 0 otherwise).  this
  // tracks the synthesized keyboard state.  if m_keys[n] > 0 but
  // m_syntheticKeys[n] == 0 then the key was pressed locally and
  // not synthesized yet.
  int32_t m_syntheticKeys[s_numButtons];

  // client data for each pressed key
  uint32_t m_keyClientData[s_numButtons];

  // server keyboard state.  an entry is 0 if not the key isn't pressed
  // otherwise it's the local KeyButton synthesized for the server key.
  KeyButton m_serverKeys[s_numButtons];

  IEventQueue *m_events;

  bool m_isLangSyncEnabled;
  bool m_keyboardSessionActive = false;
  bool m_keyboardSessionAuthoritative = false;
  bool m_keyboardStateRestored = false;
  bool m_authoritativeStateOwned = false;
  KeyModifierMask m_keyboardSessionLockBaseline = 0;
  deskflow::KeyboardModifierState m_desiredKeyboardState = deskflow::neutralKeyboardModifierState(false);
  std::deque<PendingKeyEvent> m_pendingKeyEvents;
  bool m_replayingPendingKeyEvents = false;
  bool m_pendingKeyEventsOverflowed = false;
  static constexpr std::size_t s_maxPendingKeyEvents = 1024;
};
