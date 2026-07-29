/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/KeyState.h"
#include "base/Log.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <list>

static const KeyButton kButtonMask = (KeyButton)(IKeyState::s_numButtons - 1);

namespace {

using ModifierToKeys = deskflow::KeyMap::ModifierToKeys;

bool sameModifierEntry(const ModifierToKeys::value_type &left, const ModifierToKeys::value_type &right)
{
  return left.first == right.first && left.second == right.second;
}

ModifierToKeys subtractModifierLayer(const ModifierToKeys &combined, const ModifierToKeys &layer)
{
  ModifierToKeys remainingLayer = layer;
  ModifierToKeys result;
  for (const auto &entry : combined) {
    const auto match = std::ranges::find_if(remainingLayer, [&entry](const auto &candidate) {
      return sameModifierEntry(entry, candidate);
    });
    if (match != remainingLayer.end()) {
      remainingLayer.erase(match);
    } else {
      result.insert(entry);
    }
  }
  return result;
}

KeyModifierMask modifierMask(const ModifierToKeys &modifiers, bool includeLocks)
{
  KeyModifierMask mask = 0;
  for (const auto &[modifier, keyItem] : modifiers) {
    if (includeLocks || !keyItem.m_lock) {
      mask |= modifier;
    }
  }
  return mask;
}

std::size_t buttonReferenceCount(const ModifierToKeys &modifiers, KeyButton button)
{
  return std::ranges::count_if(modifiers, [button](const auto &entry) {
    return !entry.second.m_lock && entry.second.m_button == button;
  });
}

} // namespace

