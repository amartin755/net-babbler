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

#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP

#include <semaphore.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

#include "bug.hpp"


class cSemaphore
{
public:
    cSemaphore (unsigned int initval = 0)
    {
        std::memset (&m_sem, 0, sizeof (m_sem));
        if (sem_init (&m_sem, 0, initval))
            throwError (errno);
    }
    void post ()
    {
        if (sem_post (&m_sem))
            throwError (errno);
    }
    void wait ()
    {
        if (sem_wait (&m_sem))
            throwError (errno);
    }

    ~cSemaphore ()
    {
        sem_close (&m_sem);
        std::memset (&m_sem, 0, sizeof (m_sem));
    }

private:
    void throwError (int err) const
    {
        const char* ret = strerrordesc_np (err);
        throw std::runtime_error (ret ? ret : "");
    }
    sem_t m_sem;
};

#endif