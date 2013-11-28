/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "HBQNetworkHelper.h"

#include <iostream>
#include <stdlib.h>

#include <wtf/ExportMacros.h>
#include <wtf/Assertions.h>

namespace WebCore
{

static QNetworkRequest::Attribute ATTR_ID = (QNetworkRequest::Attribute)(QNetworkRequest::User + 4242);

void HBQNetworkRequestAnnotate(QNetworkRequest* request) {

    if (HBIsCurrentEventActionValid()) {
        request->setAttribute(ATTR_ID, QVariant((int)HBCurrentEventAction()));
    } else {
        request->setAttribute(ATTR_ID, QVariant((int)HBLastUIEventAction()));
    }

}


WTF::EventActionId HBQNetworkRequestGetEventAction(const QNetworkRequest& request) {

    QVariant requestEventActionId = request.attribute(ATTR_ID);

    if (requestEventActionId.isNull() || requestEventActionId.type() != QVariant::Int) {
        std::cerr << "Error: QNetworkRequest detected without an associated initiating event action" << std::endl;
        std::exit(1);
    }

    return (WTF::EventActionId)requestEventActionId.toInt();

}

}
