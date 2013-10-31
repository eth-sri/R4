/*
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include <limits.h>
#include <wtf/ExportMacros.h>
#include <wtf/StdLibExtras.h>

#ifndef RANDOMPROVIDER_H
#define RANDOMPROVIDER_H

namespace JSC {

class RandomProvider {
public:
    virtual ~RandomProvider() {}

    virtual double get() = 0;
    virtual unsigned getUint32() = 0;

    static RandomProvider* getInstance() {
        return RandomProvider::m_instance;
    }

    static void setInstance(RandomProvider* instance) {
        delete m_instance;
        m_instance = instance;
    }

private:
    static RandomProvider* m_instance;
};

/*
 * This class copies the logic from the real random function in WebKit
 */
class RandomProviderDefault : public RandomProvider {
public:
    RandomProviderDefault()
        : m_low(0 ^ 0x49616E42)
        , m_high(0)
    {
    }

    virtual double get()
    {
        return advance() / (UINT_MAX + 1.0);
    }

    virtual unsigned getUint32()
    {
        return advance();
    }

private:
    unsigned advance()
    {
        m_high = (m_high << 16) + (m_high >> 16);
        m_high += m_low;
        m_low += m_high;
        return m_high;
    }

    unsigned m_low;
    unsigned m_high;
};

}

#endif // RANDOMPROVIDER_H
