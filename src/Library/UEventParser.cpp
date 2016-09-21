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
#include <build-config.h>

#include "UEventParser.hpp"
#include "UEvent.hpp"
#include "Logger.hpp"
#include "Common/Utility.hpp"

#include <pegtl/trace.hh>

namespace usbguard
{
  namespace UEventParser
  {
    template<typename Rule>
      struct actions
        : pegtl::nothing<Rule> {};

    template<>
      struct actions<attribute>
      {
        template<typename Input>
          static void apply(const Input& in, UEvent& uevent)
          {
            try {
              const auto p = in.string().find_first_of('=');

              if (p == std::string::npos || p == (in.string().size() - 1)) {
                throw pegtl::parse_error("invalid attribute format", in);
              }

              const String key = in.string().substr(0, p);
              const String value = in.string().substr(p + 1, std::string::npos);

              for (const String header_key : { "ACTION", "DEVPATH" }) {
                if (key == header_key) {
                  if (value != uevent.getAttribute(header_key)) {
                    throw pegtl::parse_error("header value mismatch", in);
                  }
                }
              }

              uevent.setAttribute(key, value);
            }
            catch(const pegtl::parse_error& ex) {
              throw;
            }
            catch(const std::exception& ex) {
              throw pegtl::parse_error(ex.what(), in);
            }
          }
      };

    template<>
      struct actions<action>
      {
        template<typename Input>
          static void apply(const Input& in, UEvent& uevent)
          {
            uevent.setAttribute("ACTION", in.string());
          }
      };

    template<>
      struct actions<devpath>
      {
        template<typename Input>
          static void apply(const Input& in, UEvent& uevent)
          {
            uevent.setAttribute("DEVPATH", in.string());
          }
      };
  } /* namespace UEventParser */

  void parseUEventFromString(const String& uevent_string, UEvent& uevent, bool trace)
  {
    try {
#if HAVE_PEGTL_LTE_1_3_1
      if (!trace) {
        pegtl::parse<UEventParser::grammar, UEventParser::actions>(uevent_string, String(), uevent);
      }
      else {
        pegtl::parse<UEventParser::grammar, UEventParser::actions, pegtl::tracer>(uevent_string, String(), uevent);
      }
#else
      if (!trace) {
        pegtl::parse_string<UEventParser::grammar, UEventParser::actions>(uevent_string, String(), uevent);
      }
      else {
        pegtl::parse_string<UEventParser::grammar, UEventParser::actions, pegtl::tracer>(uevent_string, String(), uevent);
      }
#endif
    }
    catch(...) {
      throw;
    }
  }
} /* namespace usbguard */
