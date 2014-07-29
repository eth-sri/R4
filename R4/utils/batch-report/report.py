#!/usr/bin/env python3

import sys
import os
import difflib
import re
import shutil
from jinja2 import Environment, PackageLoader
import subprocess
from builtins import FileNotFoundError, NotADirectoryError
import concurrent.futures
from bs4 import BeautifulSoup

NUM_PROC = 7


class ERRaceClassifier(object):

    CLASSIFICATION = {
        'rs': 'HIGH',
        'ru': 'NORMAL',
        'rk': 'LOW'
    }

    RACE_PARENT_RE = re.compile('(.*race[0-9]+)_race[0-9]+')

    def __init__(self, website_dir):
        
        self.website_dir = website_dir
        self._cache = {}

    def inject_classification(self, race_data, namespace):
        
        handle = race_data['handle']
        id = handle[handle.rindex('race')+4:]

        parent = race_data['origin']

        if parent is None:

            match = self.RACE_PARENT_RE.match(handle)
            if match is None:
                parent = 'base'
            else:
                parent = match.group(1)

        if not parent in self._cache:

            varlist_path = os.path.join(self.website_dir, parent, 'varlist')

            if not os.path.exists(varlist_path):
                
                try:
                    cmd = '$WEBERA_DIR/R4/er-classify.sh %s %s' % \
                          (os.path.join(self.website_dir, parent), namespace)
                    stdout = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
                except subprocess.CalledProcessError as e:
                    print('Failed running %s' % cmd)

            try:
                with open(varlist_path, 'r') as fp:
                    self._cache[parent] = BeautifulSoup(fp, 'html5lib')
            except FileNotFoundError:
                self._cache[parent] = None

        soup = self._cache.get(parent, None)

        if soup is not None:

            try:
                race_row = soup\
                    .find('a', href='race?id=%s' % id)\
                    .find_parent('tr')\
                    .previous_sibling.previous_sibling

                classification = self.CLASSIFICATION[race_row['class'][0][:2]]
                details = race_row.find_all('td')[-1].text
                details = details.strip()

                race_data['er_classification'] = classification
                race_data['er_classification_details'] = details
            except: #Exception as e:
                #raise e
                race_data['er_classification'] = 'PARSE_ERROR'
                race_data['er_classification_details'] = ''
        else:
            race_data['er_classification'] = 'UNKNOWN'
            race_data['er_classification_details'] = ''

# Please look away, and allow me to be a bit quick-and-dirty for a moment

memlist_cache = {}

def get_memory_stats(race_dir, key):

    if race_dir not in memlist_cache:

        memlist_file = os.path.join(race_dir, 'memlist')
        if not os.path.exists(memlist_file) or os.stat(memlist_file).st_size == 0:
            print('Warning,', memlist_file, 'is missing')
            memlist_cache[race_dir] = {}

        else:
            
            RE_memory_location = re.compile('(Variable|Attribute)</td><td>([a-zA-Z\.0-9\[\]\(\)\-_:/\$]*)</td>')
            RE_value = re.compile('Uncovered races:</b> \([a-z ]+\) ([0-9]+)(.*?)<br><b>Covered races:</b> ([0-9]+)(.*?)<br>')

            result = {}

            with open(memlist_file, 'rb') as fp:

                last_location = None

                for line in fp.readlines():
                    line = line.decode('utf8', 'ignore')

                    if last_location is None:
                        match = RE_memory_location.search(line)
                        if match is not None:
                            last_location = match.group(2)
                    
                    else:
                        match = RE_value.search(line)
                        if match is not None:
                            result[last_location] = {
                                'num_uncovered': int(match.group(1)),
                                'num_covered': int(match.group(3)),
                                'uncovered': match.group(2),
                                'covered': match.group(4)
                            }
                        else:
                            print('Warning, could not find information for location', last_location)

                        last_location = None

            memlist_cache[race_dir] = result
            
    return memlist_cache[race_dir].get(key, None)

RE_value_with_memory = re.compile('\[.*\]|event action [0-9]+|IO_[0-9]+|:0x[abcdef\-0-9]+|:[0-9]+|0x[abcdef0-9]+|x_x[0-9]+')

def anon(value, known_ids=[]):
    return RE_value_with_memory.sub('[??]', str(value)) if value not in known_ids else 'ID'