static const KeyID s_decomposeTable[] = {
    // spacing version of dead keys
    0x0060, 0x0300, 0x0020,
    0, // grave,        dead_grave,       space
    0x00b4, 0x0301, 0x0020,
    0, // acute,        dead_acute,       space
    0x005e, 0x0302, 0x0020,
    0, // asciicircum,  dead_circumflex,  space
    0x007e, 0x0303, 0x0020,
    0, // asciitilde,   dead_tilde,       space
    0x00a8, 0x0308, 0x0020,
    0, // diaeresis,    dead_diaeresis,   space
    0x00b0, 0x030a, 0x0020,
    0, // degree,       dead_abovering,   space
    0x00b8, 0x0327, 0x0020,
    0, // cedilla,      dead_cedilla,     space
    0x02db, 0x0328, 0x0020,
    0, // ogonek,       dead_ogonek,      space
    0x02c7, 0x030c, 0x0020,
    0, // caron,        dead_caron,       space
    0x02d9, 0x0307, 0x0020,
    0, // abovedot,     dead_abovedot,    space
    0x02dd, 0x030b, 0x0020,
    0, // doubleacute,  dead_doubleacute, space
    0x02d8, 0x0306, 0x0020,
    0, // breve,        dead_breve,       space
    0x00af, 0x0304, 0x0020,
    0, // macron,       dead_macron,      space

    // Latin-1 (ISO 8859-1)
    0x00c0, 0x0300, 0x0041,
    0, // Agrave,       dead_grave,       A
    0x00c1, 0x0301, 0x0041,
    0, // Aacute,       dead_acute,       A
    0x00c2, 0x0302, 0x0041,
    0, // Acircumflex,  dead_circumflex,  A
    0x00c3, 0x0303, 0x0041,
    0, // Atilde,       dead_tilde,       A
    0x00c4, 0x0308, 0x0041,
    0, // Adiaeresis,   dead_diaeresis,   A
    0x00c5, 0x030a, 0x0041,
    0, // Aring,        dead_abovering,   A
    0x00c7, 0x0327, 0x0043,
    0, // Ccedilla,     dead_cedilla,     C
    0x00c8, 0x0300, 0x0045,
    0, // Egrave,       dead_grave,       E
    0x00c9, 0x0301, 0x0045,
    0, // Eacute,       dead_acute,       E
    0x00ca, 0x0302, 0x0045,
    0, // Ecircumflex,  dead_circumflex,  E
    0x00cb, 0x0308, 0x0045,
    0, // Ediaeresis,   dead_diaeresis,   E
    0x00cc, 0x0300, 0x0049,
    0, // Igrave,       dead_grave,       I
    0x00cd, 0x0301, 0x0049,
    0, // Iacute,       dead_acute,       I
    0x00ce, 0x0302, 0x0049,
    0, // Icircumflex,  dead_circumflex,  I
    0x00cf, 0x0308, 0x0049,
    0, // Idiaeresis,   dead_diaeresis,   I
    0x00d1, 0x0303, 0x004e,
    0, // Ntilde,       dead_tilde,       N
    0x00d2, 0x0300, 0x004f,
    0, // Ograve,       dead_grave,       O
    0x00d3, 0x0301, 0x004f,
    0, // Oacute,       dead_acute,       O
    0x00d4, 0x0302, 0x004f,
    0, // Ocircumflex,  dead_circumflex,  O
    0x00d5, 0x0303, 0x004f,
    0, // Otilde,       dead_tilde,       O
    0x00d6, 0x0308, 0x004f,
    0, // Odiaeresis,   dead_diaeresis,   O
    0x00d9, 0x0300, 0x0055,
    0, // Ugrave,       dead_grave,       U
    0x00da, 0x0301, 0x0055,
    0, // Uacute,       dead_acute,       U
    0x00db, 0x0302, 0x0055,
    0, // Ucircumflex,  dead_circumflex,  U
    0x00dc, 0x0308, 0x0055,
    0, // Udiaeresis,   dead_diaeresis,   U
    0x00dd, 0x0301, 0x0059,
    0, // Yacute,       dead_acute,       Y
    0x00e0, 0x0300, 0x0061,
    0, // agrave,       dead_grave,       a
    0x00e1, 0x0301, 0x0061,
    0, // aacute,       dead_acute,       a
    0x00e2, 0x0302, 0x0061,
    0, // acircumflex,  dead_circumflex,  a
    0x00e3, 0x0303, 0x0061,
    0, // atilde,       dead_tilde,       a
    0x00e4, 0x0308, 0x0061,
    0, // adiaeresis,   dead_diaeresis,   a
    0x00e5, 0x030a, 0x0061,
    0, // aring,        dead_abovering,   a
    0x00e7, 0x0327, 0x0063,
    0, // ccedilla,     dead_cedilla,     c
    0x00e8, 0x0300, 0x0065,
    0, // egrave,       dead_grave,       e
    0x00e9, 0x0301, 0x0065,
    0, // eacute,       dead_acute,       e
    0x00ea, 0x0302, 0x0065,
    0, // ecircumflex,  dead_circumflex,  e
    0x00eb, 0x0308, 0x0065,
    0, // ediaeresis,   dead_diaeresis,   e
    0x00ec, 0x0300, 0x0069,
    0, // igrave,       dead_grave,       i
    0x00ed, 0x0301, 0x0069,
    0, // iacute,       dead_acute,       i
    0x00ee, 0x0302, 0x0069,
    0, // icircumflex,  dead_circumflex,  i
    0x00ef, 0x0308, 0x0069,
    0, // idiaeresis,   dead_diaeresis,   i
    0x00f1, 0x0303, 0x006e,
    0, // ntilde,       dead_tilde,       n
    0x00f2, 0x0300, 0x006f,
    0, // ograve,       dead_grave,       o
    0x00f3, 0x0301, 0x006f,
    0, // oacute,       dead_acute,       o
    0x00f4, 0x0302, 0x006f,
    0, // ocircumflex,  dead_circumflex,  o
    0x00f5, 0x0303, 0x006f,
    0, // otilde,       dead_tilde,       o
    0x00f6, 0x0308, 0x006f,
    0, // odiaeresis,   dead_diaeresis,   o
    0x00f9, 0x0300, 0x0075,
    0, // ugrave,       dead_grave,       u
    0x00fa, 0x0301, 0x0075,
    0, // uacute,       dead_acute,       u
    0x00fb, 0x0302, 0x0075,
    0, // ucircumflex,  dead_circumflex,  u
    0x00fc, 0x0308, 0x0075,
    0, // udiaeresis,   dead_diaeresis,   u
    0x00fd, 0x0301, 0x0079,
    0, // yacute,       dead_acute,       y
    0x00ff, 0x0308, 0x0079,
    0, // ydiaeresis,   dead_diaeresis,   y

    // Latin-2 (ISO 8859-2)
    0x0104, 0x0328, 0x0041,
    0, // Aogonek,      dead_ogonek,      A
    0x013d, 0x030c, 0x004c,
    0, // Lcaron,       dead_caron,       L
    0x015a, 0x0301, 0x0053,
    0, // Sacute,       dead_acute,       S
    0x0160, 0x030c, 0x0053,
    0, // Scaron,       dead_caron,       S
    0x015e, 0x0327, 0x0053,
    0, // Scedilla,     dead_cedilla,     S
    0x0164, 0x030c, 0x0054,
    0, // Tcaron,       dead_caron,       T
    0x0179, 0x0301, 0x005a,
    0, // Zacute,       dead_acute,       Z
    0x017d, 0x030c, 0x005a,
    0, // Zcaron,       dead_caron,       Z
    0x017b, 0x0307, 0x005a,
    0, // Zabovedot,    dead_abovedot,    Z
    0x0105, 0x0328, 0x0061,
    0, // aogonek,      dead_ogonek,      a
    0x013e, 0x030c, 0x006c,
    0, // lcaron,       dead_caron,       l
    0x015b, 0x0301, 0x0073,
    0, // sacute,       dead_acute,       s
    0x0161, 0x030c, 0x0073,
    0, // scaron,       dead_caron,       s
    0x015f, 0x0327, 0x0073,
    0, // scedilla,     dead_cedilla,     s
    0x0165, 0x030c, 0x0074,
    0, // tcaron,       dead_caron,       t
    0x017a, 0x0301, 0x007a,
    0, // zacute,       dead_acute,       z
    0x017e, 0x030c, 0x007a,
    0, // zcaron,       dead_caron,       z
    0x017c, 0x0307, 0x007a,
    0, // zabovedot,    dead_abovedot,    z
    0x0154, 0x0301, 0x0052,
    0, // Racute,       dead_acute,       R
    0x0102, 0x0306, 0x0041,
    0, // Abreve,       dead_breve,       A
    0x0139, 0x0301, 0x004c,
    0, // Lacute,       dead_acute,       L
    0x0106, 0x0301, 0x0043,
    0, // Cacute,       dead_acute,       C
    0x010c, 0x030c, 0x0043,
    0, // Ccaron,       dead_caron,       C
    0x0118, 0x0328, 0x0045,
    0, // Eogonek,      dead_ogonek,      E
    0x011a, 0x030c, 0x0045,
    0, // Ecaron,       dead_caron,       E
    0x010e, 0x030c, 0x0044,
    0, // Dcaron,       dead_caron,       D
    0x0143, 0x0301, 0x004e,
    0, // Nacute,       dead_acute,       N
    0x0147, 0x030c, 0x004e,
    0, // Ncaron,       dead_caron,       N
    0x0150, 0x030b, 0x004f,
    0, // Odoubleacute, dead_doubleacute, O
    0x0158, 0x030c, 0x0052,
    0, // Rcaron,       dead_caron,       R
    0x016e, 0x030a, 0x0055,
    0, // Uring,        dead_abovering,   U
    0x0170, 0x030b, 0x0055,
    0, // Udoubleacute, dead_doubleacute, U
    0x0162, 0x0327, 0x0054,
    0, // Tcedilla,     dead_cedilla,     T
    0x0155, 0x0301, 0x0072,
    0, // racute,       dead_acute,       r
    0x0103, 0x0306, 0x0061,
    0, // abreve,       dead_breve,       a
    0x013a, 0x0301, 0x006c,
    0, // lacute,       dead_acute,       l
    0x0107, 0x0301, 0x0063,
    0, // cacute,       dead_acute,       c
    0x010d, 0x030c, 0x0063,
    0, // ccaron,       dead_caron,       c
    0x0119, 0x0328, 0x0065,
    0, // eogonek,      dead_ogonek,      e
    0x011b, 0x030c, 0x0065,
    0, // ecaron,       dead_caron,       e
    0x010f, 0x030c, 0x0064,
    0, // dcaron,       dead_caron,       d
    0x0144, 0x0301, 0x006e,
    0, // nacute,       dead_acute,       n
    0x0148, 0x030c, 0x006e,
    0, // ncaron,       dead_caron,       n
    0x0151, 0x030b, 0x006f,
    0, // odoubleacute, dead_doubleacute, o
    0x0159, 0x030c, 0x0072,
    0, // rcaron,       dead_caron,       r
    0x016f, 0x030a, 0x0075,
    0, // uring,        dead_abovering,   u
    0x0171, 0x030b, 0x0075,
    0, // udoubleacute, dead_doubleacute, u
    0x0163, 0x0327, 0x0074,
    0, // tcedilla,     dead_cedilla,     t

    // Latin-3 (ISO 8859-3)
    0x0124, 0x0302, 0x0048,
    0, // Hcircumflex,  dead_circumflex,  H
    0x0130, 0x0307, 0x0049,
    0, // Iabovedot,    dead_abovedot,    I
    0x011e, 0x0306, 0x0047,
    0, // Gbreve,        dead_breve,       G
    0x0134, 0x0302, 0x004a,
    0, // Jcircumflex,  dead_circumflex,  J
    0x0125, 0x0302, 0x0068,
    0, // hcircumflex,  dead_circumflex,  h
    0x011f, 0x0306, 0x0067,
    0, // gbreve,        dead_breve,       g
    0x0135, 0x0302, 0x006a,
    0, // jcircumflex,  dead_circumflex,  j
    0x010a, 0x0307, 0x0043,
    0, // Cabovedot,    dead_abovedot,    C
    0x0108, 0x0302, 0x0043,
    0, // Ccircumflex,  dead_circumflex,  C
    0x0120, 0x0307, 0x0047,
    0, // Gabovedot,    dead_abovedot,    G
    0x011c, 0x0302, 0x0047,
    0, // Gcircumflex,  dead_circumflex,  G
    0x016c, 0x0306, 0x0055,
    0, // Ubreve,        dead_breve,       U
    0x015c, 0x0302, 0x0053,
    0, // Scircumflex,  dead_circumflex,  S
    0x010b, 0x0307, 0x0063,
    0, // cabovedot,    dead_abovedot,    c
    0x0109, 0x0302, 0x0063,
    0, // ccircumflex,  dead_circumflex,  c
    0x0121, 0x0307, 0x0067,
    0, // gabovedot,    dead_abovedot,    g
    0x011d, 0x0302, 0x0067,
    0, // gcircumflex,  dead_circumflex,  g
    0x016d, 0x0306, 0x0075,
    0, // ubreve,        dead_breve,       u
    0x015d, 0x0302, 0x0073,
    0, // scircumflex,  dead_circumflex,  s

    // Latin-4 (ISO 8859-4)
    0x0156, 0x0327, 0x0052,
    0, // Rcedilla,     dead_cedilla,      R
    0x0128, 0x0303, 0x0049,
    0, // Itilde,        dead_tilde,       I
    0x013b, 0x0327, 0x004c,
    0, // Lcedilla,     dead_cedilla,      L
    0x0112, 0x0304, 0x0045,
    0, // Emacron,      dead_macron,      E
    0x0122, 0x0327, 0x0047,
    0, // Gcedilla,     dead_cedilla,      G
    0x0157, 0x0327, 0x0072,
    0, // rcedilla,     dead_cedilla,      r
    0x0129, 0x0303, 0x0069,
    0, // itilde,        dead_tilde,       i
    0x013c, 0x0327, 0x006c,
    0, // lcedilla,     dead_cedilla,      l
    0x0113, 0x0304, 0x0065,
    0, // emacron,      dead_macron,      e
    0x0123, 0x0327, 0x0067,
    0, // gcedilla,     dead_cedilla,      g
    0x0100, 0x0304, 0x0041,
    0, // Amacron,      dead_macron,      A
    0x012e, 0x0328, 0x0049,
    0, // Iogonek,      dead_ogonek,      I
    0x0116, 0x0307, 0x0045,
    0, // Eabovedot,    dead_abovedot,    E
    0x012a, 0x0304, 0x0049,
    0, // Imacron,      dead_macron,      I
    0x0145, 0x0327, 0x004e,
    0, // Ncedilla,     dead_cedilla,      N
    0x014c, 0x0304, 0x004f,
    0, // Omacron,      dead_macron,      O
    0x0136, 0x0327, 0x004b,
    0, // Kcedilla,     dead_cedilla,      K
    0x0172, 0x0328, 0x0055,
    0, // Uogonek,      dead_ogonek,      U
    0x0168, 0x0303, 0x0055,
    0, // Utilde,        dead_tilde,       U
    0x016a, 0x0304, 0x0055,
    0, // Umacron,      dead_macron,      U
    0x0101, 0x0304, 0x0061,
    0, // amacron,      dead_macron,      a
    0x012f, 0x0328, 0x0069,
    0, // iogonek,      dead_ogonek,      i
    0x0117, 0x0307, 0x0065,
    0, // eabovedot,    dead_abovedot,    e
    0x012b, 0x0304, 0x0069,
    0, // imacron,      dead_macron,      i
    0x0146, 0x0327, 0x006e,
    0, // ncedilla,     dead_cedilla,      n
    0x014d, 0x0304, 0x006f,
    0, // omacron,      dead_macron,      o
    0x0137, 0x0327, 0x006b,
    0, // kcedilla,     dead_cedilla,      k
    0x0173, 0x0328, 0x0075,
    0, // uogonek,      dead_ogonek,      u
    0x0169, 0x0303, 0x0075,
    0, // utilde,        dead_tilde,       u
    0x016b, 0x0304, 0x0075,
    0, // umacron,      dead_macron,      u

    // Latin-8 (ISO 8859-14)
    0x1e02, 0x0307, 0x0042,
    0, // Babovedot,    dead_abovedot,    B
    0x1e03, 0x0307, 0x0062,
    0, // babovedot,    dead_abovedot,    b
    0x1e0a, 0x0307, 0x0044,
    0, // Dabovedot,    dead_abovedot,    D
    0x1e80, 0x0300, 0x0057,
    0, // Wgrave,        dead_grave,       W
    0x1e82, 0x0301, 0x0057,
    0, // Wacute,        dead_acute,       W
    0x1e0b, 0x0307, 0x0064,
    0, // dabovedot,    dead_abovedot,    d
    0x1ef2, 0x0300, 0x0059,
    0, // Ygrave,        dead_grave,       Y
    0x1e1e, 0x0307, 0x0046,
    0, // Fabovedot,    dead_abovedot,    F
    0x1e1f, 0x0307, 0x0066,
    0, // fabovedot,    dead_abovedot,    f
    0x1e40, 0x0307, 0x004d,
    0, // Mabovedot,    dead_abovedot,    M
    0x1e41, 0x0307, 0x006d,
    0, // mabovedot,    dead_abovedot,    m
    0x1e56, 0x0307, 0x0050,
    0, // Pabovedot,    dead_abovedot,    P
    0x1e81, 0x0300, 0x0077,
    0, // wgrave,        dead_grave,       w
    0x1e57, 0x0307, 0x0070,
    0, // pabovedot,    dead_abovedot,    p
    0x1e83, 0x0301, 0x0077,
    0, // wacute,        dead_acute,       w
    0x1e60, 0x0307, 0x0053,
    0, // Sabovedot,    dead_abovedot,    S
    0x1ef3, 0x0300, 0x0079,
    0, // ygrave,        dead_grave,       y
    0x1e84, 0x0308, 0x0057,
    0, // Wdiaeresis,    dead_diaeresis,   W
    0x1e85, 0x0308, 0x0077,
    0, // wdiaeresis,    dead_diaeresis,   w
    0x1e61, 0x0307, 0x0073,
    0, // sabovedot,    dead_abovedot,    s
    0x0174, 0x0302, 0x0057,
    0, // Wcircumflex,  dead_circumflex,  W
    0x1e6a, 0x0307, 0x0054,
    0, // Tabovedot,    dead_abovedot,    T
    0x0176, 0x0302, 0x0059,
    0, // Ycircumflex,  dead_circumflex,  Y
    0x0175, 0x0302, 0x0077,
    0, // wcircumflex,  dead_circumflex,  w
    0x1e6b, 0x0307, 0x0074,
    0, // tabovedot,    dead_abovedot,    t
    0x0177, 0x0302, 0x0079,
    0, // ycircumflex,  dead_circumflex,  y

    // Latin-9 (ISO 8859-15)
    0x0178, 0x0308, 0x0059,
    0, // Ydiaeresis,   dead_diaeresis,   Y

    // Compose key sequences
    0x00c6, kKeyCompose, 0x0041, 0x0045,
    0, // AE,             A,           E
    0x00c1, kKeyCompose, 0x0041, 0x0027,
    0, // Aacute,         A, apostrophe
    0x00c2, kKeyCompose, 0x0041, 0x0053,
    0, // Acircumflex,    A, asciicircum
    0x00c3, kKeyCompose, 0x0041, 0x0022,
    0, // Adiaeresis,     A, quotedbl
    0x00c0, kKeyCompose, 0x0041, 0x0060,
    0, // Agrave,         A, grave
    0x00c5, kKeyCompose, 0x0041, 0x002a,
    0, // Aring,           A, asterisk
    0x00c3, kKeyCompose, 0x0041, 0x007e,
    0, // Atilde,         A, asciitilde
    0x00c7, kKeyCompose, 0x0043, 0x002c,
    0, // Ccedilla,       C, comma
    0x00d0, kKeyCompose, 0x0044, 0x002d,
    0, // ETH,            D, minus
    0x00c9, kKeyCompose, 0x0045, 0x0027,
    0, // Eacute,         E, apostrophe
    0x00ca, kKeyCompose, 0x0045, 0x0053,
    0, // Ecircumflex,    E, asciicircum
    0x00cb, kKeyCompose, 0x0045, 0x0022,
    0, // Ediaeresis,     E, quotedbl
    0x00c8, kKeyCompose, 0x0045, 0x0060,
    0, // Egrave,         E, grave
    0x00cd, kKeyCompose, 0x0049, 0x0027,
    0, // Iacute,         I, apostrophe
    0x00ce, kKeyCompose, 0x0049, 0x0053,
    0, // Icircumflex,    I, asciicircum
    0x00cf, kKeyCompose, 0x0049, 0x0022,
    0, // Idiaeresis,     I, quotedbl
    0x00cc, kKeyCompose, 0x0049, 0x0060,
    0, // Igrave,         I, grave
    0x00d1, kKeyCompose, 0x004e, 0x007e,
    0, // Ntilde,         N, asciitilde
    0x00d3, kKeyCompose, 0x004f, 0x0027,
    0, // Oacute,         O, apostrophe
    0x00d4, kKeyCompose, 0x004f, 0x0053,
    0, // Ocircumflex,    O, asciicircum
    0x00d6, kKeyCompose, 0x004f, 0x0022,
    0, // Odiaeresis,     O, quotedbl
    0x00d2, kKeyCompose, 0x004f, 0x0060,
    0, // Ograve,         O, grave
    0x00d8, kKeyCompose, 0x004f, 0x002f,
    0, // Ooblique,       O, slash
    0x00d5, kKeyCompose, 0x004f, 0x007e,
    0, // Otilde,         O, asciitilde
    0x00de, kKeyCompose, 0x0054, 0x0048,
    0, // THORN,           T,           H
    0x00da, kKeyCompose, 0x0055, 0x0027,
    0, // Uacute,         U, apostrophe
    0x00db, kKeyCompose, 0x0055, 0x0053,
    0, // Ucircumflex,    U, asciicircum
    0x00dc, kKeyCompose, 0x0055, 0x0022,
    0, // Udiaeresis,     U, quotedbl
    0x00d9, kKeyCompose, 0x0055, 0x0060,
    0, // Ugrave,         U, grave
    0x00dd, kKeyCompose, 0x0059, 0x0027,
    0, // Yacute,         Y, apostrophe
    0x00e1, kKeyCompose, 0x0061, 0x0027,
    0, // aacute,         a, apostrophe
    0x00e2, kKeyCompose, 0x0061, 0x0053,
    0, // acircumflex,    a, asciicircum
    0x00b4, kKeyCompose, 0x0027, 0x0027,
    0, // acute,           apostrophe, apostrophe
    0x00e4, kKeyCompose, 0x0061, 0x0022,
    0, // adiaeresis,     a, quotedbl
    0x00e6, kKeyCompose, 0x0061, 0x0065,
    0, // ae,             a,           e
    0x00e0, kKeyCompose, 0x0061, 0x0060,
    0, // agrave,         a, grave
    0x00e5, kKeyCompose, 0x0061, 0x002a,
    0, // aring,           a, asterisk
    0x0040, kKeyCompose, 0x0041, 0x0054,
    0, // at,             A,           T
    0x00e3, kKeyCompose, 0x0061, 0x007e,
    0, // atilde,         a, asciitilde
    0x005c, kKeyCompose, 0x002f, 0x002f,
    0, // backslash,       slash, slash
    0x007c, kKeyCompose, 0x004c, 0x0056,
    0, // bar,            L,           V
    0x007b, kKeyCompose, 0x0028, 0x002d,
    0, // braceleft,       parenleft, minus
    0x007d, kKeyCompose, 0x0029, 0x002d,
    0, // braceright,     parenright, minus
    0x005b, kKeyCompose, 0x0028, 0x0028,
    0, // bracketleft,    parenleft,  parenleft
    0x005d, kKeyCompose, 0x0029, 0x0029,
    0, // bracketright,   parenright, parenright
    0x00a6, kKeyCompose, 0x0042, 0x0056,
    0, // brokenbar,       B,           V
    0x00e7, kKeyCompose, 0x0063, 0x002c,
    0, // ccedilla,       c, comma
    0x00b8, kKeyCompose, 0x002c, 0x002c,
    0, // cedilla,        comma, comma
    0x00a2, kKeyCompose, 0x0063, 0x002f,
    0, // cent,           c, slash
    0x00a9, kKeyCompose, 0x0028, 0x0063,
    0, // copyright,       parenleft,  c
    0x00a4, kKeyCompose, 0x006f, 0x0078,
    0, // currency,       o,           x
    0x00b0, kKeyCompose, 0x0030, 0x0053,
    0, // degree,         0, asciicircum
    0x00a8, kKeyCompose, 0x0022, 0x0022,
    0, // diaeresis,       quotedbl,   quotedbl
    0x00f7, kKeyCompose, 0x003a, 0x002d,
    0, // division,       colon, minus
    0x00e9, kKeyCompose, 0x0065, 0x0027,
    0, // eacute,         e, apostrophe
    0x00ea, kKeyCompose, 0x0065, 0x0053,
    0, // ecircumflex,    e, asciicircum
    0x00eb, kKeyCompose, 0x0065, 0x0022,
    0, // ediaeresis,     e, quotedbl
    0x00e8, kKeyCompose, 0x0065, 0x0060,
    0, // egrave,         e, grave
    0x00f0, kKeyCompose, 0x0064, 0x002d,
    0, // eth,            d, minus
    0x00a1, kKeyCompose, 0x0021, 0x0021,
    0, // exclamdown,     exclam, exclam
    0x00ab, kKeyCompose, 0x003c, 0x003c,
    0, // guillemotleft,  less,       less
    0x00bb, kKeyCompose, 0x003e, 0x003e,
    0, // guillemotright, greater, greater
    0x0023, kKeyCompose, 0x002b, 0x002b,
    0, // numbersign,     plus,       plus
    0x00ad, kKeyCompose, 0x002d, 0x002d,
    0, // hyphen,         minus, minus
    0x00ed, kKeyCompose, 0x0069, 0x0027,
    0, // iacute,         i, apostrophe
    0x00ee, kKeyCompose, 0x0069, 0x0053,
    0, // icircumflex,    i, asciicircum
    0x00ef, kKeyCompose, 0x0069, 0x0022,
    0, // idiaeresis,     i, quotedbl
    0x00ec, kKeyCompose, 0x0069, 0x0060,
    0, // igrave,         i, grave
    0x00af, kKeyCompose, 0x002d, 0x0053,
    0, // macron,         minus,       asciicircum
    0x00ba, kKeyCompose, 0x006f, 0x005f,
    0, // masculine,       o, underscore
    0x00b5, kKeyCompose, 0x0075, 0x002f,
    0, // mu,             u, slash
    0x00d7, kKeyCompose, 0x0078, 0x0078,
    0, // multiply,       x,           x
    0x00a0, kKeyCompose, 0x0020, 0x0020,
    0, // nobreakspace,   space, space
    0x00ac, kKeyCompose, 0x002c, 0x002d,
    0, // notsign,        comma, minus
    0x00f1, kKeyCompose, 0x006e, 0x007e,
    0, // ntilde,         n, asciitilde
    0x00f3, kKeyCompose, 0x006f, 0x0027,
    0, // oacute,         o, apostrophe
    0x00f4, kKeyCompose, 0x006f, 0x0053,
    0, // ocircumflex,    o, asciicircum
    0x00f6, kKeyCompose, 0x006f, 0x0022,
    0, // odiaeresis,     o, quotedbl
    0x00f2, kKeyCompose, 0x006f, 0x0060,
    0, // ograve,         o, grave
    0x00bd, kKeyCompose, 0x0031, 0x0032,
    0, // onehalf,        1,           2
    0x00bc, kKeyCompose, 0x0031, 0x0034,
    0, // onequarter,     1,           4
    0x00b9, kKeyCompose, 0x0031, 0x0053,
    0, // onesuperior,    1, asciicircum
    0x00aa, kKeyCompose, 0x0061, 0x005f,
    0, // ordfeminine,    a, underscore
    0x00f8, kKeyCompose, 0x006f, 0x002f,
    0, // oslash,         o, slash
    0x00f5, kKeyCompose, 0x006f, 0x007e,
    0, // otilde,         o, asciitilde
    0x00b6, kKeyCompose, 0x0070, 0x0021,
    0, // paragraph,       p, exclam
    0x00b7, kKeyCompose, 0x002e, 0x002e,
    0, // periodcentered, period, period
    0x00b1, kKeyCompose, 0x002b, 0x002d,
    0, // plusminus,       plus, minus
    0x00bf, kKeyCompose, 0x003f, 0x003f,
    0, // questiondown,   question,   question
    0x00ae, kKeyCompose, 0x0028, 0x0072,
    0, // registered,     parenleft,  r
    0x00a7, kKeyCompose, 0x0073, 0x006f,
    0, // section,        s,           o
    0x00df, kKeyCompose, 0x0073, 0x0073,
    0, // ssharp,         s,           s
    0x00a3, kKeyCompose, 0x004c, 0x002d,
    0, // sterling,       L, minus
    0x00fe, kKeyCompose, 0x0074, 0x0068,
    0, // thorn,           t,           h
    0x00be, kKeyCompose, 0x0033, 0x0034,
    0, // threequarters,  3,           4
    0x00b3, kKeyCompose, 0x0033, 0x0053,
    0, // threesuperior,  3, asciicircum
    0x00b2, kKeyCompose, 0x0032, 0x0053,
    0, // twosuperior,    2, asciicircum
    0x00fa, kKeyCompose, 0x0075, 0x0027,
    0, // uacute,         u, apostrophe
    0x00fb, kKeyCompose, 0x0075, 0x0053,
    0, // ucircumflex,    u, asciicircum
    0x00fc, kKeyCompose, 0x0075, 0x0022,
    0, // udiaeresis,     u, quotedbl
    0x00f9, kKeyCompose, 0x0075, 0x0060,
    0, // ugrave,         u, grave
    0x00fd, kKeyCompose, 0x0079, 0x0027,
    0, // yacute,         y, apostrophe
    0x00ff, kKeyCompose, 0x0079, 0x0022,
    0, // ydiaeresis,     y, quotedbl
    0x00a5, kKeyCompose, 0x0079, 0x003d,
    0, // yen,            y, equal

    // end of table
    0
};

