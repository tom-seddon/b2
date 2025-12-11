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
        f.write('push\n%s\n%d\n'%(options.name,
                                  time.time_ns()))

def pop_cmd(options):
    with open(options.g_path,'at') as f:
        f.write('pop\n%s\n%d\n'%(options.name or '',
                                 time.time_ns()))

def print_cmd(options):
    import collections
    
    with open(options.g_path,'rt') as f:
        lines=[line.rstrip() for line in f.readlines()]

    class Entry:
        def __init__(self,name,start):
            self.name=name
            self.start=start
            self.any_children=False

    rows=[]
    stack=[]
    line_index=0
    while line_index<len(lines):
        if lines[line_index]=='push':
            if len(stack)>0: stack[-1].any_children=True
            
            stack.append(Entry(name=lines[line_index+1],
                               start=int(lines[line_index+2])))
            line_index+=3
        elif lines[line_index]=='pop':
            name=lines[line_index+1]
            if len(stack)==0: fatal('mismatched push/pop')
            if name!='' and name!=stack[-1].name:
                fatal('mismatched push %s/pop %s'%(stack[-1].name,name))

            if not stack[-1].any_children:
                seconds=(int(lines[line_index+2])-stack[-1].start)/1e9

                row=[]
                for entry in stack: row.append(entry.name)
                row.append('%.3f'%seconds)

                rows.append(row)

            del stack[-1]

            line_index+=3
        else:
            fatal('%s:%d: unrecognised'%(options.g_path,line_index+1))

    headers=[]
    for i in range(len(rows[0])-1):
        if i<len(options.column_names):
            headers.append(options.column_names[i])
        else: headers.append('')
    headers.append('Time (s)')
        
    column_widths=[len(header) for header in headers]
    for row in rows:
        assert len(row)==len(column_widths)
        for i,column in enumerate(row):
            column_widths[i]=max(column_widths[i],len(column))

    def get_row_string(row):
        s=''
        for i in range(len(row)): s+='| %-*s '%(column_widths[i],row[i])
        s+='|'
        return s

    print()
    print(get_row_string(headers))
    print('|%s|'%('|'.join(['-'*(2+n) for n in column_widths])))
    for row in rows: print(get_row_string(row))
    print()

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

    push_parser=add_subparser(push_cmd,'push',help='''push new job''')
    push_parser.add_argument('name',metavar='NAME',help='''use %(metavar)s as job's name''')

    pop_parser=add_subparser(pop_cmd,'pop',help='''finish job''')
    pop_parser.add_argument('-n','--name',metavar='NAME',help='''supply job's name. Will be checked against pushed name if supplied''')

    print_parser=add_subparser(print_cmd,'print',help='''print results''')
    print_parser.add_argument('column_names',metavar='NAME',nargs='*',help='''specify column names''')

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
