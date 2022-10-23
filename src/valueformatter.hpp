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

#ifndef VALUEFORMATTER_HPP
#define VALUEFORMATTER_HPP

#include <sstream>
#include <string>

class cValueFormatter
{
public:
    template <typename T>
    static std::string toHumanReadable (T value, bool binary)
    {
        std::ostringstream out;
        out.precision (2);

        if (!binary)
        {
            if (value > 1000000000)
            {
                out << std::fixed << (value / 1000000000.0) << " G";
            }
            else if (value > 1000000)
            {
                out << std::fixed << (value / 1000000.0) << " M";
            }
            else if (value > 1000)
            {
                out << std::fixed << (value / 1000.0) << " k";
            }
            else
            {
                out << value;
            }
        }
        else
        {
            if (value > 1024*1024*1024)
            {
                out << std::fixed << value / (double)(1024*1024*1024) << " Gi";
            }
            else if (value > 1024*1024)
            {
                out << std::fixed << value / (double)(1024*1024) << " Mi";
            }
            else if (value > 1024)
            {
                out << std::fixed << value / 1024.0 << " Ki";
            }
            else
            {
                out << value;
            }
        }
        return out.str();
    }
};
#endif