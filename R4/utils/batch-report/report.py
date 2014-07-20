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

    def inject_classification(self, race_data):
        
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

            try:
                with open(os.path.join(self.website_dir, parent, 'varlist'), 'r') as fp:
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
                if 'ERROR' in description:
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
    if not os.path.isfile(schedule_file):
        schedule_file = os.path.join(handle_dir, 'schedule.data')        

    schedule = []
    raceFirst = ""
    raceSecond = ""
    
    with open(schedule_file, 'rb') as fp:

        for line in fp.readlines():

            line = line.decode('utf8', 'ignore')

            if line == '':
                break

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

                if raceSecond is None:
                    raceSecond = (event_action_id, event_action_descriptor)

            schedule.append(HashableDict(
                event_action_id=event_action_id,
                type='schedule',
                event_action_descriptor=event_action_descriptor,
                _ignore_properties=['event_action_id', 'type']
            ))

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

        zip_errors_schedule.extend(buckets[s['event_action_id']])
        del buckets[s['event_action_id']]

    for bucket in buckets:
        zip_errors_schedule.extend(bucket)

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
            result['races_total'] = int(result_match.group(1))
            result['races_success'] = int(result_match.group(3))
            result['races_failure'] = int(result_match.group(2)) - result['races_success']
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


def compare_race(base_data, race_data):
    """
    Outputs comparison
    """

    # Compare image

    cimage_meta = os.path.join(race_data['race_dir'], 'comparison.txt')

    if not os.path.isfile(cimage_meta):

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

        file1 = os.path.join(base_data['race_dir'], 'out.screenshot.png')
        if not os.path.exists(file1):
            file1 = os.path.join(base_data['race_dir'], 'screenshot.png')

        file2 = os.path.join(race_data['race_dir'], 'out.screenshot.png')
        if not os.path.exists(file2):
            file2 = os.path.join(race_data['race_dir'], 'screenshot.png')


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
        if len(base_list) > 0 and len(race_list) > 0 and \
            base_list[-1]['type'] == 'error' and race_list[-1]['type'] == 'error' and \
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

    # Low triggers

    classification = 'LOW'
    classification_details = None

    if not visual_state_match:
        classification = 'LOW'
        classification_details = 'Visual state mismatch'

    # Medium triggers

    if errors_distance > 0:
        classification = 'NORMAL'
        classification_details = 'Errors count diff'

    if not html_state_match:
        classification = 'NORMAL'
        classification_details = 'HTML state mismatch'

    # High triggers

    if exceptions_distance > 0:
        classification = 'HIGH'
        classification_details = 'Exception count diff'

    if not visual_state_match and not html_state_match:
        classification = 'HIGH'
        classification_details = 'Visual and DOM state mismatch'

    # Medium Triggers (strong update)

    if not visual_state_match and not html_state_match:

        # Unfinished business heuristic
        # If we have pending events we have a strong indication that the race only delayed some behaviour
        race_first_id, race_first_descriptor = race_data['raceFirst']
        race_first_parts = race_first_descriptor.split(',')

        if len(race_data['new_events']) > 0:
            classification = 'NORMAL'
            classification_details = 'PENDING_EVENTS' 

    # Low Triggers (strong update)

    if 'LATE_EVENT_ATTACH ' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details = 'LATE_EVENT_ATTACH' 

    if 'COOKIE_OR_CLASSNAME' in race_data['er_classification_details']:
        classification = 'LOW'
        classification_details = 'COOKIE_OR_CLASSNAME'

    # Ad-hoc sync heuristic
    # If a DOM timer is swapped in the conflict, and a continuation of that DOM timer appears as a new event,
    # then we have an indication of a ad-hoc synchronisation (DOM timer reading a memory location, until 
    # the memory location changes and the DOM timer reacts.

    race_first_id, race_first_descriptor = race_data['raceFirst']
    race_first_parts = race_first_descriptor.split(',')

    for new_event in race_data['new_events']:

        # race_first_descriptor would be 
        # 1-DOMTimer(<some url>,x,y,z,w,q)

        # A new DOM timer (which is a continuation of race_first) would be
        # 1-DOMTimer(<some url>,x,y,z,<race_first_id>,q) or
        # 1-DOMTimer(<some url>,x,y,z,w,q+1)

        new_parts = new_event.replace(')', ',').split(',')

        if len(new_parts) < 4 or len(race_first_parts) < 4:
            continue

        #print(race_first_id, race_first_descriptor, new_event, "\nPART1",  ','.join(race_first_parts[:-3]), "\nPART2", ','.join(new_parts[:-3]), "\nCOMPARE",  ','.join(race_first_parts[:-3]) == ','.join(new_parts[:-3]), "\nHEU1", new_parts[-3] == race_first_id and new_parts[-2] == race_first_parts[-2], "\nHEU2",                     (new_parts[-3] == race_first_parts[-3] and int(new_parts[-2]) == int(race_first_parts[-2]+1)))

        if ','.join(race_first_parts[:-3]) == ','.join(new_parts[:-3]): 

            if (new_parts[-3] == race_first_id and new_parts[-2] == race_first_parts[-2]) or \
                    (new_parts[-3] == race_first_parts[-3] and int(new_parts[-2]) == int(race_first_parts[-2]+1)):

                classification = 'LOW'
                classification_details = 'DOM_TIMER_AD_HOC_SYNC'

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

def process_race(website_dir, race, base_data, er_race_classifier):
    race_data = parse_race(website_dir, race)
    er_race_classifier.inject_classification(race_data)

    comparison = compare_race(base_data, race_data)

    return {
        'handle': race,
        'base_data': base_data,
        'race_data': race_data,
        'comparison': comparison
    }


def process(job):

    website, analysis_dir, output_dir = job

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

    for ignore_file in ignore_files:
        if ignore_file in races:
            races.remove(ignore_file)

    ## Parse each race ##

    try:
        base_data = parse_race(website_dir, 'base')
    except RaceParseException:
        print("Error parsing %s :: base" % website)
        return None

    er_race_classifier = ERRaceClassifier(website_dir)
    er_log = parse_er_log(website_dir)                

    for race in races:
        try:
            parsed_races.append(process_race(website_dir, race, base_data, er_race_classifier))
        except RaceParseException:
            print("Error parsing %s :: %s" % (website, race))

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
        website_index = executor.map(process, [(website, analysis_dir, output_dir) for website in websites])
        output_website_index(website_index, output_dir, analysis_dir)

if __name__ == '__main__':
    main()