static const KeyID s_numpadTable[] = {
    kKeyKP_Space,  0x0020,     kKeyKP_Tab,       kKeyTab,      kKeyKP_Enter,    kKeyReturn, kKeyKP_F1,       kKeyF1,
    kKeyKP_F2,     kKeyF2,     kKeyKP_F3,        kKeyF3,       kKeyKP_F4,       kKeyF4,     kKeyKP_Home,     kKeyHome,
    kKeyKP_Left,   kKeyLeft,   kKeyKP_Up,        kKeyUp,       kKeyKP_Right,    kKeyRight,  kKeyKP_Down,     kKeyDown,
    kKeyKP_PageUp, kKeyPageUp, kKeyKP_PageDown,  kKeyPageDown, kKeyKP_End,      kKeyEnd,    kKeyKP_Begin,    kKeyBegin,
    kKeyKP_Insert, kKeyInsert, kKeyKP_Delete,    kKeyDelete,   kKeyKP_Equal,    0x003d,     kKeyKP_Multiply, 0x002a,
    kKeyKP_Add,    0x002b,     kKeyKP_Separator, 0x002c,       kKeyKP_Subtract, 0x002d,     kKeyKP_Decimal,  0x002e,
    kKeyKP_Divide, 0x002f,     kKeyKP_0,         0x0030,       kKeyKP_1,        0x0031,     kKeyKP_2,        0x0032,
    kKeyKP_3,      0x0033,     kKeyKP_4,         0x0034,       kKeyKP_5,        0x0035,     kKeyKP_6,        0x0036,
    kKeyKP_7,      0x0037,     kKeyKP_8,         0x0038,       kKeyKP_9,        0x0039
};

