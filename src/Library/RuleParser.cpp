//
// Copyright (C) 2015 Red Hat, Inc.
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

#include "RuleParser.hpp"
#include "RuleParser/Grammar.hpp"
#include "RuleParser/Actions.hpp"

#include "Typedefs.hpp"
#include "RulePrivate.hpp"
#include "USB.hpp"
#include "Common/Utility.hpp"
#include "Logger.hpp"

#include <cstddef>
#include <stdexcept>
#include <stdlib.h>

#include <pegtl/trace.hh>

namespace usbguard
{
  Rule parseRuleFromString(const String& rule_spec, const String& file, size_t line, bool trace)
  {
    try {
      Rule rule;
#if HAVE_PEGTL_LTE_1_3_1
      if (!trace) {
        pegtl::parse<RuleParser::rule_grammar, RuleParser::rule_parser_actions>(rule_spec, file, rule);
      }
      else {
        pegtl::parse<RuleParser::rule_grammar, RuleParser::rule_parser_actions, pegtl::tracer>(rule_spec, file, rule);
      }
#else
      if (!trace) {
        pegtl::parse_string<RuleParser::rule_grammar, RuleParser::rule_parser_actions>(rule_spec, file, rule);
      }
      else {
        pegtl::parse_string<RuleParser::rule_grammar, RuleParser::rule_parser_actions, pegtl::tracer>(rule_spec, file, rule);
      }
#endif
      return rule;
    }
    catch(const pegtl::parse_error& ex) {
      RuleParserError error(rule_spec);

      error.setHint(ex.what());
#if HAVE_PEGTL_LTE_1_3_1
      error.setOffset(ex.positions[0].column);
#else
      error.setOffset(ex.positions[0].byte_in_line);
#endif

      if (!file.empty() || line != 0) {
        error.setFileInfo(file, line);
      }

      throw error;
    }
    catch(const std::exception& ex) {

      throw;
    }
  }
} /* namespace usbguard */
