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

#ifndef PARAMETERPARSER_HPP
#define PARAMETERPARSER_HPP

#include <utility>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include <list>

class cParameterParser
{
public:
    static std::pair<unsigned long, unsigned long> range (const char* s)
    {
        char* end;
        unsigned long a = std::strtoul (s, &end, 0);
        if (errno == ERANGE)
            throw std::out_of_range ();
        if (*end == '\0')
            return std::pair <unsigned long, unsigned long> (a, a);
        else if (*end != '-')
            throw std::invalid_argument ();

        unsigned b = std::strtoul (end, &end, 0);
        if (errno == ERANGE)
            throw std::out_of_range ();

        return std::pair <unsigned long, unsigned long> (a, b);
    }
    static std::list<std::pair<unsigned long, unsigned long>> rangeList (const char* s)
    {
    }

};
#endif