def abstract_memory_equal(handle, m1, m2):

    # start: Some fixes to the approximation

    # (b) id numbers (such as timer ids) can be stored as values, and naturally differ from execution to execution

    def get_known_ids(mem):
        ids = []
        for k,v in mem.items():
            if 'Timer:' in k:
                ids.append(k.split(':')[1])
        
        return ids

    known_ids = get_known_ids(m1)
    known_ids.extend(get_known_ids(m2))
    
    # Memory adresses are not equal in the two memory "diffs". We approximate their equality  by stripping out
    # memory locations, and converting them into anon_memory_location=value strings. 
    # We then check set equality

    memory1 = ['%s=%s' % (anon(location), anon(value, known_ids)) for location, value in m1.items()]

    # start: Some fixes to the approximation 

    # (a) in some cases we observe registered event actions (which we execute) in the reordered sequence
    # but not in the original sequence. This is purely an anomaly in our instrumentation.

    ignore_memory2_keys = []
    for k,v in m2.items():
        if 'event action' in k and '-1' not in k and '%s=%s' % (anon(k), anon(v, known_ids)) not in memory1:
            ignore_memory2_keys.append(k)

    # end: fixes

    memory2 = ['%s=%s' % (anon(location), anon(value, known_ids)) for location, value in m2.items() if k not in ignore_memory2_keys]

    #if not set(memory1) == set(memory2):
        #print('\n\nMISS', "\n", handle, "\n", memory1, "\n\n", memory2, "\n\n", set(memory1) == set(memory2), "\n\n", 
        #     [v for v in memory1 if v not in memory2], "\n\n", 
        #    [k for k,v in m1.items() if '%s=%s' % (anon(k), anon(v, known_ids)) not in memory2], "\n\n",
        #   [v for v in memory2 if v not in memory1], "\n\n",
        #[k for k,v in m2.items() if k not in ignore_memory2_keys and '%s=%s' % (anon(k), anon(v, known_ids)) not in memory1], "\n\n")


    #print(memory1, "\n\n", memory2, "\n\n", set(memory1) == set(memory2), "\n\n", [v for v in memory1 if v not in memory2], "\n\n", [k for k,v in m1.items() if '%s=%s' % (anon(k), anon(v, known_ids)) not in memory2], "\n\n", [v for v in memory2 if v not in memory1], "\n\n", [k for k,v in m2.items() if k not in ignore_memory2_keys and '%s=%s' % (anon(k), anon(v, known_ids)) not in memory1])
    
    return (set(memory1) == set(memory2), 
            [v for v in memory1 if v not in memory2], 
            [v for v in memory2 if v not in memory1],
            [k for k,v in m1.items() if '%s=%s' % (anon(k), anon(v, known_ids)) not in memory2],
            [k for k,v in m2.items() if k not in ignore_memory2_keys and '%s=%s' % (anon(k), anon(v, known_ids)) not in memory1])

def create_if_missing_event_action_code(base_dir, namespace, *args):

    to_be_added = []

    for event_action_id in args:

        code_file = os.path.join(base_dir, 'varlist-%s.code' % event_action_id)
        if not os.path.exists(code_file) or os.stat(code_file).st_size == 0:
            to_be_added.append(event_action_id)

    if len(to_be_added) > 0:
        try:
            cmd = '$WEBERA_DIR/R4/er-code-file.sh %s %s %s' % (base_dir, namespace, ' '.join(to_be_added))
            stdout = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
        except subprocess.CalledProcessError as e:
            print('Failed running %s' % cmd)


def get_or_create_event_action_memory(base_dir, memory, event_action_id):

    RE_operation = re.compile('(read|write|value|start) <.*>(.*)</.*>')

    code_file = os.path.join(base_dir, 'varlist-%s.code' % event_action_id)
    line_num = 0
    with open(code_file, 'rb') as fp:

        last_location = None

        for line in fp:
            line_num += 1
            line = line.decode('utf8', 'ignore')
            
            match = RE_operation.search(line)

            if match is None:
                continue

            operation = match.group(1)
            location_or_value = match.group(2)

            if operation == 'write':
                last_location = location_or_value

            elif operation == 'value' and last_location is not None:
                memory[last_location] = location_or_value
                last_location = None

            elif operation == 'start' and 'event action -1' not in location_or_value:
                # Approximation, is it always safe to filter out event action -1?
                # TODO: Figure out why these are added non-deterministically in some cases
                memory[location_or_value] = memory.get(location_or_value, 0) + 1
                last_location = None

            elif operation == 'read':
                last_location = None

    if line_num == 0:
        print('Warning,', code_file, 'is empty!')
        
    return memory

def get_abstract_memory(base_dir, namespace, event_handler1, event_handler2):

    race_first_id, race_first_descriptor = event_handler1
    race_second_id, race_second_descriptor = event_handler2

    create_if_missing_event_action_code(base_dir, namespace, race_first_id, race_second_id)

    memory = get_or_create_event_action_memory(base_dir, {}, race_first_id)
    memory = get_or_create_event_action_memory(base_dir, memory, race_second_id)

    return memory


class RaceParseException(Exception):
    pass


class HashableDict(dict):

    def __init__(self, *args, **kwargs):
        self.ignore_properties = kwargs.pop('_ignore_properties', None)

        if self.ignore_properties is None:
            self.ignore_properties = []

        super(HashableDict, self).__init__(*args, **kwargs)

    def __cmp__(self, other):
        t = self.__hash__()
        o = other.__hash__()

        if t == o:
            return 0
        elif t < o:
            return -1
        else:
            return 1

    def __eq__(self, other):
        return self.__hash__() == other.__hash__()

    def __hash__(self):
        return hash(tuple(sorted([item for item in self.items() if item[0] not in self.ignore_properties])))


