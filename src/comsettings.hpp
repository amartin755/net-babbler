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

#ifndef COMSETTINGS_HPP
#define COMSETTINGS_HPP

#include <string>
#include <regex>
#include <stdexcept>

class cComSettings
{
public:
    cComSettings (const std::string s)
    {
        m_disconnect = false;
        // size -> fixed
        // min,max -> rand
        // reqMin,reqMax,resMin,resMax -> rand
        // min,max,width ->sweep
        // reqMin,reqMax,resMin,resMax,width -> sweep
//        std::regex re{R"(^[0-9]+(,[0-9]+)*$)"};
/*
        auto const glob = std::regex{R"(^[0-9]+(,[0-9]+)*$)"};
        if (std::regex_match (s, glob))
            Console::Print ("MATCH\n");
*/
        auto const re = std::regex{R"(,)"};
        auto const vec = std::vector<std::string>(
            std::sregex_token_iterator{begin(s), end(s), re, -1},
            std::sregex_token_iterator{});

        m_stepWidth = 0;
        switch (vec.size())
        {
        case 1:
            m_requestSizeMin = m_requestSizeMax =
                m_responseSizeMin = m_responseSizeMax = (unsigned)std::stoul(vec.at(0));
            break;
        case 2:
            m_requestSizeMin = m_responseSizeMin = (unsigned)std::stoul(vec.at(0));
            m_requestSizeMax = m_responseSizeMax = (unsigned)std::stoul(vec.at(1));
            break;
        case 3:
            m_requestSizeMin = m_responseSizeMin = (unsigned)std::stoul(vec.at(0));
            m_requestSizeMax = m_responseSizeMax = (unsigned)std::stoul(vec.at(1));
            m_stepWidth = (unsigned)std::stoul(vec.at(3));
            break;
        case 4:
            m_requestSizeMin  = (unsigned)std::stoul(vec.at(0));
            m_requestSizeMax  = (unsigned)std::stoul(vec.at(1));
            m_responseSizeMin = (unsigned)std::stoul(vec.at(2));
            m_responseSizeMax = (unsigned)std::stoul(vec.at(3));
            break;
        case 5:
            m_requestSizeMin  = (unsigned)std::stoul(vec.at(0));
            m_requestSizeMax  = (unsigned)std::stoul(vec.at(1));
            m_responseSizeMin = (unsigned)std::stoul(vec.at(2));
            m_responseSizeMax = (unsigned)std::stoul(vec.at(3));
            m_stepWidth       = (unsigned)std::stoul(vec.at(4));
            break;
        default:
            throw std::range_error ("invalid settings");
        }

    }
    // fixed size
    cComSettings (unsigned size) :
        m_requestSizeMin (size),
        m_requestSizeMax (size),
        m_responseSizeMin (size),
        m_responseSizeMax (size),
        m_stepWidth (0),
        m_disconnect (false)
    {
    }
    // random, equal size for request and response
    cComSettings (unsigned min, unsigned max) :
        m_requestSizeMin (min),
        m_requestSizeMax (max),
        m_responseSizeMin (min),
        m_responseSizeMax (max),
        m_stepWidth (0),
        m_disconnect (false)
    {
    }
    // random, independent size for request and response
    cComSettings (unsigned requestSizeMin, unsigned requestSizeMax,
        unsigned responseSizeMin, unsigned responseSizeMax) :
        m_requestSizeMin (requestSizeMin),
        m_requestSizeMax (requestSizeMax),
        m_responseSizeMin (responseSizeMin),
        m_responseSizeMax (responseSizeMax),
        m_stepWidth (0),
        m_disconnect (false)
    {
    }
    // sweep, equal size for request and response
    cComSettings (unsigned min, unsigned max, unsigned stepWidth) :
        m_requestSizeMin (min),
        m_requestSizeMax (max),
        m_responseSizeMin (min),
        m_responseSizeMax (max),
        m_stepWidth (stepWidth),
        m_disconnect (false)
    {
    }
    // sweep, independent size for request and response
    cComSettings (unsigned requestSizeMin, unsigned requestSizeMax,
        unsigned responseSizeMin, unsigned responseSizeMax, unsigned stepWidth) :
        m_requestSizeMin (requestSizeMin),
        m_requestSizeMax (requestSizeMax),
        m_responseSizeMin (responseSizeMin),
        m_responseSizeMax (responseSizeMax),
        m_stepWidth (stepWidth),
        m_disconnect (false)
    {
    }

    bool isFixed () const
    {
        return m_requestSizeMin == m_requestSizeMax && m_responseSizeMin == m_responseSizeMax;
    }
    bool isRand () const
    {
        return !isFixed () && !m_stepWidth;
    }
    bool isSweep () const
    {
        return !isFixed () && m_stepWidth;
    }

    unsigned m_requestSizeMin;
    unsigned m_requestSizeMax;
    unsigned m_responseSizeMin;
    unsigned m_responseSizeMax;
    unsigned m_stepWidth;
    bool m_disconnect;
};


#endif