//
// KeyState
//

KeyState::KeyState(IEventQueue *events, std::vector<std::string> layouts, bool isLangSyncEnabled)
    : IKeyState(events),
      m_keyMapPtr(new deskflow::KeyMap()),
      m_keyMap(*m_keyMapPtr),
      m_mask(0),
      m_events(events),
      m_isLangSyncEnabled(isLangSyncEnabled)
{
  m_keyMap.setLanguageData(std::move(layouts));
  init();
}

KeyState::KeyState(
    IEventQueue *events, deskflow::KeyMap &keyMap, std::vector<std::string> layouts, bool isLangSyncEnabled
)
    : IKeyState(events),
      m_keyMapPtr(nullptr),
      m_keyMap(keyMap),
      m_mask(0),
      m_events(events),
      m_isLangSyncEnabled(isLangSyncEnabled)
{
  m_keyMap.setLanguageData(std::move(layouts));
  init();
}

KeyState::~KeyState()
{
  if (m_keyMapPtr)
    delete m_keyMapPtr;
}

void KeyState::init()
{
  memset(&m_keys, 0, sizeof(m_keys));
  memset(&m_syntheticKeys, 0, sizeof(m_syntheticKeys));
  memset(&m_keyClientData, 0, sizeof(m_keyClientData));
  memset(&m_serverKeys, 0, sizeof(m_serverKeys));
}

void KeyState::onKey(KeyButton button, bool down, KeyModifierMask newState)
{
  // update modifier state
  m_mask = newState;
  LOG_VERBOSE("new mask: 0x%04x", m_mask);

  // ignore bogus buttons
  button &= kButtonMask;
  if (button == 0) {
    return;
  }

  // update key state
  if (down) {
    m_keys[button] = 1;
    m_syntheticKeys[button] = 1;
  } else {
    m_keys[button] = 0;
    m_syntheticKeys[button] = 0;
  }
}

void KeyState::sendKeyEvent(
    void *target, bool press, bool isAutoRepeat, KeyID key, KeyModifierMask mask, int32_t count, KeyButton button
)
{
  using enum EventTypes;
  if (m_keyMap.isHalfDuplex(key, button)) {
    if (isAutoRepeat) {
      // ignore auto-repeat on half-duplex keys
    } else {
      m_events->addEvent(Event(KeyStateKeyDown, target, KeyInfo::alloc(key, mask, button, 1)));
      m_events->addEvent(Event(KeyStateKeyUp, target, KeyInfo::alloc(key, mask, button, 1)));
    }
  } else {
    if (isAutoRepeat) {
      m_events->addEvent(Event(KeyStateKeyRepeat, target, KeyInfo::alloc(key, mask, button, count)));
    } else if (press) {
      m_events->addEvent(Event(KeyStateKeyDown, target, KeyInfo::alloc(key, mask, button, 1)));
    } else {
      m_events->addEvent(Event(KeyStateKeyUp, target, KeyInfo::alloc(key, mask, button, 1)));
    }
  }
}

