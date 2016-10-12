//
// Copyright (C) 2016 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#pragma once

#include "Typedefs.hpp"
#include <pegtl.hh>

using namespace pegtl;

namespace usbguard
{
  class UEvent;

  namespace UEventParser
  {
    struct value
      : seq<not_one<'\0'>, star<not_one<'\0'>>> {};

    struct key
      : seq<ranges<'A', 'Z'>, star<ranges<'A', 'Z', '0', '9'>>> {};

    struct attribute
      : seq<key, one<'='>, value> {};

    struct attributes
      : list<attribute, one<'\0'>> {};

    struct action
      : seq<ranges<'a', 'z'>, star<ranges<'a', 'z'>>> {};

    struct devpath
      : seq<ascii::print, star<ascii::print>> {};

    struct header
      : seq<action, one<'@'>, devpath, one<'\0'>> {};

    struct uevent
      : seq<header, attributes> {};

    struct grammar
      : until<eof, uevent> {};

  } /* namespace UEventParser */

  void parseUEventFromString(const String& uevent_string, UEvent& uevent, bool trace = false);
} /* namespace usbguard */
