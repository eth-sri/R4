#!/usr/bin/env python

import sys
import os
import difflib
import re
from jinja2 import Environment, PackageLoader


class HashableDict(dict):
    def __hash__(self):
        return hash(tuple(sorted(self.items())))


def parse(base_dir, handle):
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

            module, description, length = header.split(';')
            length = int(length)

            if length == 0:
                details = ''
            else:
                details = fp.read(length)

            errors.append(HashableDict(
                module=module,
                description=description,
                details=details
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
        'html_state': state_match
    }


def compare(base_data, race_data):
    """
    Outputs comparison
    """

    # Errors diff

    base_errors = base_data['errors']
    race_errors = race_data['errors']

    diff = difflib.SequenceMatcher(None, base_errors, race_errors)
    opcodes = diff.get_opcodes()

    distance = sum(1 for opcode in opcodes if opcode[0] != 'equal')

    return {
        'errors_diff_count': abs(len(base_data['errors']) - len(race_data['errors'])),
        'errors_diff': opcodes,
        'errors_diff_distance': distance,
        'html_state_match': base_data['html_state'] == race_data['html_state']
    }


def output_report(base_data, race_data, comparison, comparison_template):
    """
    Outputs filename of written report
    """

    return ''


def init_index():
    return []


def append_index(index, report_filepath, base_data, race_data, comparison):
    index.append((report_filepath, base_data, race_data, comparison))


def output_index(index, index_template):

    with open('/tmp/index.html', 'w') as fp:
        fp.write(index_template.render(
            index=index
        ))

if __name__ == '__main__':

    jinja = Environment(loader=PackageLoader('r5comparison', 'templates'))

    try:
        output_dir = sys.argv[1]
    except IndexError:
        print('Usage: %s <EventRacer-output-dir>' % sys.argv[0])
        sys.exit(1)

    races = os.listdir(output_dir)

    try:
        races.remove('base')
    except ValueError:
        print 'Error, missing base directory in output dir'
        sys.exit(1)

    base_data = parse(output_dir, 'base')

    index = init_index()
    comparison_template = jinja.get_template('comparison.html')

    for race in races:
        race_data = parse(output_dir, race)

        comparison = compare(base_data, race_data)
        report_filepath = output_report(base_data, race_data, comparison, comparison_template)
        append_index(index, report_filepath, base_data, race_data, comparison)

    print output_index(index, jinja.get_template('index.html'))