def parse_race(base_dir, handle):
    """
    Outputs data
    """

    handle_dir = os.path.join(base_dir, handle)

    # ERRORS

    errors_file = os.path.join(handle_dir, 'out.errors.log')
    if not os.path.exists(errors_file):
            errors_file = os.path.join(handle_dir, 'errors.log')

    errors = []
    exceptions = []
    new_events = []

    try:
        fp = open(errors_file, 'rb')
    except (FileNotFoundError, NotADirectoryError):
        raise RaceParseException()

    with fp:
        while True:
            header = fp.readline().decode('utf8', 'ignore')

            if header == '':
                break  # EOF reached

            event_action_id, module, description, length = header.split(';')
            length = int(length)

            if length == 0:
                details = ''
            else:
                details = fp.read(length).decode('utf8', 'ignore')

            container = errors
            t = 'error'

            if 'JavaScript_Interpreter' in module:
                continue

            if 'console.log' in module:
                if 'ERROR' in description and \
                   any([buildin_exception_indicator in details for buildin_exception_indicator in [
                       'EvalError', 'RangeError', 'ReferenceError', 'SyntaxError', 'TypeError', 'URIError'
                   ]]):

                    container = exceptions
                    t = 'exception'
                else:
                    continue

            if 'Non-executed event action' in description:
                new_events.append(details)

            container.append(HashableDict(
                event_action_id=event_action_id,
                type=t,
                module=module,
                description=description,
                details=details,
                _ignore_properties=['event_action_id', 'type']
            ))

    # STDOUT

    stdout_file = os.path.join(handle_dir, 'stdout.txt')
    if not os.path.exists(stdout_file):
        stdout_file = os.path.join(handle_dir, 'stdout')        

    with open(stdout_file, 'rb') as fp:
        stdout = fp.read().decode('utf8', 'ignore')

    result_match = re.compile('Result: ([A-Z]+)').search(stdout)
    state_match = re.compile('HTML-hash: ([0-9]+)').search(stdout)

    # Origin

    origin = None
    origin_file = os.path.join(handle_dir, 'origin')

    if os.path.exists(origin_file):
        with open(origin_file, 'rb') as fp:
            origin = str(fp.read().decode('utf8', 'ignore')).strip()

    # SCHEDULE

    schedule_file = os.path.join(handle_dir, 'new_schedule.data')
    if not os.path.isfile(schedule_file):
        schedule_file = os.path.join(handle_dir, 'out.schedule.data')        

    output_schedule_file = os.path.join(handle_dir, 'schedule.data')        

    schedule = []
    raceFirst = ""
    raceFirstIndex = -1
    raceSecond = ""
    raceSecondIndex = -1
    
    with open(schedule_file, 'rb') as fp:

        index = 0
        for line in fp.readlines():

            line = line.decode('utf8', 'ignore')

            if line == '':
                continue

            if '<relax>' in line or '<change>' in line:
                event_action_id = -1
                event_action_descriptor = line

                # This is a hack,
                # If se see <change>, then the next line should be saved as the first racing event
                # and if we see  <relax>, then the next line should be saved as the second racing event.

                if '<change>' in line:
                    raceFirst = None
                
                if '<relax>' in line:
                    raceSecond = None

            else:
                event_action_id, event_action_descriptor = line.split(';', 1)

                # If any race is marked as None, then we should update it now

                if raceFirst is None:
                    raceFirst = (event_action_id, event_action_descriptor)
                    raceFirstIndex = index

                if raceSecond is None:
                    raceSecond = (event_action_id, event_action_descriptor)
                    raceSecondIndex = index

            schedule.append(HashableDict(
                event_action_id=event_action_id,
                type='schedule',
                event_action_descriptor=event_action_descriptor,
                _ignore_properties=['event_action_id', 'type']
            ))

            index += 1

    output_race_first = None
    output_race_second = None

    if raceFirstIndex != -1 and raceSecondIndex != -1:
        with open(output_schedule_file, 'rb') as fp:

            lines = fp.readlines()

            try:
                # subtract 1 for the missing <change> marker
                output_race_first = lines[raceFirstIndex-1].decode('utf8', 'ignore').split(';', 1)

                # subtract 2 for <change> and <relax> markers
                output_race_second = lines[raceSecondIndex-2].decode('utf8', 'ignore').split(';', 1)

            except IndexError:
                output_race_first = None
                output_race_second = None

    # ZIP ERRORS AND SCHEDULE

    zip_errors_schedule = []
    current_event_action_id = None

    buckets = {}

    for s in schedule:
        if s['event_action_id'] == -1:
            continue

        bucket = buckets.get(s['event_action_id'], [])
        bucket.append(s)
        buckets[s['event_action_id']] = bucket
        
    for s in errors:
        bucket = buckets.get(s['event_action_id'], [])
        bucket.append(s)
        buckets[s['event_action_id']] = bucket

    for s in exceptions:
        bucket = buckets.get(s['event_action_id'], [])
        bucket.append(s)
        buckets[s['event_action_id']] = bucket

    for s in schedule:
        if s['event_action_id'] == -1:
            zip_errors_schedule.append(s)
            continue

        try:
            zip_errors_schedule.extend(buckets[s['event_action_id']])
            del buckets[s['event_action_id']]
        except:
            print("Error applying event action", s['event_action_id'], "to zipped schedule")

    for bucket in buckets:
        zip_errors_schedule.extend(buckets[bucket])

    return {
        'handle': handle,
        'errors': errors,
        'exceptions': exceptions,
        'schedule': schedule,
        'zip_errors_schedule': zip_errors_schedule,
        'result': result_match.group(1) if result_match is not None else 'ERROR',
        'html_state': state_match.group(1),
        'race_dir': handle_dir,
        'origin': origin,
        'raceFirst': raceFirst,
        'raceSecond': raceSecond,
        'output_race_first': output_race_first,
        'output_race_second': output_race_second,
        'new_events': new_events
    }


