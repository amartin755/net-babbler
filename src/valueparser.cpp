// SPDX-License-Identifier: GPL-3.0-only
/*
 * NET-BABBLER <https://github.com/amartin755/net-babbler>
 * Copyright (C) 2022 Andreas Martin (netnag@mailbox.org)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdexcept>
#include <list>
#include <regex>

#include "bug.hpp"
#include "valueparser.hpp"


std::pair<unsigned long, unsigned long> cValueParser::range (const std::string& s)
{
    // yes, this is overkill just for parsing two integers separated by a minus
    // but I don't want to manualy parse and think about the corner cases

    // expected: number-numer, whitespaces are allowed
    if (!std::regex_match (s, std::regex(R"(\s*\d+\s*(-\s*\d+\s*)?$)")))
        throw std::invalid_argument (s);

    auto const re = std::regex(R"(\d+)");
    auto const vec = std::vector<std::string>(
        std::sregex_token_iterator{begin(s), end(s), re},
        std::sregex_token_iterator{});
    const size_t found = vec.size ();

    // this would mean the expression in regex_match above is buggy
    BUG_ON (found > 2 || found == 0);

    unsigned long a, b;
    a = b = std::stoul (vec[0]);
    if (found == 2)
        b = std::stoul (vec[1]);

    if (a > b)
        throw std::invalid_argument ("The beginning of the range is greater than the end");

    return std::pair <unsigned long, unsigned long> (a, b);
}

std::list<std::pair<unsigned long, unsigned long>> cValueParser::rangeList (const std::string& s)
{
    if (!std::regex_match (s, std::regex(R"(^(\s*\d+\s*(-\s*\d+\s*)?)(,(\s*\d+\s*(-\s*\d+\s*)?))*$)")))
        throw std::invalid_argument (s);

    auto const re = std::regex(R"(\s*\d+\s*(-\s*\d+\s*)?)");
    auto const vec = std::vector<std::string>(
        std::sregex_token_iterator{begin(s), end(s), re},
        std::sregex_token_iterator{});
    const size_t found = vec.size ();

    // this would mean the expression in regex_match above is buggy
    BUG_ON (found < 1);

    std::list<std::pair<unsigned long, unsigned long>> ret;

    for (const auto& val : vec)
    {
        ret.push_back (range (val));
    }
    return ret;
}
