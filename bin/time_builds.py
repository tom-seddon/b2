#!/usr/bin/python3
import sys,os,os.path,argparse,sqlite3,glob,contextlib,subprocess,shlex,collections,multiprocessing,time,re

##########################################################################
##########################################################################

b2_URL='https://github.com/tom-seddon/b2'
DB_PATH='b2_timings.db'

##########################################################################
##########################################################################

BuildType=collections.namedtuple('BuildType','folder name')

BUILD_TYPES=[
    BuildType(folder='d',name='Debug'),
    BuildType(folder='r',name='RelWithDebInfo'),
    BuildType(folder='f',name='Final'),
]

##########################################################################
##########################################################################

def fatal(msg):
    sys.stderr.write('FATAL: %s\n'%msg)
    sys.exit(1)

##########################################################################
##########################################################################

@contextlib.contextmanager
def working_folder(path):
    old_folder=os.getcwd()
    try:
        os.chdir(path)
        yield None
    finally:
        os.chdir(old_folder)

##########################################################################
##########################################################################

g_verbose=False

def pv(msg):
    if g_verbose:
        sys.stdout.write(msg)
        sys.stdout.flush()

##########################################################################
##########################################################################

def get_command_line(argv):
    line=''
    for arg in argv:
        if os.name=='posix': arg=shlex.quote(arg)
        elif ' ' in arg and not arg.startswith('"'): arg='"%s"'%arg

        if len(line)>0: line+=' '
        line+=arg

    return line

##########################################################################
##########################################################################

def run_subprocess(argv,*args,**kwargs):
    if g_verbose: pv('Running: %s\n'%get_command_line(argv))
            
    try: result=subprocess.run(argv,*args,**kwargs)
    except subprocess.CalledProcessError as e:
        if e.stderr is not None:
            sys.stderr.write(e.stderr)
        fatal('failed with code %d: %s'%(e.returncode,
                                         get_command_line(argv)))
    return result
        
##########################################################################
##########################################################################

def get_db_path(options): return os.path.join(options.g_output_path,DB_PATH)

def get_b2_working_copy_path(options):
    return os.path.join(options.g_output_path,'b2')
    
##########################################################################
##########################################################################

def init_cmd(options):
    if not os.path.isdir(options.g_output_path):
        fatal('not a folder: %s'%options.g_output_path)

    if not options.force:
        if len(glob.glob(os.path.join(options.g_output_path,'*')))>0:
            fatal('folder not empty: %s'%options.g_output_path)

    b2_working_copy_path=get_b2_working_copy_path(options)
    if not os.path.isdir(b2_working_copy_path):
        argv=['git','clone',b2_URL,b2_working_copy_path]
        run_subprocess(argv,check=True)

    db_path=get_db_path(options)

    if options.force:
        if os.path.isfile(db_path): os.unlink(db_path)

    dbconn=sqlite3.connect(db_path)

    dbcur=dbconn.cursor()
    dbcur.execute('CREATE TABLE timings(hash,date,compiler,configuration,status,build_time,test_time)')

    # get list of commits.
    num_commits=0
    num_builds=0
    with working_folder(b2_working_copy_path):
        argv=['git',
              'rev-list',
              '--ancestry-path','%s..%s'%(options.older_commit,
                                          options.newer_commit)]
        result=run_subprocess(argv,capture_output=True,check=True,encoding='utf-8')
        commits=[line.strip() for line in result.stdout.strip().splitlines()]
        if len(commits)==0: fatal('no commits found')

        for commit in commits:
            # %aI = ISO8601 format
            argv=['git','log',commit,'-1','--format=%aI']
            result=run_subprocess(argv,capture_output=True,check=True,encoding='utf-8')
            date=result.stdout.strip()

            for build_type in BUILD_TYPES:
                dbcur.execute('''INSERT INTO timings VALUES(?,?,"Default",?,NULL,NULL,NULL)''',(commit,date,build_type.name))
                num_builds+=1

            num_commits+=1

        dbconn.commit()

    print('%d builds in %d commits'%(num_builds,num_commits))

##########################################################################
##########################################################################

