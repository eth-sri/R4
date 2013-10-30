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

#ifndef TIMEPROVIDER_H
#define TIMEPROVIDER_H

namespace JSC {

class TimeProvider {
public:
    virtual ~TimeProvider() {}

    virtual double currentTime() = 0;

    static TimeProvider* getInstance() {
        return TimeProvider::m_instance;
    }

    static void setInstance(TimeProvider* instance) {
        delete m_instance;
        m_instance = instance;
    }

private:
    static TimeProvider* m_instance;
};

class TimeProviderDefault : public TimeProvider {
public:
    virtual double currentTime();
};

}

#endif // TIMEPROVIDER_H
