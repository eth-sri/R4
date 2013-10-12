/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <limits>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QString>

#include <WebCore/eventaction/EventActionSchedule.h>
#include <WebCore/eventaction/EventActionHB.h>
#include <WebCore/eventaction/EventActionDescriptor.h>

WebCore::EventActionsHB* readHappensBeforeData(const std::string& filepath)
{
    std::ifstream fp;
    fp.open(filepath);
    WebCore::EventActionsHB* hb = WebCore::EventActionsHB::deserialize(fp);
    fp.close();

    return hb;
}

WebCore::EventActionSchedule* readScheduleData(const std::string& filepath)
{
    std::ifstream fp;
    fp.open(filepath);
    WebCore::EventActionSchedule* schedule = WebCore::EventActionSchedule::deserialize(fp);
    fp.close();

    return schedule;
}

WebCore::EventActionSchedule::const_iterator findFirstOccurrenceOfRelation(const WebCore::EventActionsHB* const hb,
                                                                           const WebCore::EventActionDescriptor needle,
                                                                           WebCore::EventActionSchedule::const_iterator start,
                                                                           WebCore::EventActionSchedule::const_iterator end)
{
    WebCore::EventActionSchedule::const_iterator pointer;

    for (pointer = start; pointer != end; pointer++) {

        if (hb->haveAnyOrderRelation(needle, (*pointer))) {
            return pointer;
        }
    }

    return end;
}

WebCore::EventActionSchedule* reorder(unsigned int from,
                                     unsigned int to,
                                     WebCore::EventActionSchedule* original)
{

    WebCore::EventActionSchedule* newSchedule =  new WebCore::EventActionSchedule(*original);

    WebCore::EventActionDescriptor item = newSchedule->at(from);
    newSchedule->remove(from);

    if (to == UINT_MAX) {
        newSchedule->append(item);
    } else {
        newSchedule->insert(to-1, item); // -1 because we have removed an item thus the index has moved
    }

    ASSERT(newSchedule->size() == original->size());

    return newSchedule;
}

void save(WebCore::EventActionSchedule* schedule, unsigned int serialId)
{
    std::ostringstream filename;
    filename << "/tmp/schedule-" << serialId << ".data";

    std::ofstream outputfp;
    outputfp.open(filename.str());
    schedule->serialize(outputfp);
    outputfp.close();
}


int main(int argc, char **argv)
{
    if (argc == 1) {
        std::cout << "Usage: " << argv[0] << " <schedule.data> <hb.data> <output-dir>" << std::endl;
        std::exit(0);
    }

    WebCore::EventActionSchedule* original = readScheduleData(argv[1]);
    WebCore::EventActionsHB* hb = readHappensBeforeData(argv[2]);

    // This implements a worst-case delay reordering
    //
    // For each event action in the schedule we try to prolong its occurrence for as long as possible
    // However, we will only try to reorder a single item in the schedule, we will not try to reorder
    // entire blocks of event actions in a schedule - this is mainly for simplicity.

    // Step: For each event action, A, in the schedule

        // Case: If A is the last event action in the schedule then ignore A.

        // Step: find the first subsequent occurrence of an event action, B, for which there exist an
        // happens before relation A->B.

            // Case: If the distance between A and B is ONE, that is B immediatly follows A, then ignore this
            // case.

            // Case: If the distance is infinite, that is there exist no event action B, then move A to the
            // last position in the schedule.

            // Default: Move A to the position just before B.

    unsigned int serialId = 0;

    for (WebCore::EventActionSchedule::const_iterator A = original->begin(); A != original->end(); A++) {

        if ((A+1) == original->end()) {
            continue;
        }

        WebCore::EventActionSchedule::const_iterator B = findFirstOccurrenceOfRelation(hb, (*A), A+1, original->end());

        unsigned int distance = B - A;

        if (distance == 1) {
            continue;
        }

        unsigned int Apos = A-original->begin();
        unsigned int Bpos = (B == original->end()) ? UINT_MAX : B-original->begin();

        std::cout << "Move: " << Apos << " -> " << Bpos << std::endl;

        WebCore::EventActionSchedule* newSchedule = reorder(Apos,
                                                           Bpos,
                                                           original);

        save(newSchedule, serialId);
        serialId++;
    }

}