void KeyState::updateKeyMap(deskflow::KeyMap *existing)
{
  if (existing) {
    m_keyMap.swap(*existing);
  } else {
    // get the current keyboard map
    deskflow::KeyMap keyMap;
    getKeyMap(keyMap);
    m_keyMap.swap(keyMap);
    m_keyMap.finish();
  }

  // add special keys
  addCombinationEntries();
  addKeypadEntries();
  addAliasEntries();
}

void KeyState::updateKeyState()
{
  // reset our state
  memset(&m_keys, 0, sizeof(m_keys));
  memset(&m_syntheticKeys, 0, sizeof(m_syntheticKeys));
  memset(&m_keyClientData, 0, sizeof(m_keyClientData));
  memset(&m_serverKeys, 0, sizeof(m_serverKeys));
  m_activeModifiers.clear();
  m_authoritativeModifiers.clear();
  m_actionModifiers.clear();
  m_clientModifiers.clear();
  m_authoritativeMask = 0;
  m_actionModifierMask = 0;
  m_actionModifierRefs = 0;
  m_actionModifierRefCounts.fill(0);
  m_authoritativeStateOwned = false;

  // get the current keyboard state
  KeyButtonSet keysDown;
  pollPressedKeys(keysDown);
  for (const auto &key : keysDown) {
    m_keys[key] = 1;
  }

  // get the current modifier state
  clearStaleModifiers();
  m_mask = pollActiveModifiers();
  m_authoritativeMask = m_mask & deskflow::kLockModifierMask;

  // set active modifiers
  AddActiveModifierContext addModifierContext(pollActiveGroup(), m_mask, m_activeModifiers);
  m_keyMap.foreachKey(&KeyState::addActiveModifierCB, &addModifierContext);

  LOG_VERBOSE("modifiers on update: 0x%04x", m_mask);
}

void KeyState::addActiveModifierCB(KeyID, int32_t group, deskflow::KeyMap::KeyItem &keyItem, void *vcontext)
{
  auto *context = static_cast<AddActiveModifierContext *>(vcontext);
  if (group == context->m_activeGroup && (keyItem.m_generates & context->m_mask) != 0) {
    context->m_activeModifiers.insert(std::make_pair(keyItem.m_generates, keyItem));
  }
}

void KeyState::setHalfDuplexMask(KeyModifierMask mask)
{
  m_keyMap.clearHalfDuplexModifiers();
  if ((mask & KeyModifierCapsLock) != 0) {
    m_keyMap.addHalfDuplexModifier(kKeyCapsLock);
  }
  if ((mask & KeyModifierNumLock) != 0) {
    m_keyMap.addHalfDuplexModifier(kKeyNumLock);
  }
  if ((mask & KeyModifierScrollLock) != 0) {
    m_keyMap.addHalfDuplexModifier(kKeyScrollLock);
  }
}

void KeyState::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton serverID, const std::string &lang)
{
  const bool sessionBlocked =
      m_keyboardSessionActive &&
      (!m_keyboardStateRestored || !m_pendingKeyEvents.empty() || m_pendingKeyEventsOverflowed);
  if (sessionBlocked || !isKeyInjectionAvailable()) {
    if (m_keyboardSessionActive) {
      deferKeyEvent(PendingKeyEvent{PendingKeyEventType::Down, id, mask, 1, serverID, lang});
      if (m_keyboardStateRestored && isKeyInjectionAvailable()) {
        replayPendingKeyEvents();
      }
    } else {
      LOG_DEBUG("discarding key down while injection is unavailable outside a keyboard session");
    }
    return;
  }

  const auto result = fakeKeyDownNow(id, mask, serverID, lang);
  if (result == KeyEventResult::Retry) {
    deferKeyEvent(PendingKeyEvent{PendingKeyEventType::Down, id, mask, 1, serverID, lang});
  }
}

KeyState::KeyEventResult
KeyState::fakeKeyDownNow(KeyID id, KeyModifierMask mask, KeyButton serverID, const std::string &lang)
{
  if (!isKeyInjectionAvailable()) {
    return KeyEventResult::Retry;
  }

  if (id == kKeySetModifiers || id == kKeyClearModifiers) {
    const KeyModifierMask requested = mask & deskflow::kMomentaryModifierMask;
    const auto previousRefCounts = m_actionModifierRefCounts;
    const KeyModifierMask previousRefs = m_actionModifierRefs;
    if (id == kKeySetModifiers) {
      retainActionModifiers(requested);
    } else {
      releaseActionModifiers(requested);
    }
    const auto result =
        reconcileModifierLayer(m_actionModifiers, m_actionModifierMask, m_actionModifierRefs, false, "action");
    if (!result.injected) {
      m_actionModifierRefCounts = previousRefCounts;
      m_actionModifierRefs = previousRefs;
      return KeyEventResult::Retry;
    }
    return KeyEventResult::Injected;
  }

  mask = keyEventModifierMask(mask);

  // if this server key is already down then this is probably a
  // mis-reported autorepeat.
  serverID &= kButtonMask;
  if (m_serverKeys[serverID] != 0) {
    return fakeKeyRepeatNow(id, mask, 1, serverID, lang);
  }

  // ignore certain keys
  if (isIgnoredKey(id, mask)) {
    LOG_VERBOSE("ignored key %04x %04x", id, mask);
    return KeyEventResult::Consumed;
  }

  Keystrokes keys;
  const ModifierToKeys oldActiveModifiers = m_activeModifiers;
  ModifierToKeys nextActiveModifiers = m_activeModifiers;
  KeyModifierMask nextMask = m_mask;
  const deskflow::KeyMap::KeyItem *keyItem =
      m_keyMap.mapKey(keys, id, pollActiveGroup(), nextActiveModifiers, nextMask, mask, false, lang);

  if (keyItem == nullptr) {
    // a media key won't be mapped on mac, so we need to fake it in a
    // special way
    if (id == kKeyAudioDown || id == kKeyAudioUp || id == kKeyAudioMute || id == kKeyAudioPlay || id == kKeyAudioPrev ||
        id == kKeyAudioNext || id == kKeyBrightnessDown || id == kKeyBrightnessUp) {
      LOG_VERBOSE("emulating media key");
      fakeMediaKey(id);
    }

    return KeyEventResult::Consumed;
  }

  const auto localID = static_cast<KeyButton>(keyItem->m_button & kButtonMask);
  if (localID != 0 && keyItem->m_generates != 0 && !keyItem->m_lock && m_syntheticKeys[localID] > 0) {
    std::erase_if(keys, [localID](const Keystroke &key) {
      return key.m_type == Keystroke::KeyType::Button && key.m_data.m_button.m_button == localID &&
             key.m_data.m_button.m_press && !key.m_data.m_button.m_repeat;
    });
  }

  if (!fakeKeys(keys, 1)) {
    LOG_WARN("key down was not injected; retaining the previous keyboard ledger");
    return KeyEventResult::Retry;
  }

  applyModifierReferenceDelta(oldActiveModifiers, nextActiveModifiers, localID);
  if (localID != 0) {
    ++m_keys[localID];
    ++m_syntheticKeys[localID];
    m_keyClientData[localID] = keyItem->m_client;
    m_serverKeys[serverID] = localID;
  }

  m_activeModifiers = std::move(nextActiveModifiers);
  refreshClientModifierLayer();
  m_mask = nextMask;
  return KeyEventResult::Injected;
}

bool KeyState::fakeKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton serverID, const std::string &lang)
{
  const bool sessionBlocked =
      m_keyboardSessionActive &&
      (!m_keyboardStateRestored || !m_pendingKeyEvents.empty() || m_pendingKeyEventsOverflowed);
  if (sessionBlocked || !isKeyInjectionAvailable()) {
    if (m_keyboardSessionActive) {
      deferKeyEvent(PendingKeyEvent{PendingKeyEventType::Repeat, id, mask, count, serverID, lang});
      if (m_keyboardStateRestored && isKeyInjectionAvailable()) {
        replayPendingKeyEvents();
      }
    } else {
      LOG_DEBUG("discarding key repeat while injection is unavailable outside a keyboard session");
    }
    return false;
  }

  const auto result = fakeKeyRepeatNow(id, mask, count, serverID, lang);
  if (result == KeyEventResult::Retry) {
    deferKeyEvent(PendingKeyEvent{PendingKeyEventType::Repeat, id, mask, count, serverID, lang});
  }
  return result == KeyEventResult::Injected;
}

