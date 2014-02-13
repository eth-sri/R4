#!/usr/bin/env python3

import sys
import os
import difflib
import re
import shutil
from jinja2 import Environment, PackageLoader
import subprocess
import concurrent.futures

NUM_PROC = 7


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

    errors_file = os.path.join(handle_dir, 'errors.log')
    errors = []

    with open(errors_file, 'r') as fp:
        while True:
            header = fp.readline()

            if header == '':
                break  # EOF reached

            event_action_id, module, description, length = header.split(';')
            length = int(length)

            if length == 0:
                details = ''
            else:
                details = fp.read(length)

            errors.append(HashableDict(
                event_action_id=event_action_id,
                module=module,
                description=description,
                details=details,
                _ignore_properties=['event_action_id']
            ))

    stdout_file = os.path.join(handle_dir, 'stdout.txt')

    with open(stdout_file, 'r') as fp:
        stdout = fp.read()

    result_match = re.compile('Result: ([A-Z]+)').search(stdout)
    state_match = re.compile('HTML-hash: ([0-9]+)').search(stdout)

    return {
        'handle': handle,
        'errors': errors,
        'stdout': stdout,
        'result': result_match.group(1) if result_match is not None else 'ERROR',
        'html_state': state_match is not None,
        'race_dir': handle_dir
    }


def generate_comparison_file(record_file, replay_file, comparison_file):
    subprocess.check_call('compare %s %s %s' % (record_file, replay_file, comparison_file), shell=True)


def compare_race(base_data, race_data, executor):
    """
    Outputs comparison
    """

    # Compare image

    if not os.path.isfile(os.path.join(race_data['race_dir'], 'comparison.png')):
        executor.submit(generate_comparison_file,
                        os.path.join(base_data['race_dir'], 'replay.png'),
                        os.path.join(race_data['race_dir'], 'replay.png'),
                        os.path.join(race_data['race_dir'], 'comparison.png'))

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

    return {
        'errors_diff_count': abs(len(base_data['errors']) - len(race_data['errors'])),
        'errors_diff': opcodes,
        'errors_diff_human': opcodes_human,
        'errors_diff_distance': distance,
        'html_state_match': base_data['html_state'] == race_data['html_state']
    }


def output_race_report(website, race, base_data, race_data, comparison, jinja, output_dir, input_dir):
    """
    Outputs filename of written report
    """

    record_file = os.path.join(output_dir, 'record.png')
    replay_file = os.path.join(output_dir, 'replay.png')
    comparison_file = os.path.join(output_dir, 'comparison.png')

    shutil.copy(os.path.join(input_dir, website, 'base', 'replay.png'), record_file)
    shutil.copy(os.path.join(input_dir, website, race, 'replay.png'), replay_file)
    shutil.copy(os.path.join(input_dir, website, race, 'comparison.png'), comparison_file)

    with open(os.path.join(output_dir, '%s.html' % race), 'w') as fp:

        fp.write(jinja.get_template('race.html').render(
            website=website,
            race=race,
            base_data=base_data,
            race_data=race_data,
            comparison=comparison
        ))


def init_race_index():
    return []


def append_race_index(race_index, race, base_data, race_data, comparison):
    race_index.append((race, base_data, race_data, comparison))


def output_race_index(website, race_index, jinja, output_dir, input_dir):

    try:
        os.mkdir(output_dir)
    except OSError:
        pass  # folder exists

    for race, base_data, race_data, comparison in race_index:
        output_race_report(website, race, base_data, race_data, comparison, jinja, output_dir, input_dir)

    with open(os.path.join(output_dir, 'index.html'), 'w') as fp:

        fp.write(jinja.get_template('race_index.html').render(
            website=website,
            index=race_index
        ))


def init_website_index():
    return []


def append_website_index(website_index, website, race_index):
    website_index.append((website, race_index))


def output_website_index(website_index, jinja, output_dir, input_dir):

    try:
        os.mkdir(output_dir)
    except OSError:
        pass  # folder exists

    website_statistics = []

    for website, race_index in website_index:

        website_dir = os.path.join(output_dir, website)
        output_race_index(website, race_index, jinja, website_dir, input_dir)

        website_statistics.append({
            'website': website,
        })

    with open(os.path.join(output_dir, 'index.html'), 'w') as fp:

        fp.write(jinja.get_template('index.html').render(
            website_index=website_statistics
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

            website_dir = os.path.join(analysis_dir, website)

            races = os.listdir(website_dir)
            race_index = init_race_index()

            try:
                races.remove('base')
                races.remove('record')
            except ValueError:
                print('Error, missing base directory in output dir')
                sys.exit(1)

            base_data = parse_race(website_dir, 'base')

            for race in races:
                race_data = parse_race(website_dir, race)
                comparison = compare_race(base_data, race_data, executor)
                append_race_index(race_index, race, base_data, race_data, comparison)

            append_website_index(website_index, website, race_index)

        executor.shutdown()

    jinja = Environment(loader=PackageLoader('r5comparison', 'templates'))
    print(output_website_index(website_index, jinja, output_dir, analysis_dir))

if __name__ == '__main__':
    main()