def parse_er_log(base_dir):

    result = {
        'races_total': -1,
        'races_success': -1,
        'races_failure': -1,
        'execution_time': 0,
        'stdout': ''
    }

    try:
        fp = open(os.path.join(base_dir, 'runner', 'stdout.txt'), 'rb')
    except (FileNotFoundError, NotADirectoryError):
        return result

    with fp:
        stdout = fp.read().decode('utf8', 'ignore')
        result['stdout'] = stdout

        if 'WARNING: Stopped iteration' in stdout:
            print('Warning, iteration max reached.')

        result_match = re.compile('Tried ([0-9]+) schedules. ([0-9]+) generated, ([0-9]+)').search(stdout)

        if result_match is not None:
            result['races_total'] = int(result_match.group(2))
            result['races_success'] = int(result_match.group(3))
            result['races_failure'] = int(result_match.group(2)) - int(result_match.group(3))
        else:
            print('FAIL')
            print(stdout)

        time_match = re.compile('real ([0-9]+)\.[0-9]+').search(stdout)

        if time_match is not None:
            result['execution_time'] = int(time_match.group(1))

    return result


def generate_comparison_file(record_file, replay_file, comparison_file):
    try:
        cmd = 'compare -metric RMSE %s %s %s' % (record_file, replay_file, comparison_file)
        print(cmd)
        stdout = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
        type = 'normal'
    except subprocess.CalledProcessError as e:
        if e.returncode == 2:
            return 'error', -1

        stdout = e.output
        type = 'normal'

    if b'image widths or heights differ' in stdout:
        # do a subimage search

        try:
            cmd = 'compare -metric RMSE -subimage-search %s %s %s' % (record_file, replay_file, comparison_file)
            print(cmd)
            stdout = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
            type = 'subimage'
        except subprocess.CalledProcessError as e:
            if e.returncode == 2:
                return 'error', -1

            stdout = e.output
            type = 'subimage'

    match = re.compile(b'([0-9]+\.?[0-9]*) \([0-9]+\.?[0-9]*\)').search(stdout)

    if match is None:
        return 'error', -1

    return type, float(match.group(1))


def cimage_human(type, distance):
    if type == 'normal' and distance == 0:
        return 'EXACT'
    elif type == 'normal' and distance != 0:
        return 'DIFFER'
    elif type == 'subimage':
        return 'SUBSET'
    else:
        return 'ERROR'