def run_cmd(options):
    dbconn=sqlite3.connect(get_db_path(options))

    dbcur=dbconn.cursor()

    # this doesn't persist from run to run, but that's fine. It just
    # needs to save a bit of time when doing the run in sequence.
    prepared_hash=None

    rows=[row for row in dbcur.execute('''SELECT hash,date,configuration from timings WHERE status IS NULL''')]
    if len(rows)==0: fatal('nothing left to do')

    # ensure it's ordered by date
    rows.sort(key=lambda x:x[1],reverse=True)

    if sys.platform=='darwin': os_name='osx'
    else:
        fatal('unknown platform: sys.platform=%s; os.name=%s'%(sys.platform,os.name))

    for index,row in enumerate(rows):
        hash,date,configuration=row
        
        build_type=None
        for t in BUILD_TYPES:
            if t.name==configuration:
                build_type=t
                break
        assert build_type is not None,configuration

        status=None
        build_time=None
        test_time=None

        build_folder=os.path.join(get_b2_working_copy_path(options),
                                  'build',
                                  '%s.%s'%(build_type.folder,os_name))
        
        def prepare():
            with working_folder(get_b2_working_copy_path(options)):
                argv=['git','checkout',hash]
                result=run_subprocess(argv)
                if result.returncode!=0: return 'checkout_failed'

                argv=['git','submodule','update','--init','--recursive']
                result=run_subprocess(argv)
                if result.returncode!=0: return 'submodules failed'

                argv=['make',
                      '_unix',
                      'SANITIZER=',
                      'SUFFIX=',
                      '-j',str(options.num_jobs)]
                result=run_subprocess(argv)
                if result.returncode!=0: return 'init failed'

                return None

        def compile():
            nonlocal build_time
            
            with working_folder(build_folder):
                build_time_start=time.time_ns()
                argv=['ninja','-j',str(options.num_jobs)]
                result=run_subprocess(argv)
                if result.returncode!=0: return 'build failed'
                build_time=(time.time_ns()-build_time_start)/1e9

                return None

        def test():
            nonlocal test_time
            
            with working_folder(build_folder):
                test_time_start=time.time_ns()
                argv=['ctest','--progress','-j',str(options.num_jobs)]
                result=run_subprocess(argv)
                if result.returncode!=0: return 'test failed'
                test_time=(time.time_ns()-test_time_start)/1e9

                return None

        if hash!=prepared_hash:
            status=prepare()
            if status is None: prepared_hash=hash

        print('building %d/%d: %s %s'%(1+index,len(rows),hash,configuration))
            
        if status is None: status=compile()

        if status is None: status=test()

        if status is None: status='ok'

        dbcur.execute('UPDATE timings SET status=?,build_time=?,test_time=? WHERE hash=? AND configuration=?',(status,build_time,test_time,hash,configuration))
        dbconn.commit()
            
##########################################################################
##########################################################################

def export_csv(rows,options):
    pass

EXPORT_TYPES={
    'csv':export_csv,
}
DEFAULT_EXPORT_TYPE='csv'
assert DEFAULT_EXPORT_TYPE in EXPORT_TYPES

def export_cmd(options):
    if options.export_type not in EXPORT_TYPES:
        fatal('unknown export type: %s'%options.export_type)
        
    dbconn=sqlite3.connect(get_db_path(options))

    dbcur=dbconn.cursor()

    rows=[row for row in dbcur.execute('''SELECT hash,date,compiler,configuration,build_time,test_time FROM timings WHERE status="ok"''')]
    if len(rows)==0: fatal('no results to export')

    fatal('TODO...')


##########################################################################
##########################################################################

def auto_int(x): return int(x,0)

def main(argv):
    parser=argparse.ArgumentParser()

    parser.add_argument('-v','--verbose',dest='g_verbose',action='store_true',help='''be more verbose''')
    parser.add_argument('-o','--output',default='.',dest='g_output_path',metavar='FOLDER',help='''use %(default)s as output path. Default: %(default)s''')
    parser.set_defaults(fun=None)

    subparsers=parser.add_subparsers()

    init=subparsers.add_parser('init',help='''initialise timing run''')
    init.add_argument('--force',action='store_true',help='''proceed even if folder not empty. Will reuse previous b2 clone but recreate db''')
    init.add_argument('older_commit',metavar='COMMIT',help='''older commit''')
    init.add_argument('newer_commit',metavar='COMMIT',help='''newer commit''')
    init.set_defaults(fun=init_cmd)

    run=subparsers.add_parser('run',help='''continue timing run''')
    run.add_argument('-j',type=auto_int,dest='num_jobs',metavar='N',help='''run up to %(metavar)s job(s) simultaneously''')
    run.set_defaults(fun=run_cmd)

    export=subparsers.add_parser('export',help='''export data''')
    export.add_argument('-o',metavar='FILE',dest='output_path',help='''write data to %(metavar)s (specify - for stdout)''')
    export.add_argument('-t',metavar='TYPE',default=DEFAULT_EXPORT_TYPE,dest='export_type',help='''export format (one of: '''+'; '.join(sorted(EXPORT_TYPES.keys()))+'''). Default: %(default)ss''')
    export.set_defaults(fun=export_cmd)

    options=parser.parse_args(argv)
    if options.fun is None:
        parser.print_help()
        sys.exit(1)

    global g_verbose
    g_verbose=options.g_verbose

    options.fun(options)
    
##########################################################################
##########################################################################
    
if __name__=='__main__': main(sys.argv[1:])