KeyState::KeyEventResult KeyState::fakeKeyRepeatNow(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton serverID, const std::string &lang
)
{
  LOG_VERBOSE("fakeKeyRepeat");
  if (!isKeyInjectionAvailable()) {
    return KeyEventResult::Retry;
  }
  mask = keyEventModifierMask(mask);
  serverID &= kButtonMask;

  // if we haven't seen this button go down then ignore it
  const KeyButton oldLocalID = m_serverKeys[serverID];
  if (oldLocalID == 0) {
    return KeyEventResult::Consumed;
  }

  // get keys for key repeat
  Keystrokes keys;
  const ModifierToKeys oldActiveModifiers = m_activeModifiers;
  ModifierToKeys nextActiveModifiers = m_activeModifiers;
  KeyModifierMask nextMask = m_mask;
  const deskflow::KeyMap::KeyItem *keyItem =
      m_keyMap.mapKey(keys, id, pollActiveGroup(), nextActiveModifiers, nextMask, mask, true, lang);
  if (keyItem == nullptr) {
    return KeyEventResult::Consumed;
  }
  const auto localID = static_cast<KeyButton>(keyItem->m_button & kButtonMask);
  if (localID == 0) {
    return KeyEventResult::Consumed;
  }

  // if the KeyButton for the auto-repeat is not the same as for the
  // initial press then mark the initial key as released and the new
  // key as pressed.  this can happen when we auto-repeat after a
  // dead key.  for example, a dead accent followed by 'a' will
  // generate an 'a with accent' followed by a repeating 'a'.  the
  // KeyButtons for the two KeyIDs might be different.
  if (localID != oldLocalID) {
    // replace key up with previous KeyButton but leave key down
    // alone so it uses the new KeyButton.
    for (auto &key : keys) {
      if (key.m_type == Keystroke::KeyType::Button && key.m_data.m_button.m_button == localID) {
        key.m_data.m_button.m_button = oldLocalID;
        break;
      }
    }

    if (m_syntheticKeys[oldLocalID] > 1) {
      std::erase_if(keys, [oldLocalID](const Keystroke &key) {
        return key.m_type == Keystroke::KeyType::Button && key.m_data.m_button.m_button == oldLocalID &&
               !key.m_data.m_button.m_press;
      });
    }
    if (m_syntheticKeys[localID] > 0) {
      std::erase_if(keys, [localID](const Keystroke &key) {
        return key.m_type == Keystroke::KeyType::Button && key.m_data.m_button.m_button == localID &&
               key.m_data.m_button.m_press && !key.m_data.m_button.m_repeat;
      });
    }
  }

  if (!fakeKeys(keys, count)) {
    LOG_WARN("key repeat was not injected; retaining the previous keyboard ledger");
    return KeyEventResult::Retry;
  }

  if (localID != oldLocalID) {
    applyModifierReferenceDelta(oldActiveModifiers, nextActiveModifiers, oldLocalID, localID);
    if (m_keys[oldLocalID] > 0) {
      --m_keys[oldLocalID];
    }
    if (m_syntheticKeys[oldLocalID] > 0) {
      --m_syntheticKeys[oldLocalID];
    }
    ++m_keys[localID];
    ++m_syntheticKeys[localID];
    m_keyClientData[localID] = keyItem->m_client;
    m_serverKeys[serverID] = localID;
  }

  m_activeModifiers = std::move(nextActiveModifiers);
  refreshClientModifierLayer();
  m_mask = nextMask;
  return KeyEventResult::Injected;
}

bool KeyState::fakeKeyUp(KeyButton serverID)
{
  const bool sessionBlocked =
      m_keyboardSessionActive &&
      (!m_keyboardStateRestored || !m_pendingKeyEvents.empty() || m_pendingKeyEventsOverflowed);
  if (sessionBlocked || !isKeyInjectionAvailable()) {
    if (m_keyboardSessionActive) {
      deferKeyEvent(PendingKeyEvent{PendingKeyEventType::Up, kKeyNone, 0, 1, serverID, {}});
      if (m_keyboardStateRestored && isKeyInjectionAvailable()) {
        replayPendingKeyEvents();
      }
    } else {
      LOG_DEBUG("discarding key up while injection is unavailable outside a keyboard session");
    }
    return false;
  }

  const auto result = fakeKeyUpNow(serverID);
  if (result == KeyEventResult::Retry) {
    deferKeyEvent(PendingKeyEvent{PendingKeyEventType::Up, kKeyNone, 0, 1, serverID, {}});
  }
  return result == KeyEventResult::Injected;
}

KeyState::KeyEventResult KeyState::fakeKeyUpNow(KeyButton serverID)
{
  if (!isKeyInjectionAvailable()) {
    return KeyEventResult::Retry;
  }

  serverID &= kButtonMask;

  // if we haven't seen this button go down then ignore it
  const KeyButton localID = m_serverKeys[serverID];
  if (localID == 0) {
    return KeyEventResult::Consumed;
  }

  ModifierToKeys nextActiveModifiers = m_activeModifiers;
  eraseClientModifier(nextActiveModifiers, localID);

  // Multiple logical owners of the same canonical modifier share one
  // physical key press. Only the final owner emits the release.
  Keystrokes keys;
  if (m_syntheticKeys[localID] <= 1) {
    keys.push_back(Keystroke(localID, false, false, m_keyClientData[localID]));
  }

  if (!fakeKeys(keys, 1)) {
    LOG_WARN("key up was not injected; retaining the previous keyboard ledger");
    return KeyEventResult::Retry;
  }

  if (m_keys[localID] > 0) {
    --m_keys[localID];
  }
  if (m_syntheticKeys[localID] > 0) {
    --m_syntheticKeys[localID];
  }
  m_serverKeys[serverID] = 0;
  m_activeModifiers = std::move(nextActiveModifiers);
  refreshClientModifierLayer();
  recomputeActiveModifierMask();
  return KeyEventResult::Injected;
}

bool KeyState::deferKeyEvent(PendingKeyEvent event)
{
  if (!m_keyboardSessionActive || m_replayingPendingKeyEvents) {
    LOG_DEBUG("discarding key event while no restorable keyboard session is active");
    return false;
  }

  if (m_pendingKeyEventsOverflowed) {
    return false;
  }

  if (event.type == PendingKeyEventType::Repeat && !m_pendingKeyEvents.empty()) {
    auto &previous = m_pendingKeyEvents.back();
    if (previous.type == PendingKeyEventType::Repeat && previous.id == event.id && previous.mask == event.mask &&
        previous.button == event.button && previous.lang == event.lang) {
      const auto combined = static_cast<std::int64_t>(previous.count) + event.count;
      previous.count = static_cast<int32_t>(std::min<std::int64_t>(combined, std::numeric_limits<int32_t>::max()));
      return true;
    }
  }

  if (m_pendingKeyEvents.size() >= s_maxPendingKeyEvents) {
    LOG_WARN(
        "keyboard restoration queue exceeded %zu events; discarding the incomplete queued transaction",
        s_maxPendingKeyEvents
    );
    m_pendingKeyEvents.clear();
    m_pendingKeyEventsOverflowed = true;
    return false;
  }

  LOG_VERBOSE(
      "queued keyboard event type=%d button=0x%04x count=%d while restoration is pending",
      static_cast<int>(event.type), event.button, event.count
  );
  m_pendingKeyEvents.push_back(std::move(event));
  return true;
}

bool KeyState::replayPendingKeyEvents()
{
  if (!m_keyboardSessionActive || !isKeyInjectionAvailable()) {
    return false;
  }

  if (m_pendingKeyEventsOverflowed) {
    LOG_WARN("not replaying keyboard restoration queue because its transaction was incomplete");
    m_pendingKeyEventsOverflowed = false;
    return true;
  }

  if (m_pendingKeyEvents.empty()) {
    return true;
  }

  std::size_t replayed = 0;
  m_replayingPendingKeyEvents = true;
  while (!m_pendingKeyEvents.empty()) {
    const auto &event = m_pendingKeyEvents.front();
    KeyEventResult result = KeyEventResult::Consumed;
    switch (event.type) {
    case PendingKeyEventType::Down:
      result = fakeKeyDownNow(event.id, event.mask, event.button, event.lang);
      break;
    case PendingKeyEventType::Repeat:
      result = fakeKeyRepeatNow(event.id, event.mask, event.count, event.button, event.lang);
      break;
    case PendingKeyEventType::Up:
      result = fakeKeyUpNow(event.button);
      break;
    }

    if (result == KeyEventResult::Retry) {
      m_replayingPendingKeyEvents = false;
      LOG_DEBUG("paused keyboard restoration replay after %zu events", replayed);
      return false;
    }

    m_pendingKeyEvents.pop_front();
    ++replayed;
  }
  m_replayingPendingKeyEvents = false;

  LOG_DEBUG("replayed %zu keyboard events after authoritative restoration", replayed);
  return true;
}

void KeyState::clearPendingKeyEvents()
{
  if (!m_pendingKeyEvents.empty() || m_pendingKeyEventsOverflowed) {
    LOG_DEBUG("discarding %zu pending keyboard events at session boundary", m_pendingKeyEvents.size());
  }
  m_pendingKeyEvents.clear();
  m_pendingKeyEventsOverflowed = false;
  m_replayingPendingKeyEvents = false;
}

void KeyState::clearSyntheticState()
{
  memset(&m_keys, 0, sizeof(m_keys));
  memset(&m_syntheticKeys, 0, sizeof(m_syntheticKeys));
  memset(&m_keyClientData, 0, sizeof(m_keyClientData));
  memset(&m_serverKeys, 0, sizeof(m_serverKeys));
  m_activeModifiers.clear();
  m_authoritativeModifiers.clear();
  m_actionModifiers.clear();
  m_clientModifiers.clear();
  m_mask = pollActiveModifiers() & deskflow::kLockModifierMask;
  m_authoritativeMask = m_mask;
  m_actionModifierMask = 0;
  m_actionModifierRefs = 0;
  m_actionModifierRefCounts.fill(0);
  m_keyboardStateRestored = false;
  m_authoritativeStateOwned = false;
}