def compare_race(base_data, race_data, namespace):
    """
    Outputs comparison
    """

    # Compare image

    cimage_meta = os.path.join(race_data['race_dir'], 'comparison.txt')

    if not os.path.isfile(cimage_meta):

        file1 = os.path.join(base_data['race_dir'], 'out.screenshot.png')
        if not os.path.exists(file1):
            file1 = os.path.join(base_data['race_dir'], 'screenshot.png')

        file2 = os.path.join(race_data['race_dir'], 'out.screenshot.png')
        if not os.path.exists(file2):
            file2 = os.path.join(race_data['race_dir'], 'screenshot.png')

        match_type, distance = generate_comparison_file(
            file1,
            file2,
            os.path.join(race_data['race_dir'], 'comparison.png'))

        cimage = {
            'distance': float(distance),
            'match_type': match_type,
            'human': cimage_human(match_type, distance)
        }

        with open(cimage_meta, 'w') as fp:
            fp.writelines([
                str(cimage['distance']),
                cimage['match_type']
            ])

    else:

        with open(cimage_meta, 'r') as fp:
            line = fp.readline()
            match = re.compile('^(\-?[0-9]+\.?[0-9]*)([a-z]+)$').search(line)

            cimage = {
                'distance': float(match.group(1)),
                'match_type': match.group(2),
                'human': cimage_human(match.group(2), float(match.group(1)))
            }

    def remove_new_event_action(base_list, race_list):
        # Remove "new event action" error at the end of error reports if they both have it
        # Often, this is not an error, just a side effect of the system, and this side effect
        # is different from execution to execution. Filter it away
        if len(base_list) > 0 and len(race_list) > 0:

            if base_list[-1]['type'] == 'error' and race_list[-1]['type'] == 'error' and \
               'event action observed' in base_list[-1]['description'] and \
               'event action observed' in race_list[-1]['description']:

                return base_list[:-1], race_list[:-1]

        return base_list, race_list

    # Errors diff

    base_errors = base_data['errors']
    race_errors = race_data['errors']

    base_errors, race_errors = remove_new_event_action(base_errors, race_errors)

    diff = difflib.SequenceMatcher(None, base_errors, race_errors)
    opcodes = diff.get_opcodes()
    opcodes_human = []

    for opcode in opcodes:
        tag, i1, i2, j1, j2 = opcode

        opcodes_human.append({
            'base': base_errors[i1:i2],
            'tag': tag,
            'race': race_errors[j1:j2]
        })

    errors_distance = sum(1 for opcode in opcodes if opcode[0] != 'equal')

    # Exceptions diff

    base_exceptions = base_data['exceptions']
    race_exceptions = race_data['exceptions']

    exceptions_diff = difflib.SequenceMatcher(None, base_exceptions, race_exceptions)
    exceptions_opcodes = exceptions_diff.get_opcodes()
    exceptions_opcodes_human = []

    for opcode in exceptions_opcodes:
        tag, i1, i2, j1, j2 = opcode

        exceptions_opcodes_human.append({
            'base': base_exceptions[i1:i2],
            'tag': tag,
            'race': race_exceptions[j1:j2]
        })

    exceptions_distance = sum(1 for opcode in exceptions_opcodes if opcode[0] != 'equal')

    # Race and Errors Diff

    base_zip = base_data['zip_errors_schedule']
    race_zip = race_data['zip_errors_schedule']

    base_zip, race_zip = remove_new_event_action(base_zip, race_zip)

    zip_diff = difflib.SequenceMatcher(None, base_zip, race_zip)
    zip_opcodes = zip_diff.get_opcodes()
    zip_opcodes_human = []

    unequal_seen = False

    for zip_opcode in zip_opcodes:
        tag, i1, i2, j1, j2 = zip_opcode

        if not unequal_seen and tag != 'equal':
            unequal_seen = True
            first_unequal = True
        else:
            first_unequal = False

        zip_opcodes_human.append({
            'base': base_zip[i1:i2],
            'tag': tag,
            'race': race_zip[j1:j2],
            'first_unequal': first_unequal
        })

    # R4 race classification

    html_state_match = base_data['html_state'] == race_data['html_state']
    visual_state_match = cimage.get('human', 'FAIL') == 'EXACT'

    classification = 'NORMAL'
    classification_details = ""

    # Low triggers

    # High triggers (classifiers)

    if exceptions_distance > 0:
        classification = 'HIGH'
        classification_details += 'R4_EXCEPTIONS '

    if not visual_state_match and not html_state_match:
        classification = 'HIGH'
        classification_details += 'R4_DOM_AND_RENDER_MISMATCH '

    if 'initialization race' in race_data['er_classification_details']:
        classification = 'HIGH'
        classification_details += 'ER_INITIALIZATION_RACE ' 

    if 'readyStateChange race' in race_data['er_classification_details']:
        classification = 'HIGH'
        classification_details += 'ER_READYSTATECHANGE_RACE ' 

    # Low Triggers (strong update)

    if 'LATE_EVENT_ATTACH' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details += 'ER_LATE_EVENT_ATTACH ' 

    if 'COOKIE_OR_CLASSNAME' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details += 'ER_COOKIE_OR_CLASSNAME '

    if 'MAYBE_LAZY_INIT' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details += 'ER_MAYBE_LAZY_INIT '

    if 'ONLY_LOCAL_WRITE' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details += 'ER_ONLY_LOCAL_WRITE '

    if 'NO_EVENT_ATTACHED' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details += 'ER_NO_EVENT_ATTACHED '

    if 'WRITE_SAME_VALUE' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details += 'ER_WRITE_SAME_VALUE '

    if classification != 'LOW' and race_data['er_classification'] == 'LOW':
        print('Error: Unknown classification',  race_data['er_classification_details'])

    # Ad-hoc sync heuristic
    #
    # If a DOM timer is part of a conflict, and the DOM timer implements the many-reads-ad-hoc-sync pattern,
    # then we should observe that if:
    # - The DOM timer is delayed and moved after another event, then the sync. can happen for that timer and
    #   not the next timer (DELAY).
    # - The DOM timer is executed early and moved before another event, then the sync. will not happen for
    #   this DOM timer and a new (similar) DOM timer is posted (EARLY).
    #

    if not visual_state_match and not html_state_match:

        # Unfinished business heuristic
        # If we have pending events we have a strong indication that the race only delayed some behaviour

        if len(race_data['new_events']) > 0:
            classification = 'LOW'
            classification_details += 'R4_AD_HOC_SYNC_PENDING_EVENTS '

    # Detect the (EARLY) pattern.

    race_first_id, race_first_descriptor = race_data['raceFirst']
    race_first_parts = race_first_descriptor.split(',')

    if '1-DOMTimer' in race_first_descriptor:
        
        # race_first_descriptor would be 
        # 1-DOMTimer(<some url>,x,y,z,w,q)

        # A new DOM timer (which is a continuation of race_first) would be
        # 1-DOMTimer(<some url>,x,y,z,<race_first_id>,q) or
        # 1-DOMTimer(<some url>,x,y,z,w,q+1)

        # We simplify this such that they only need to share a common prefix.

        for new_event in race_data['new_events']:

            new_parts = new_event.split(',')

            if len(new_parts) > 4 and ''.join(race_first_parts[:4]) == ''.join(new_parts[:4]):
                classification = 'LOW'
                classification_details += 'R4_DOM_TIMER_AD_HOC_SYNC[EARLY] '

    # Detect the (DELAY) pattern

    race_second_id, race_second_descriptor = race_data['raceSecond']
    race_second_parts = race_second_descriptor.split(',')

    if len(race_second_parts) > 4 and '1-DOMTimer' in race_second_parts[0]:
        indicator = 'NEXT -> ' + ','.join(race_second_parts[:4])

        for error in race_data['errors']:
            if 'skipped' in error['description'] and indicator in error['details']:
                classification = 'LOW'
                classification_details += 'R4_DOM_TIMER_AD_HOC_SYNC[DELAY] '
                break
                
    if race_data['output_race_first'] is not None and \
       race_data['output_race_second'] is not None:

        # Abstract memory operation patterns

        equal, original_diff, reordered_diff, original_diff_keys, reordered_diff_keys = \
            abstract_memory_equal(
                race_data['race_dir'],
                get_abstract_memory(base_data['race_dir'], namespace, 
                                    race_data['raceSecond'], 
                                    race_data['raceFirst']),  # Find the original memory output 
                get_abstract_memory(race_data['race_dir'], namespace,
                                    race_data['output_race_first'], 
                                    race_data['output_race_second']))  # Find the new memory output        

        if equal: 
            # Pattern 1: Commute
            
            classification = 'LOW'
            classification_details += 'R4_EVENTS_COMMUTE '
            #print("MARKER", race_data['race_dir'], 'R4_EVENTS_COMMUTE')

        """
        elif len(reordered_diff) == 1 and 'Timer[??]=DOMTimer[??]' in reordered_diff[0]:  
            # Pattern 2-a: Do nothing but spawn a future DOM timer
            
            classification = 'LOW'
            classification_details += 'R4_SPAWN_TIMER_AD_HOC[DELAY] '
            #print("MARKER", race_data['race_dir'], 'SPAWN_TIMER_AD_HOC[2A]')

        elif len(reordered_diff) == 0 and len(original_diff) == 2 and \
             'Timer[??]=DOMTimer[??]' in original_diff and \
             ('[??]' in original_diff[0] or '[??]' in original_diff[1]): 
            # Pattern 2-b: Spawn a timer, which is handled later

            classification = 'LOW'
            classification_details += 'R4_SPAWN_TIMER_AD_HOC[EARLY] '
        """

        if classification == 'HIGH':
            if (len(original_diff) > 0 and len(reordered_diff) == 0) or \
               (len(reordered_diff) > 0 and len(original_diff) == 0):

                hit = True
                for key in original_diff_keys:
                    v = get_memory_stats(base_data['race_dir'], key)
                    if v is not None and (v['num_uncovered'] > 0 or v['num_covered'] > 1):
                        hit = False
                        break

                for key in reordered_diff_keys:
                    v = get_memory_stats(race_data['race_dir'], key)
                    if v is not None and (v['num_uncovered'] > 0 or v['num_covered'] > 1):
                        hit = False
                        break

                if hit:
                    classification = 'LOW'
                    classification_details += 'R4_CONTINUATION_AD_HOC '

                    #print("MARKER", race_data['race_dir'], 'CONTINUATION_AD_HOC[3]')
        
        
        #if classification == 'HIGH':
        #   print(race_data['race_dir'], race_data['handle'], "is a candidate", "\n",
        #         '\n'.join([' '.join(["ORIGINAL:", item]) for item in original_diff]),
        #        '\n'.join([' '.join(["REORDERED:", item]) for item in reordered_diff]), "\n======\n\n")

    return {
        'exceptions_diff': exceptions_opcodes,
        'exceptions_diff_human': exceptions_opcodes_human,
        'exceptions_diff_distance': exceptions_distance,
        'errors_diff': opcodes,
        'errors_diff_human': opcodes_human,
        'errors_diff_distance': errors_distance,
        'zip_diff': zip_opcodes,
        'zip_diff_human': zip_opcodes_human,
        'zip_diff_has_unequal': unequal_seen,
        'html_state_match': html_state_match,
        'visual_state_match': cimage,
        'is_equal': base_data['html_state'] == race_data['html_state'] and 'human' in cimage and cimage['human'] == 'EXACT' and errors_distance == 0 and exceptions_distance == 0,
        'r4_classification': classification,
        'r4_classification_details': classification_details
    }


