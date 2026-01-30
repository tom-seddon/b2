#!/usr/bin/python3
import sys,os,argparse,time

##########################################################################
##########################################################################

g_verbose=False

def pv(x):
    if g_verbose:
        sys.stderr.write(x)
        sys.stderr.flush()

##########################################################################
##########################################################################

def fatal(message):
    sys.stderr.write('FATAL: %s\n'%message)
    sys.exit(1)

##########################################################################
##########################################################################

def init_cmd(options):
    with open(options.g_path,'wt') as f: pass

def push_cmd(options):
    with open(options.g_path,'at') as f:
        f.write('push\n%s\n%s\n%d\n'%(options.key,
                                      options.value,
                                      time.time_ns()))

def pop_cmd(options):
    with open(options.g_path,'at') as f:
        f.write('pop\n%s\n%d\n'%(options.key or '',
                                 time.time_ns()))


def print_cmd(options):
    import collections
    
    with open(options.g_path,'rt') as f:
        lines=[line.rstrip() for line in f.readlines()]

    class Entry:
        def __init__(self,key,value,start):
            self.key=key
            self.value=value
            self.start=start
            self.end=None
            self.any_children=False

    Row=collections.namedtuple('Row','values seconds')

    headers=options.column_names[:]
    rows=[]
    stack=[]
    line_index=0
    first_push_start=None
    last_pop_start=None
    while line_index<len(lines):
        if lines[line_index]=='push':
            if len(stack)>0: stack[-1].any_children=True

            key=lines[line_index+1]
            if key not in headers: headers.append(key)

            start=int(lines[line_index+3])
            stack.append(Entry(key=key,
                               value=lines[line_index+2],
                               start=start))
            if first_push_start is None: first_push_start=start
            line_index+=4
        elif lines[line_index]=='pop':
            key=lines[line_index+1]
            if len(stack)==0: fatal('mismatched push/pop')
            if key!='' and key!=stack[-1].key:
                fatal('mismatch: pushed %s, popped %s'%(stack[-1].key,key))

            start=int(lines[line_index+2])
            if not stack[-1].any_children:
                row=Row(seconds=(start-stack[-1].start)/1e9,values={})
                for entry in stack: row.values[entry.key]=entry.value
                rows.append(row)

            del stack[-1]

            last_pop_start=start

            line_index+=3
        else:
            fatal('%s:%d: unrecognised'%(options.g_path,line_index+1))

    rows_strs=[]
    for row in rows:
        row_strs=[row.values.get(header,'') for header in headers]
        row_strs.append('%.3f'%row.seconds)
        rows_strs.append(row_strs)

    headers.append('Time (s)')

    column_widths=[len(header) for header in headers]
    for row_strs in rows_strs:
        for i,s in enumerate(row_strs):
            column_widths[i]=max(column_widths[i],len(s))

    for name in options.sort_columns:
        if name not in headers: fatal('unknown sort column: %s'%name)
        index=headers.index(name)
        rows_strs.sort(key=lambda x:x[index])

    def get_row_string(row):
        s=''
        for i in range(len(row)): s+='| %-*s '%(column_widths[i],row[i])
        s+='|'
        return s

    print()
    print(get_row_string(headers))
    print('|%s|'%('|'.join(['-'*(2+n) for n in column_widths])))
    for row_strs in rows_strs: print(get_row_string(row_strs))
    print()
    if first_push_start is not None and last_pop_start is not None: 
        print('Total time: %.1f seconds'%((last_pop_start-first_push_start)/1e9))

##########################################################################
##########################################################################

ENV_VAR_NAME='TIME_JOBS_FILE'

def main(argv):
    parser=argparse.ArgumentParser()
    parser.add_argument('-v','--verbose',action='store_true',dest='g_verbose',help='''be more verbose''')

    default_path=os.getenv(ENV_VAR_NAME)
    parser.add_argument('-f','--file',dest='g_path',metavar='PATH',default=default_path,help='''store state in %(metavar)s. Will use value of env var '''+ENV_VAR_NAME+''' as default.'''+('' if default_path is None else ''' Default: %(default)s'''))
    parser.set_defaults(fun=None)

    subparsers=parser.add_subparsers()

    def add_subparser(fun,*args,**kwargs):
        subparser=subparsers.add_parser(*args,**kwargs)
        subparser.set_defaults(fun=fun)
        return subparser

    init_parser=add_subparser(init_cmd,'init',help='''initialise timing process''')

    push_parser=add_subparser(push_cmd,'push',help='''push new key''')
    push_parser.add_argument('key',metavar='KEY',help='''push key named %(metavar)s''')
    push_parser.add_argument('value',metavar='VALUE',help='''value for key pushed''')

    pop_parser=add_subparser(pop_cmd,'pop',help='''pop key''')
    pop_parser.add_argument('-k','--key',metavar='KEY',help='''supply key's name. Will be checked against pushed name if supplied''')

    print_parser=add_subparser(print_cmd,'print',help='''print results''')
    print_parser.add_argument('column_names',metavar='NAME',nargs='*',help='''specify column name order''')
    print_parser.add_argument('-s','--sort',metavar='NAME',action='append',dest='sort_columns',default=[],help='''sort on column named %(metavar)s. Can specify multiple times''')

    options=parser.parse_args(argv)
    if options.fun is None:
        parser.print_help()
        sys.exit(1)

    global g_verbose
    g_verbose=options.g_verbose

    if options.g_path is None: fatal('must specify state file')

    options.fun(options)

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])
