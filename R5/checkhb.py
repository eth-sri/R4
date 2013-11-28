#!/usr/bin/python
__author__ = 'semadk'

import sys

if __name__ == '__main__':

    if len(sys.argv) != 3:
        print('Usage: %s arc.log schedule.data' % sys.argv[0])
        sys.exit(1)

    idToLine = {}

    with open(sys.argv[2], 'r') as sfp:
        linenum = 1

        for line in sfp:
            linenum += 1

            if line == '<relax>':
                continue

            idnum, rest = line.split(';', 1)

            idToLine[int(idnum)] = linenum

    with open(sys.argv[1], 'r') as afp:

        for line in afp:

            tail_raw, head_raw = line.split('->')
            tail, head = int(tail_raw.strip()), int(head_raw.strip())

            if tail in idToLine and head in idToLine and not idToLine[tail] < idToLine[head]:
                print('Error, %s -> %s is not ordered, see line %s and %s' % (tail, head, idToLine[tail], idToLine[head]))