def output_race_report(website, race, jinja, output_dir, input_dir):
    """
    Outputs filename of written report
    """

    record_file = os.path.join(output_dir, 'record.png')
    replay_file = os.path.join(output_dir, '%s-replay.png' % race['handle'])
    comparison_file = os.path.join(output_dir, '%s-comparison.png' % race['handle'])

    try:
        if not os.path.isfile(record_file):
            path = os.path.join(input_dir, website, 'base', 'out.screenshot.png')
            if not os.path.exists(path):
                path = os.path.join(input_dir, website, 'base', 'screenshot.png')

            shutil.copy(path, record_file)

    except FileNotFoundError:
        pass

    try:
        path = os.path.join(input_dir, website, race['handle'], 'out.screenshot.png')
        if not os.path.exists(path):
            path = os.path.join(input_dir, website, race['handle'], 'screenshot.png')
        shutil.copy(path, replay_file)
    except FileNotFoundError:
        pass

    try:
        shutil.copy(os.path.join(input_dir, website, race['handle'], 'comparison.png'), comparison_file)
    except FileNotFoundError:
        try:
            shutil.copy(os.path.join(input_dir, website, race['handle'], 'comparison-0.png'), comparison_file)
        except FileNotFoundError:
            pass

    with open(os.path.join(output_dir, '%s.html' % race['handle']), 'w') as fp:

        fp.write(jinja.get_template('race.html').render(
            website=website,
            race=race
        ))


