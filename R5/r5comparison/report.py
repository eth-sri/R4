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

NUM_PROC = 7


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

    errors_file = os.path.join(handle_dir, 'errors.log')
    errors = []

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

            errors.append(HashableDict(
                event_action_id=event_action_id,
                type='error',
                module=module,
                description=description,
                details=details,
                _ignore_properties=['event_action_id', 'type']
            ))

    # STDOUT

    stdout_file = os.path.join(handle_dir, 'stdout.txt')

    with open(stdout_file, 'rb') as fp:
        stdout = fp.read().decode('utf8', 'ignore')

    result_match = re.compile('Result: ([A-Z]+)').search(stdout)
    state_match = re.compile('HTML-hash: ([0-9]+)').search(stdout)

    # SCHEDULE

    schedule_file = os.path.join(handle_dir, 'new_schedule.data')
    schedule = []

    with open(schedule_file, 'rb') as fp:

        for line in fp.readlines():

            line = line.decode('utf8', 'ignore')

            if line == '':
                break

            if '<relax>' in line or '<change>' in line:
                event_action_id = -1
                event_action_descriptor = line
            else:
                event_action_id, event_action_descriptor = line.split(';', 1)

            schedule.append(HashableDict(
                event_action_id=event_action_id,
                type='schedule',
                event_action_descriptor=event_action_descriptor,
                _ignore_properties=['event_action_id', 'type']
            ))

    # ZIP ERRORS AND SCHEDULE

    zip_errors_schedule = []
    errors_index = 0
    schedule_index = 0
    current_event_action_id = None

    while True:

        if current_event_action_id is None:
            zip_errors_schedule.append(schedule[schedule_index])

            current_event_action_id = zip_errors_schedule[-1]['event_action_id']
            schedule_index += 1

        elif errors_index < len(errors) and \
                errors[errors_index]['event_action_id'] == current_event_action_id:

            zip_errors_schedule.append(errors[errors_index])

            errors_index += 1

        elif schedule_index < len(schedule):
            zip_errors_schedule.append(schedule[schedule_index])

            current_event_action_id = zip_errors_schedule[-1]['event_action_id']
            schedule_index += 1

        else:
            break

    return {
        'handle': handle,
        'errors': errors,
        'schedule': schedule,
        'zip_errors_schedule': zip_errors_schedule,
        'stdout': stdout,
        'result': result_match.group(1) if result_match is not None else 'ERROR',
        'html_state': state_match.group(1),
        'race_dir': handle_dir
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

        result_match = re.compile('Tried ([0-9]+) schedules. ([0-9]+) generated, ([0-9]+) successful').search(stdout)

        if result_match is not None:
            result['races_total'] = int(result_match.group(1))
            result['races_success'] = int(result_match.group(3))
            result['races_failure'] = result['races_total'] - result['races_success']

        time_match = re.compile('real ([0-9]+)\.[0-9]+').search(stdout)

        if time_match is not None:
            result['execution_time'] = int(time_match.group(1))

    return result


def generate_comparison_file(record_file, replay_file, comparison_file):
    try:
        stdout = subprocess.check_output('compare -metric RMSE %s %s %s' % (record_file, replay_file, comparison_file), stderr=subprocess.STDOUT, shell=True)
        type = 'normal'
    except subprocess.CalledProcessError as e:
        if e.returncode == 2:
            return 'error', -1

        stdout = e.output
        type = 'normal'

    if b'image widths or heights differ' in stdout:
        # do a subimage search

        try:
            stdout = subprocess.check_output('compare -metric RMSE -subimage-search %s %s %s' % (record_file, replay_file, comparison_file), stderr=subprocess.STDOUT, shell=True)
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


def compare_race(base_data, race_data, executor):
    """
    Outputs comparison
    """

    # Compare image

    cimage_meta = os.path.join(race_data['race_dir'], 'comparison.txt')

    if not os.path.isfile(cimage_meta):

        cimage = {

        }

        def finished_callback(future):

            match_type, distance = future.result()
            cimage['distance'] = distance
            cimage['match_type'] = match_type
            cimage['human'] = cimage_human(match_type, distance)

            with open(cimage_meta, 'w') as fp:
                fp.writelines([
                    str(cimage['distance']),
                    cimage['match_type']
                ])

        visual_state_match = executor.submit(generate_comparison_file,
                                             os.path.join(base_data['race_dir'], 'replay.png'),
                                             os.path.join(race_data['race_dir'], 'replay.png'),
                                             os.path.join(race_data['race_dir'], 'comparison.png'))

        visual_state_match.add_done_callback(finished_callback)

    else:

        with open(cimage_meta, 'r') as fp:
            line = fp.readline()
            match = re.compile('^(\-?[0-9]+\.?[0-9]*)([a-z]+)$').search(line)

            cimage = {
                'distance': float(match.group(1)),
                'match_type': match.group(2),
                'human': cimage_human(match.group(2), float(match.group(1)))
            }

    # Errors diff

    base_errors = base_data['errors']
    race_errors = race_data['errors']

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

    distance = sum(1 for opcode in opcodes if opcode[0] != 'equal')

    # Race and Errors Diff

    base_zip = base_data['zip_errors_schedule']
    race_zip = race_data['zip_errors_schedule']

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

    return {
        'errors_diff_count': abs(len(base_data['errors']) - len(race_data['errors'])),
        'errors_diff': opcodes,
        'errors_diff_human': opcodes_human,
        'errors_diff_distance': distance,
        'zip_diff': zip_opcodes,
        'zip_diff_human': zip_opcodes_human,
        'zip_diff_has_unequal': unequal_seen,
        'html_state_match': base_data['html_state'] == race_data['html_state'],
        'visual_state_match': cimage
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
            shutil.copy(os.path.join(input_dir, website, 'base', 'replay.png'), record_file)
    except FileNotFoundError:
        pass

    try:
        shutil.copy(os.path.join(input_dir, website, race['handle'], 'replay.png'), replay_file)
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


def init_race_index():
    return []


def append_race_index(race_index, race, base_data, race_data, comparison):
    race_index.append({
        'handle': race,
        'base_data': base_data,
        'race_data': race_data,
        'comparison': comparison
    })


def output_race_index(website, output_dir, input_dir):

    jinja = Environment(loader=PackageLoader('r5comparison', 'templates'))

    try:
        os.mkdir(output_dir)
    except OSError:
        pass  # folder exists

    for race in website['race_index']:
        output_race_report(website['handle'], race, jinja, output_dir, input_dir)

    with open(os.path.join(output_dir, 'index.html'), 'w') as fp:

        fp.write(jinja.get_template('race_index.html').render(
            website=website
        ))


def init_website_index():
    return []


def append_website_index(website_index, website, er_log, race_index):
    website_index.append({
        'handle': website,
        'race_index': race_index,
        'er_log': er_log
    })


def output_website_index(website_index, jinja, output_dir, input_dir):

    try:
        os.mkdir(output_dir)
    except OSError:
        pass  # folder exists

    website_statistics = []
    summary = {
        'race_result': {
            'success': 0,
            'timeout': 0,
            'error': 0
        },
        'execution_result': {
            'total': 0,
            'success': 0,
            'failure': 0
        },
        'execution_time': 0,
    }

    with concurrent.futures.ProcessPoolExecutor(NUM_PROC) as executor:
        for website in website_index:
            website_dir = os.path.join(output_dir, website['handle'])
            executor.submit(output_race_index, website, website_dir, input_dir)

        executor.shutdown()

    for website in website_index:

        result = {
            'success': len([race for race in website['race_index'] if race['race_data']['result'] == 'FINISHED']),
            'timeout': len([race for race in website['race_index'] if race['race_data']['result'] == 'TIMEOUT']),
            'error': len([race for race in website['race_index'] if race['race_data']['result'] == 'ERROR'])
        }

        website_statistics.append({
            'handle': website['handle'],
            'race_index': website['race_index'],
            'er_log': website['er_log'],
            'result': result
        })

        summary['race_result']['success'] += result['success']
        summary['race_result']['timeout'] += result['timeout']
        summary['race_result']['error'] += result['error']

        summary['execution_result']['total'] += website['er_log']['races_total']
        summary['execution_result']['success'] += website['er_log']['races_success']
        summary['execution_result']['failure'] += website['er_log']['races_failure']

        summary['execution_time'] += website['er_log']['execution_time']

    with open(os.path.join(output_dir, 'index.html'), 'w') as fp:

        fp.write(jinja.get_template('index.html').render(
            website_index=website_statistics,
            summary=summary
        ))


def main():

    try:
        analysis_dir = sys.argv[1]
        output_dir = sys.argv[2]
    except IndexError:
        print('Usage: %s <analysis-dir> <report-dir>' % sys.argv[0])
        sys.exit(1)

    websites = os.listdir(analysis_dir)
    website_index = init_website_index()

    with concurrent.futures.ProcessPoolExecutor(NUM_PROC) as executor:

        for website in websites:

            print ("PROCESSING", website)

            website_dir = os.path.join(analysis_dir, website)

            races = os.listdir(website_dir)
            race_index = init_race_index()

            try:
                races.remove('base')
                races.remove('record')
            except ValueError:
                print('Error, missing base or record directory in output dir for %s' % website)
                continue

            if 'runner' in races:
                races.remove('runner')

            if 'new_schedule.data' in races:
                races.remove('new_schedule.data')

            if 'stdout.txt' in races:
                races.remove('stdout.txt')

            try:
                base_data = parse_race(website_dir, 'base')

                for race in races:
                    try:
                        race_data = parse_race(website_dir, race)
                        comparison = compare_race(base_data, race_data, executor)
                        append_race_index(race_index, race, base_data, race_data, comparison)

                    except RaceParseException:
                        print("Error parsing %s :: %s" % (website, race))

                er_log = parse_er_log(website_dir)

                append_website_index(website_index, website, er_log, race_index)

            except RaceParseException:
                print("Error parsing %s :: base" % website)

        executor.shutdown()

    jinja = Environment(loader=PackageLoader('r5comparison', 'templates'))
    output_website_index(website_index, jinja, output_dir, analysis_dir)

if __name__ == '__main__':
    main()