KeyModifierMask KeyState::keyEventModifierMask(KeyModifierMask eventMask) const
{
  // Active-session events are mapped against the state that was actually
  // reconciled from DKST. This is especially important for events queued while
  // the snapshot was invalid: their wire mask predates the restored state.
  const KeyModifierMask baseMask =
      (m_keyboardSessionActive && m_keyboardSessionAuthoritative && m_keyboardStateRestored)
          ? m_authoritativeMask
          : eventMask;

  // Lock keys are reconciled only by the absolute-state transaction. Ordinary
  // key mapping must use the lock state that has actually been injected, or it
  // could toggle a lock while a device-resume baseline is still pending.
  return (baseMask & ~deskflow::kLockModifierMask) | (m_authoritativeMask & deskflow::kLockModifierMask) |
         m_actionModifierMask | modifierMask(m_clientModifiers, false);
}

KeyState::ModifierReconcileResult KeyState::reconcileModifierLayer(
    ModifierToKeys &layer, KeyModifierMask &layerMask, KeyModifierMask desiredMask, bool includeLocks,
    const char *layerName
)
{
  if (!isKeyInjectionAvailable()) {
    return {};
  }

  desiredMask &= deskflow::kMomentaryModifierMask | (includeLocks ? deskflow::kLockModifierMask : 0);
  static constexpr std::array<KeyModifierMask, 9> s_managedModifiers = {
      KeyModifierScrollLock, KeyModifierNumLock, KeyModifierCapsLock, KeyModifierAltGr, KeyModifierSuper,
      KeyModifierMeta,       KeyModifierAlt,     KeyModifierControl,  KeyModifierShift
  };

  ModifierToKeys nextLayer = layer;
  KeyModifierMask nextLayerMask = layerMask;
  Keystrokes lockKeys;
  Keystrokes momentaryKeys;
  bool complete = true;
  for (const auto modifier : s_managedModifiers) {
    const bool isLock = (modifier & deskflow::kLockModifierMask) != 0;
    if (isLock && !includeLocks) {
      continue;
    }

    Keystrokes pending;
    if (!m_keyMap.mapModifierState(
            pending, pollActiveGroup(), nextLayer, nextLayerMask, desiredMask, modifier
        )) {
      LOG_WARN("unable to reconcile %s keyboard modifier 0x%04x", layerName, modifier);
      complete = false;
      continue;
    }

    auto &destination = isLock ? lockKeys : momentaryKeys;
    destination.insert(destination.end(), pending.begin(), pending.end());
  }

  ModifierToKeys nextActiveModifiers = m_activeModifiers;
  replaceModifierLayer(nextActiveModifiers, layer, nextLayer);

  Keystrokes keys = std::move(lockKeys);
  for (const auto &key : momentaryKeys) {
    if (key.m_type != Keystroke::KeyType::Button) {
      keys.push_back(key);
      continue;
    }

    const KeyButton button = key.m_data.m_button.m_button;
    const auto oldRefs = buttonReferenceCount(m_activeModifiers, button);
    const auto nextRefs = buttonReferenceCount(nextActiveModifiers, button);
    if ((key.m_data.m_button.m_press && oldRefs == 0 && nextRefs > 0) ||
        (!key.m_data.m_button.m_press && oldRefs > 0 && nextRefs == 0)) {
      keys.push_back(key);
    }
  }

  if (!fakeKeys(keys, 1)) {
    LOG_WARN("%s keyboard state reconciliation was not injected; retaining the previous ledger", layerName);
    return {};
  }

  applyModifierReferenceDelta(m_activeModifiers, nextActiveModifiers);
  layer = std::move(nextLayer);
  layerMask = nextLayerMask;
  m_activeModifiers = std::move(nextActiveModifiers);
  recomputeActiveModifierMask();

  LOG_VERBOSE(
      "reconciled %s keyboard modifiers desired=0x%04x actual=0x%04x generated=%zu complete=%d", layerName,
      desiredMask, getActiveModifiers(), keys.size(), complete
  );
  return ModifierReconcileResult{true, complete};
}

void KeyState::replaceModifierLayer(
    ModifierToKeys &combined, const ModifierToKeys &oldLayer, const ModifierToKeys &newLayer
) const
{
  combined = subtractModifierLayer(combined, oldLayer);
  combined.insert(newLayer.begin(), newLayer.end());
}

void KeyState::applyModifierReferenceDelta(
    const ModifierToKeys &oldModifiers, const ModifierToKeys &newModifiers, KeyButton excludedButton1,
    KeyButton excludedButton2
)
{
  for (KeyButton button = 1; button < IKeyState::s_numButtons; ++button) {
    if (button == excludedButton1 || button == excludedButton2) {
      continue;
    }

    const auto oldRefs = buttonReferenceCount(oldModifiers, button);
    const auto newRefs = buttonReferenceCount(newModifiers, button);
    if (newRefs > oldRefs) {
      const auto added = static_cast<int32_t>(newRefs - oldRefs);
      m_keys[button] += added;
      m_syntheticKeys[button] += added;
      const auto key = std::ranges::find_if(newModifiers, [button](const auto &entry) {
        return !entry.second.m_lock && entry.second.m_button == button;
      });
      if (key != newModifiers.end()) {
        m_keyClientData[button] = key->second.m_client;
      }
    } else if (oldRefs > newRefs) {
      const auto removed = static_cast<int32_t>(oldRefs - newRefs);
      m_keys[button] = std::max<int32_t>(0, m_keys[button] - removed);
      m_syntheticKeys[button] = std::max<int32_t>(0, m_syntheticKeys[button] - removed);
    }
  }
}

bool KeyState::eraseClientModifier(ModifierToKeys &modifiers, KeyButton button) const
{
  ModifierToKeys clientModifiers = m_clientModifiers;
  const auto clientEntry = std::ranges::find_if(clientModifiers, [button](const auto &entry) {
    return !entry.second.m_lock && entry.second.m_button == button;
  });
  if (clientEntry == clientModifiers.end()) {
    return false;
  }

  const auto combinedEntry = std::ranges::find_if(modifiers, [&clientEntry](const auto &entry) {
    return sameModifierEntry(entry, *clientEntry);
  });
  if (combinedEntry == modifiers.end()) {
    return false;
  }

  modifiers.erase(combinedEntry);
  return true;
}

void KeyState::refreshClientModifierLayer()
{
  m_clientModifiers = subtractModifierLayer(m_activeModifiers, m_authoritativeModifiers);
  m_clientModifiers = subtractModifierLayer(m_clientModifiers, m_actionModifiers);
}

void KeyState::retainActionModifiers(KeyModifierMask mask)
{
  for (std::size_t bit = 0; bit < m_actionModifierRefCounts.size(); ++bit) {
    const KeyModifierMask modifier = KeyModifierMask{1} << bit;
    if ((mask & modifier) == 0) {
      continue;
    }

    if (m_actionModifierRefCounts[bit] == std::numeric_limits<std::uint32_t>::max()) {
      LOG_WARN("action modifier reference count saturated for modifier 0x%04x", modifier);
      continue;
    }

    ++m_actionModifierRefCounts[bit];
    m_actionModifierRefs |= modifier;
  }
}

void KeyState::releaseActionModifiers(KeyModifierMask mask)
{
  for (std::size_t bit = 0; bit < m_actionModifierRefCounts.size(); ++bit) {
    const KeyModifierMask modifier = KeyModifierMask{1} << bit;
    if ((mask & modifier) == 0 || m_actionModifierRefCounts[bit] == 0) {
      continue;
    }

    if (--m_actionModifierRefCounts[bit] == 0) {
      m_actionModifierRefs &= ~modifier;
    }
  }
}

void KeyState::recomputeActiveModifierMask()
{
  m_mask = (m_authoritativeMask & deskflow::kLockModifierMask) | modifierMask(m_activeModifiers, false);
}

void KeyState::fakeAllKeysUp()
{
  Keystrokes keys;
  for (KeyButton i = 0; i < IKeyState::s_numButtons; ++i) {
    if (m_syntheticKeys[i] > 0) {
      keys.push_back(Keystroke(i, false, false, m_keyClientData[i]));
    }
  }

  if (!isKeyInjectionAvailable()) {
    clearSyntheticState();
    return;
  }

  if (fakeKeys(keys, 1)) {
    clearSyntheticState();
  }
}

void KeyState::beginKeyboardSession(const deskflow::KeyboardModifierState &initialState)
{
  LOG_DEBUG(
      "begin keyboard session supported=%d initial-valid=%d initial-lock-mask=0x%04x", initialState.supported,
      initialState.valid, initialState.locked & deskflow::kLockModifierMask
  );
  clearPendingKeyEvents();
  if (m_keyboardSessionActive) {
    endKeyboardSession();
  } else {
    clearSyntheticState();
  }

  m_keyboardSessionLockBaseline = pollActiveModifiers() & deskflow::kLockModifierMask;
  m_keyboardSessionActive = true;
  m_keyboardSessionAuthoritative = initialState.supported;
  m_keyboardStateRestored = false;
  m_desiredKeyboardState = deskflow::normalizedKeyboardModifierState(initialState);
  if (m_desiredKeyboardState.valid) {
    (void)reconcileKeyboardState(m_desiredKeyboardState);
  } else {
    LOG_DEBUG("keyboard session is waiting for its first authoritative state snapshot");
  }
}