def output_race_index(website, parsed_races, er_log, jinja, output_dir, input_dir):

    try:
        os.mkdir(output_dir)
    except OSError:
        pass  # folder exists

    for race in parsed_races:
        output_race_report(website, race, jinja, output_dir, input_dir)

    with open(os.path.join(output_dir, 'index.html'), 'w') as fp:

        fp.write(jinja.get_template('race_index.html').render(
            website=website,
            parsed_races=parsed_races,
            er_log=er_log
        ))

def output_website_index(website_index, output_dir, input_dir):

    jinja = Environment(loader=PackageLoader('templates', ''))

    try:
        os.mkdir(output_dir)
    except OSError:
        pass  # folder exists

    summary = {
        'race_result': {
            'equal': 0,
            'diff': 0,
            'timeout': 0,
            'error': 0
        },
        'execution_result': {
            'total': 0,
            'success': 0,
            'failure': 0
        },
        'er_classification_result': {
            'high': 0,
            'normal': 0,
            'low': 0,
            'unknown': 0
        },
        'r4_classification_result': {
            'high': 0,
            'normal': 0,
            'low': 0
        },
        'execution_time': 0,
    }

    items = []

    for item in website_index:

        if item is None:
            continue

        summary['race_result']['equal'] += item['summary']['equal']
        summary['race_result']['diff'] += item['summary']['diff']
        summary['race_result']['timeout'] += item['summary']['timeout']
        summary['race_result']['error'] += item['summary']['error']

        summary['execution_result']['total'] += item['er_log']['races_total']
        summary['execution_result']['success'] += item['er_log']['races_success']
        summary['execution_result']['failure'] += item['er_log']['races_failure']

        summary['execution_time'] += item['er_log']['execution_time']

        summary['er_classification_result']['high'] += item['summary']['er_high']
        summary['er_classification_result']['normal'] += item['summary']['er_normal']
        summary['er_classification_result']['low'] += item['summary']['er_low']
        summary['er_classification_result']['unknown'] += item['summary']['er_unknown']
        summary['r4_classification_result']['high'] += item['summary']['r4_high']
        summary['r4_classification_result']['normal'] += item['summary']['r4_normal']
        summary['r4_classification_result']['low'] += item['summary']['r4_low']

        items.append(item)

    with open(os.path.join(output_dir, 'index.html'), 'w') as fp:

        fp.write(jinja.get_template('index.html').render(
            website_index=items,
            summary=summary
        ))

def process_race(website_dir, race_data, base_data, er_race_classifier, namespace):

    er_race_classifier.inject_classification(race_data, namespace)

    comparison = compare_race(base_data, race_data, namespace)

    return {
        'handle': race_data['handle'],
        'base_data': base_data,
        'race_data': race_data,
        'comparison': comparison
    }


