#!/usr/bin/python3
import sys,os,os.path,argparse,subprocess

##########################################################################
##########################################################################

# Eventually this will cover the functionality of release.py as well.

##########################################################################
##########################################################################

g_verbose=False

def pv(msg):
    if g_verbose:
        sys.stdout.write(msg)
        sys.stdout.flush()

##########################################################################
##########################################################################

def print_suffix_cmd(options):
    subprocess.run(['git','log','-1','--format=%cd-%h','--date=format:%Y%m%d-%H%M%S'],check=True)

def print_timestamp_cmd(options):
    subprocess.run(['git','log','-1','--format=%cd','--date=format:%Y%m%d-%H%M%S'],check=True)
    
##########################################################################
##########################################################################

def main(argv):
    parser=argparse.ArgumentParser()
    parser.add_argument('-v','--verbose',dest='g_verbose',action='store_true',help='''be more verbose''')
    parser.set_defaults(fun=None)
    subparsers=parser.add_subparsers()

    def add_subparser(name,fun,**kwargs):
        subparser=subparsers.add_parser(name,**kwargs)
        subparser.set_defaults(fun=fun)
        return subparser

    print_suffix_parser=add_subparser('print-suffix',print_suffix_cmd,help='''print build suffix: time, date and hash of head commit''')
    print_timestamp_parser=add_subparser('print-timestamp',print_timestamp_cmd,help='''print buidl timestamp: time and date of head commit''')

    options=parser.parse_args(argv[1:])
    if options.fun is None:
        parser.print_help()
        sys.exit(1)

    global g_verbose;g_verbose=options.g_verbose
    options.fun(options)

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv)