bool KeyState::reconcileKeyboardState(const deskflow::KeyboardModifierState &state)
{
  if (!m_keyboardSessionActive) {
    return false;
  }

  m_desiredKeyboardState = deskflow::normalizedKeyboardModifierState(state);
  m_keyboardSessionAuthoritative = m_desiredKeyboardState.supported;
  if (!m_desiredKeyboardState.valid) {
    m_keyboardStateRestored = false;
    if (!m_authoritativeStateOwned) {
      LOG_DEBUG("authoritative keyboard state is unobserved; no owned state requires cleanup");
      return false;
    }
    if (!isKeyInjectionAvailable()) {
      LOG_DEBUG("authoritative keyboard state is not observed and injection is unavailable");
      return false;
    }

    const auto reset = reconcileModifierLayer(
        m_authoritativeModifiers, m_authoritativeMask, 0, true, "authoritative reset"
    );
    if (!reset.injected || !reset.complete) {
      LOG_DEBUG("authoritative keyboard reset remains incomplete while waiting for a fresh snapshot");
    } else {
      m_authoritativeStateOwned = false;
      LOG_DEBUG("cleared authoritative keyboard state while waiting for a fresh snapshot");
    }
    return false;
  }

  if (!isKeyInjectionAvailable()) {
    m_keyboardStateRestored = false;
    LOG_DEBUG("deferring keyboard state reconciliation while injection is unavailable");
    return false;
  }

  const auto result = reconcileModifierLayer(
      m_authoritativeModifiers, m_authoritativeMask,
      deskflow::effectiveKeyboardModifiers(m_desiredKeyboardState), true, "authoritative"
  );
  if (result.injected) {
    m_authoritativeStateOwned = true;
  }
  if (!result.injected || !result.complete) {
    m_keyboardStateRestored = false;
    if (result.injected) {
      LOG_DEBUG("authoritative keyboard restoration is incomplete; keeping ordinary keys deferred");
    }
    return false;
  }

  m_authoritativeStateOwned = true;
  m_keyboardStateRestored = true;
  const bool replayed = replayPendingKeyEvents();
  return replayed;
}

void KeyState::endKeyboardSession()
{
  LOG_DEBUG(
      "end keyboard session active=%d restored=%d synthetic=%d pending=%zu lock-baseline=0x%04x",
      m_keyboardSessionActive, m_keyboardStateRestored, hasSyntheticKeys(), m_pendingKeyEvents.size(),
      m_keyboardSessionLockBaseline
  );
  clearPendingKeyEvents();
  if (!m_keyboardSessionActive) {
    return;
  }

  fakeAllKeysUp();
  const auto lockRestore = reconcileModifierLayer(
      m_authoritativeModifiers, m_authoritativeMask, m_keyboardSessionLockBaseline, true, "session lock restore"
  );
  if (!lockRestore.injected || !lockRestore.complete) {
    LOG_WARN(
        "keyboard session ended before the local lock-state baseline could be fully restored (target=0x%04x)",
        m_keyboardSessionLockBaseline
    );
  }

  clearSyntheticState();
  m_keyboardSessionActive = false;
  m_keyboardSessionAuthoritative = false;
  m_keyboardStateRestored = false;
  m_keyboardSessionLockBaseline = 0;
  m_desiredKeyboardState = deskflow::neutralKeyboardModifierState(false);
}

void KeyState::resetKeyboardSession()
{
  LOG_DEBUG(
      "reset keyboard session ledger active=%d restored=%d synthetic=%d pending=%zu", m_keyboardSessionActive,
      m_keyboardStateRestored, hasSyntheticKeys(), m_pendingKeyEvents.size()
  );
  clearPendingKeyEvents();
  clearSyntheticState();
}

bool KeyState::restoreKeyboardSession()
{
  LOG_DEBUG(
      "restore keyboard session active=%d authoritative=%d desired-valid=%d injection-available=%d",
      m_keyboardSessionActive, m_keyboardSessionAuthoritative, m_desiredKeyboardState.valid,
      isKeyInjectionAvailable()
  );
  return m_keyboardSessionActive && m_desiredKeyboardState.valid && reconcileKeyboardState(m_desiredKeyboardState);
}

bool KeyState::fakeMediaKey(KeyID)
{
  return false;
}

bool KeyState::isKeyDown(KeyButton button) const
{
  return (m_keys[button & kButtonMask] > 0);
}

KeyModifierMask KeyState::getActiveModifiers() const
{
  return m_mask;
}

bool KeyState::hasSyntheticKeys() const
{
  return std::ranges::any_of(m_syntheticKeys, [](const auto count) { return count > 0; });
}

KeyModifierMask &KeyState::getActiveModifiersRValue()
{
  return m_mask;
}

int32_t KeyState::getEffectiveGroup(int32_t group, int32_t offset) const
{
  return m_keyMap.getEffectiveGroup(group, offset);
}

bool KeyState::isIgnoredKey(KeyID key, KeyModifierMask) const
{
  switch (key) {
  case kKeyCapsLock:
  case kKeyNumLock:
  case kKeyScrollLock:
    return true;

  default:
    return false;
  }
}

KeyButton KeyState::getButton(KeyID id, int32_t group) const
{
  const deskflow::KeyMap::KeyItemList *items = m_keyMap.findCompatibleKey(id, group, 0, 0);
  if (items == nullptr) {
    return 0;
  } else {
    return items->back().m_button;
  }
}

void KeyState::addAliasEntries()
{
  for (int32_t g = 0, n = m_keyMap.getNumGroups(); g < n; ++g) {
    // if we can't shift any kKeyTab key in a particular group but we can
    // shift kKeyLeftTab then add a shifted kKeyTab entry that matches a
    // shifted kKeyLeftTab entry.
    m_keyMap.addKeyAliasEntry(
        kKeyTab, g, KeyModifierShift, KeyModifierShift, kKeyLeftTab, KeyModifierShift, KeyModifierShift
    );

    // if we have no kKeyLeftTab but we do have a kKeyTab that can be
    // shifted then add kKeyLeftTab that matches a kKeyTab.
    m_keyMap.addKeyAliasEntry(kKeyLeftTab, g, KeyModifierShift, KeyModifierShift, kKeyTab, 0, KeyModifierShift);

    // map non-breaking space to space
    m_keyMap.addKeyAliasEntry(0x20, g, 0, 0, 0xa0, 0, 0);
  }
}

void KeyState::addKeypadEntries()
{
  // map every numpad key to its equivalent non-numpad key if it's not
  // on the keyboard.
  for (int32_t g = 0, n = m_keyMap.getNumGroups(); g < n; ++g) {
    for (size_t i = 0; i < std::size(s_numpadTable); i += 2) {
      m_keyMap.addKeyCombinationEntry(s_numpadTable[i], g, s_numpadTable + i + 1, 1);
    }
  }
}

void KeyState::addCombinationEntries()
{
  for (int32_t g = 0, n = m_keyMap.getNumGroups(); g < n; ++g) {
    // add dead and compose key composition sequences
    const KeyID *i = s_decomposeTable;
    while (*i != 0) {
      // count the decomposed keys for this key
      uint32_t numKeys = 0;
      const KeyID *j = i;
      while (*++j != 0) {
        ++numKeys;
      }

      // add an entry for this key
      m_keyMap.addKeyCombinationEntry(*i, g, i + 1, numKeys);

      // next key
      i += numKeys + 1;
      ++i;
    }
  }
}

bool KeyState::fakeKeys(const Keystrokes &keys, uint32_t count)
{
  // do nothing if no keys or no repeats
  if (count == 0 || keys.empty()) {
    return true;
  }
  if (!isKeyInjectionAvailable()) {
    return false;
  }

  // generate key events
  for (auto k = keys.begin(); k != keys.end();) {
    if (k->m_type == Keystroke::KeyType::Button && k->m_data.m_button.m_repeat) {
      // repeat from here up to but not including the next key
      // with m_repeat == false count times.
      Keystrokes::const_iterator start = k;
      while (count-- > 0) {
        // send repeating events
        for (k = start; k != keys.end() && k->m_type == Keystroke::KeyType::Button && k->m_data.m_button.m_repeat;
             ++k) {
          if (!fakeKey(*k)) {
            return false;
          }
        }
      }

      // note -- k is now on the first non-repeat key after the
      // repeat keys, exactly where we'd like to continue from.
    } else if (k->m_type != Keystroke::KeyType::Group || (!k->m_data.m_group.m_restore && m_isLangSyncEnabled)) {
      // send event
      if (!fakeKey(*k)) {
        return false;
      }

      // next key
      ++k;
    } else {
      LOG_VERBOSE("skipping keystroke, language sync is disabled");
      ++k;
    }
  }

  return true;
}

//
// KeyState::AddActiveModifierContext
//

KeyState::AddActiveModifierContext::AddActiveModifierContext(
    int32_t group, KeyModifierMask mask, ModifierToKeys &activeModifiers
)
    : m_activeGroup(group),
      m_mask(mask),
      m_activeModifiers(activeModifiers)
{
  // do nothing
}