def process(job):

    website, analysis_dir, output_dir, namespace = job

    print ("PROCESSING", website)

    website_dir = os.path.join(analysis_dir, website)
    parsed_races = []
        
    ## Get a list of races ##

    races = os.listdir(website_dir)

    try:
        races.remove('base')
        races.remove('record')
    except ValueError:
        print('Error, missing base or record directory in output dir for %s' % website)
        return None
            
    ignore_files = ['runner', 'arcs.log', 'out.schedule.data', 'new_schedule.data', 'stdout.txt', 'out.ER_actionlog', 'out.log.network.data', 'out.log.time.data', 'out.log.random.data', 'out.status.data']

    races = [race for race in races if not race.startswith('_') and not race in ignore_files]

    ## Parse each race ##

    er_race_classifier = ERRaceClassifier(website_dir)
    er_log = parse_er_log(website_dir)                

    for race in races:

        try:
            race_data = parse_race(website_dir, race)
        except RaceParseException:
            print("Error parsing %s :: %s" % (website, race))
        
        try:
            base_data = parse_race(website_dir, race_data['origin'])
        except RaceParseException:
            print("Error parsing %s :: %s (base)" % website)
            break

        parsed_races.append(process_race(website_dir, race_data, base_data, er_race_classifier, namespace))

    classifiers = ['R4_EXCEPTIONS', 'R4_DOM_AND_RENDER_MISMATCH', 'ER_INITIALIZATION_RACE', 'ER_READYSTATECHANGE_RACE']

    er_tags = ['ER_LATE_EVENT_ATTACH', 'ER_COOKIE_OR_CLASSNAME', 'ER_MAYBE_LAZY_INIT', 'ER_ONLY_LOCAL_WRITE', 
             'ER_NO_EVENT_ATTACHED', 'ER_WRITE_SAME_VALUE']

    r4_tags = ['R4_AD_HOC_SYNC_PENDING_EVENTS', 
             'R4_DOM_TIMER_AD_HOC_SYNC[EARLY]', 'R4_DOM_TIMER_AD_HOC_SYNC[DELAY]', 'R4_EVENTS_COMMUTE', 
             'R4_SPAWN_TIMER_AD_HOC[DELAY]', 'R4_SPAWN_TIMER_AD_HOC[EARLY]', 'R4_CONTINUATION_AD_HOC']

    tags =  ['R4_EXCEPTIONS', 'R4_DOM_AND_RENDER_MISMATCH', 'ER_INITIALIZATION_RACE', 'ER_READYSTATECHANGE_RACE',
             'ER_LATE_EVENT_ATTACH', 'ER_COOKIE_OR_CLASSNAME', 'ER_MAYBE_LAZY_INIT', 'ER_ONLY_LOCAL_WRITE', 
             'ER_NO_EVENT_ATTACHED', 'ER_WRITE_SAME_VALUE', 'R4_AD_HOC_SYNC_PENDING_EVENTS', 
             'R4_DOM_TIMER_AD_HOC_SYNC[EARLY]', 'R4_DOM_TIMER_AD_HOC_SYNC[DELAY]', 'R4_EVENTS_COMMUTE', 
             'R4_SPAWN_TIMER_AD_HOC[DELAY]', 'R4_SPAWN_TIMER_AD_HOC[EARLY]', 'R4_CONTINUATION_AD_HOC']

    data = [website, 
            str(len(parsed_races)),
            str(len([1 for race in parsed_races if race['comparison']['r4_classification'] == 'LOW'])),
            str(len([1 for race in parsed_races if race['comparison']['r4_classification'] == 'NORMAL'])),
            str(len([1 for race in parsed_races if race['comparison']['r4_classification'] == 'HIGH']))]

    def filter_classifiers(details):
        return [c for c in details.split(' ') if c not in classifiers]

    def only_classifiers(details):
        return [c for c in details.split(' ') if c in classifiers]


    #for tag in tags:
    #        if tag in classifiers:
    #           data.append(str(len([1 for race in parsed_races if tag in only_classifiers(race['comparison']['r4_classification_details']) and len(only_classifiers(race['comparison']['r4_classification_details'])) == 1])))
    #      else:
    #         data.append(str(len([1 for race in parsed_races if tag in filter_classifiers(race['comparison']['r4_classification_details']) and len(filter_classifiers(race['comparison']['r4_classification_details'])) == 1])))

    for tag in tags:
        data.append(str(len([1 for race in parsed_races if tag in race['comparison']['r4_classification_details']])))

    # filtered by ER
    data.append(str(len([1 for race in parsed_races if 
                         any([tag in er_tags for tag in race['comparison']['r4_classification_details'].split(' ')])])))

    # filtered by R4
    data.append(str(len([1 for race in parsed_races if
                         any([tag in r4_tags for tag in race['comparison']['r4_classification_details'].split(' ')])])))


    ## Output statistics
    print(','.join(data))

    ## Generate HTML files ##

    jinja = Environment(loader=PackageLoader('templates', ''))

    website_output_dir = os.path.join(output_dir, website)
    output_race_index(website, parsed_races, er_log, jinja, website_output_dir, analysis_dir)

    ## End ##

    summary = {
        'equal': len([race for race in parsed_races if race['race_data']['result'] == 'FINISHED' and race['comparison']['is_equal']]),
        'diff': len([race for race in parsed_races if race['race_data']['result'] == 'FINISHED' and not race['comparison']['is_equal']]),
        'timeout': len([race for race in parsed_races if race['race_data']['result'] == 'TIMEOUT']),
        'error': len([race for race in parsed_races if race['race_data']['result'] == 'ERROR']),
        'er_high': len([race for race in parsed_races if race['race_data']['er_classification'] == 'HIGH']),
        'er_normal': len([race for race in parsed_races if race['race_data']['er_classification'] == 'NORMAL']),
        'er_low': len([race for race in parsed_races if race['race_data']['er_classification'] == 'LOW']),
        'er_unknown': len([race for race in parsed_races if race['race_data']['er_classification'] in ['UNKNOWN', 'PARSE_ERROR']]),
        'r4_high': len([race for race in parsed_races if race['comparison']['r4_classification'] == 'HIGH']),
        'r4_normal': len([race for race in parsed_races if race['comparison']['r4_classification'] == 'NORMAL']),
        'r4_low': len([race for race in parsed_races if race['comparison']['r4_classification'] == 'LOW'])
    }

    return {
        'website': website,
        'er_log': er_log,
        'summary': summary
    }

def main():

    try:
        analysis_dir = sys.argv[1]
        output_dir = sys.argv[2]
    except IndexError:
        print('Usage: %s <analysis-dir> <report-dir>' % sys.argv[0])
        sys.exit(1)

    websites = os.listdir(analysis_dir)

    with concurrent.futures.ProcessPoolExecutor(NUM_PROC) as executor:
        website_index = executor.map(process, [(websites[index], analysis_dir, output_dir, str(8001+index)) for index in range(0, len(websites))])
        #website_index = [process([website, analysis_dir, output_dir, "8001"]) for website in websites]
        output_website_index(website_index, output_dir, analysis_dir)

if __name__ == '__main__':
    main()
