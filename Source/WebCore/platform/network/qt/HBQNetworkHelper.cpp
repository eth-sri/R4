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

/**
 * WebERA
 *
 * Some parts of WebKit creates requests, and queues them for later dispatch.
 * The ORIGIN is the event action creating the request, and the SENDER
 * is the event action initiating the request from the queue. Both should
 * have a HB relation with the response of the request.
 */

static QNetworkRequest::Attribute ATTR_ORIGIN_ID = (QNetworkRequest::Attribute)(QNetworkRequest::User + 4242);
static QNetworkRequest::Attribute ATTR_SENDER_ID = (QNetworkRequest::Attribute)(QNetworkRequest::User + 4243);

void HBQNetworkRequestAnnotate(QNetworkRequest* request, HBQNetworkRequestAnnotateFrom from, WTF::EventActionId eventActionId) {

    QNetworkRequest::Attribute attr = from == ORIGIN ? ATTR_ORIGIN_ID : ATTR_SENDER_ID;

    request->setAttribute(attr, QVariant((int)eventActionId));
}

WTF::EventActionId HBQNetworkRequestGetEventAction(const QNetworkRequest& request, HBQNetworkRequestAnnotateFrom from) {

    QNetworkRequest::Attribute attr = from == ORIGIN ? ATTR_ORIGIN_ID : ATTR_SENDER_ID;
    QVariant requestEventActionId = request.attribute(attr);

    if (requestEventActionId.isNull() || requestEventActionId.type() != QVariant::Int) {
        std::cerr << "Error: QNetworkRequest detected without an associated initiating event action" << std::endl;
        std::exit(1);
    }

    return (WTF::EventActionId)requestEventActionId.toInt();

}

}
