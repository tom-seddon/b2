#!/usr/bin/python3
import sys,argparse

##########################################################################
##########################################################################

# Several of the b2 test programs can run a number of tests, so that
# there aren't hundreds of test programs being compiled and so that
# configuring the parallelism can be done via tweaking the ctest
# setup.
#
# To check every possible test got run:
#
# 1. have such tests print all the possible tests they can run run to
# stdout, either as a byproduct of running tests or as a separate
# dummy test. List the tests one per line, starting each line with
# TEST_AVAILABLE_PREFIX, below, then the test name.
#
# 2. have such tests print the tests they actually run to stdout.
# Start each line with TEST_RUN_PREFIX, below, then the test name.
# Print this once for each run of a test.
#
# This output will end up in the ctest LastTest.log file.
#
# check_ctest_log will scan the file, find every available test,
# cross-reference against the tests actually run to find tests that
# weren't run exactly once, or tests that weren't listed in the
# available list.

##########################################################################
##########################################################################

TEST_AVAILABLE_PREFIX="2fcf9707-9498-4a03-9b27-ef501fa2fbb6:"
TEST_AVAILABLE_BUT_HIDDEN_PREFIX="1902bf7f-8607-4cd3-a6e1-abaf2ebe85c9:"
TEST_RUN_PREFIX="ea73a8dc-2d1a-43bc-ae41-078e441e53c5:"

##########################################################################
##########################################################################

def main2(options):
    tests_available=set()
    tests_available_but_hidden=set()
    tests_run={}

    with open(options.input_path,'rt') as f:
        for line in f.readlines():
            if line.startswith(TEST_AVAILABLE_PREFIX):
                tests_available.add(line[len(TEST_AVAILABLE_PREFIX):].strip())
            elif line.startswith(TEST_AVAILABLE_BUT_HIDDEN_PREFIX):
                tests_available_but_hidden.add(line[len(TEST_AVAILABLE_BUT_HIDDEN_PREFIX):].strip())
            elif line.startswith(TEST_RUN_PREFIX):
                name=line[len(TEST_RUN_PREFIX):].strip()
                tests_run[name]=tests_run.get(name,0)+1

    for name in tests_available:
        if name not in tests_run: tests_run[name]=0

    good=True
    for name,count in tests_run.items():
        if name in tests_available_but_hidden: bad_count=count>1
        else: bad_count=count!=1
        
        if bad_count:
            sys.stderr.write('FATAL: test ran %d times: %s\n'%(count,name))
            good=False

        if name in tests_available_but_hidden: pass
        elif name not in tests_available:
            sys.stderr.write('FATAL: unknown test ran %d times: %s\n'%(count,name))
            good=False

    if not good: sys.exit(1)

def main(argv):
    parser=argparse.ArgumentParser()
    parser.add_argument('input_path',metavar='FILE',help='''read LastTest.log from %(metavar)s''')

    main2(parser.parse_args(argv))

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])
