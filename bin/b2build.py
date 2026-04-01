#!/usr/bin/python3
import sys,os,os.path,argparse,subprocess,shutil,shlex,collections

##########################################################################
##########################################################################

b2build_py_basename=os.path.basename(__file__)

##########################################################################
##########################################################################

def fatal(msg):
    sys.stderr.write('FATAL: %s\n'%msg)
    sys.exit(1)

##########################################################################
##########################################################################

g_verbose=False

def pv(msg):
    if g_verbose:
        sys.stdout.write(msg)
        sys.stdout.flush()

##########################################################################
##########################################################################

class ChangeDirectory:
    def __init__(self,path):
        self._oldcwd=os.getcwd()
        self._newcwd=path

    def __enter__(self): os.chdir(self._newcwd)

    def __exit__(self,*args): os.chdir(self._oldcwd)

##########################################################################
##########################################################################

def makedirs(path):
    if not os.path.isdir(path): os.makedirs(path)

def rmtree(path):
    if os.path.isdir(path): shutil.rmtree(path)

##########################################################################
##########################################################################

def is_macos(): return sys.platform=='darwin'

def is_windows(): return sys.platform=='osx'

def is_linux(): return sys.platform=='linux'

def is_unix(): return is_macos() or is_linux()

##########################################################################
##########################################################################

def get_copyable_argv(argv):
    assert isinstance(argv,list),type(argv)

    def quote(x):
        assert isinstance(x,str),type(x)
    
        if is_windows():
            if not x.startswith('"') and ' ' in x: return '"%s"'%x
            else: return x
        else: return shlex.quote(x)
        
    return ' '.join([quote(arg) for arg in argv])

def run_subprocess(argv,options,**other_popen_kwargs):
    if g_verbose:
        print('b2build (cwd: %s) running: %s'%(os.getcwd(),get_copyable_argv(argv)))
                                               

    process=subprocess.Popen(argv,**other_popen_kwargs)
    process.wait()
    return process

##########################################################################
##########################################################################

def get_make_path(options):
    if is_unix(): return options.g_make_path
    else: return os.path.join(options.g_working_copy_path,'bin/snmake.exe')

##########################################################################
##########################################################################

def get_max_jobs_args_for_make(options):
    if os.getenv('MAKEFLAGS') is not None:
        if not options.g_ignore_submake:
            # Probably running as part of a submake, so don't specify -j.
            pv(f'''b2build: MAKEFLAGS detected. Assuming running from Make. Running make without -j {options.g_max_jobs}\n''')
            return []
        
    return ['-j',str(options.g_max_jobs)]

##########################################################################
##########################################################################

def get_build_folder_path(name,options):
    return os.path.join(options.g_working_copy_path,
                        'build',
                        '%s%s'%(options.prefix or '',name))

##########################################################################
##########################################################################

def get_cmake_defines(options):
    defines=[]
    
    if is_macos():
        if options.osx_deployment_target is not None:
            defines.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=%s'%options.osx_deployment_target)

        # new requirement for CMake 4.x.
        defines.append('-DCMAKE_OSX_SYSROOT=macosx')

    if options.name is not None:
        defines.append('-DRELEASE_NAME=%s'%options.name)

    return defines

##########################################################################
##########################################################################

CMAKE_BUILD_TYPES={
    'd':'Debug',
    'r':'RelWithDebInfo',
    'f':'Final',
}

##########################################################################
##########################################################################

Sanitizer=collections.namedtuple('Sanitizer','friendly_name cmake_name')

UNIX_SANITIZER_TYPES={
    'u':Sanitizer(friendly_name='undefined',cmake_name='UNDEFINED'),
    't':Sanitizer(friendly_name='thread',cmake_name='THREAD'),
    'm':Sanitizer(friendly_name='memory',cmake_name='MEMORY'),
    'a':Sanitizer(friendly_name='address',cmake_name='ADDRESS'),
}

##########################################################################
##########################################################################

def init_cmd(options):
    if options.unix and not is_unix():
        fatal('''Can build Unix-style on Unix only''')

    if options.xcode and not is_macos():
        fatal('''Can build with Xcode on macOS only''')

    if options.vs2022 and not is_windows():
        fatal('''Can build with Visual Studio on Windows only''')

    if (options.cc is not None)!=(options.cxx is not None):
        fatal('must specify neither or both of C/C++ compilers')
        
    build_folder=get_build_folder_path('',options)

    makefile_basename='Makefile.init.mak'
    
    makedirs(build_folder)
    makefile_path=os.path.join(build_folder,makefile_basename)

    def get_optional_option(option,value):
        if value is None: return ''
        else: return ' %s "%s"'%(option,value)

    global_options=''
    global_options+=' $(if $(VERBOSE),--verbose,)'
    global_options+=' --working-copy "%s"'%(os.path.relpath(options.g_working_copy_path,build_folder))

    cmd_options=''
    cmd_options+=get_optional_option('--prefix',options.prefix)
    cmd_options+=get_optional_option('--name',options.name)
    if is_macos():
        cmd_options+=get_optional_option('--osx-deployment-target',options.osx_deployment_target)

    b2build_py_path=os.path.join(options.g_working_copy_path,'bin',b2build_py_basename)
    b2build_py_path=os.path.relpath(b2build_py_path,build_folder)
    
    with open(makefile_path,'wt') as f:
        f.write('''_V:=$(if $(VERBOSE),,@)\n''')
        f.write(f'''PYTHON:={sys.executable}\n''')
        f.write('''.PHONY:default\ndefault:\n\t$(error Must specify target)\n''')

        targets=[]
        def target(name):
            assert ' ' not in name,name
            f.write(f'''.PHONY:{name}\n''')
            f.write(f'''{name}:\n''')
            targets.append(name)

        if options.unix:
            for build in CMAKE_BUILD_TYPES.keys():
                def write(sanitizer):
                    target('init_unix_%s%s'%(build,sanitizer or ''))

                    if sanitizer is None: cmd_prefix=''
                    else:
                        # It's normal for not all sanitizers to be
                        # available. Try all of them, and ignore
                        # failures. If there's anything actually wrong
                        # with the build, it'll show up in one of the
                        # non-sanitizer cases.
                        cmd_prefix='-'

                    f.write(f'''\t$(_V){cmd_prefix}"$(PYTHON)" "{b2build_py_path}"{global_options} _init_unix{cmd_options} {get_optional_option('--sanitizer',sanitizer)}{get_optional_option('--cc',options.cc)}{get_optional_option('--cxx',options.cxx)} {build}\n''')

                write(None)
                if options.enable_sanitizers:
                    for sanitizer in UNIX_SANITIZER_TYPES.keys():
                        write(sanitizer)

        if options.xcode:
            target('init_xcode')
            f.write(f'''\t$(_V)"$(PYTHON)" "{b2build_py_path}"{global_options} _init_xcode{cmd_options}\n''')

        f.write(f'''.PHONY:all\n''')
        f.write(f'''all:{' '.join(targets)}\n''')

    with ChangeDirectory(build_folder):
        argv=[get_make_path(options)]
        argv+=get_max_jobs_args_for_make(options)
        argv+=['-f',makefile_basename]
        argv+=['all']
        if options.g_verbose: argv+=['VERBOSE=1']
        
        ret=run_subprocess(argv,options,close_fds=False)
        if ret.returncode!=0: fatal('make failed with exit code: %d'%ret.returncode)

    print('''Init completed successfully. (It's normal for some errors and warnings to be printed during the process. If you can see this message, it finished successfully and nothing unexpected happened.)''')

##########################################################################
##########################################################################

def _init_xcode_cmd(options):
    xcode_folder=get_build_folder_path('Xcode',options)

    rmtree(xcode_folder)
    makedirs(xcode_folder)

    with ChangeDirectory(xcode_folder):
        argv=['cmake','-G','Xcode']
        argv+=get_cmake_defines(options)
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    xcode_folder)]
        argv+=['-B','.']
        ret=run_subprocess(argv,options,check=True,close_fds=False)

##########################################################################
##########################################################################

def _init_unix_cmd(options):
    if (options.cc is not None)!=(options.cxx is not None):
        fatal('must specify neither or both of C/C++ compilers')
    
    cmake_build_type=CMAKE_BUILD_TYPES.get(options.build)
    if cmake_build_type is None:
        fatal('unknown build type: %s'%options.build)

    if options.sanitizer is None: sanitizer=None
    else:
        sanitizer=UNIX_SANITIZER_TYPES.get(options.sanitizer)
        if sanitizer is None:
            fatal('unknown sanitizer type: %s'%options.sanitizer)

    if is_linux(): os_name='linux'
    elif is_macos(): os_name='osx'
    else: fatal('unexpected OS type')

    unix_folder_name=options.build
    if options.sanitizer is not None: unix_folder_name+=options.sanitizer
    unix_folder_name+='.'+os_name
    
    unix_folder=get_build_folder_path(unix_folder_name,options)

    rmtree(unix_folder)
    makedirs(unix_folder)

    # bit ugly, but all the process does is run cmake then quit, so
    # it's not a massive problem having these settings lie around
    # afterwards.
    if options.cc is not None: os.putenv('CC',options.cc)
    if options.cxx is not None: os.putenv('CXX',options.cxx)

    # TODO: might be nice to have the build system configurable?
    with ChangeDirectory(unix_folder):
        argv=['cmake','-G','Ninja']
        argv+=get_cmake_defines(options)
        if options.sanitizer is not None:
            argv+=['-DSANITIZE_%s=On'%sanitizer.cmake_name]
        argv+=['-DCMAKE_BUILD_TYPE=%s'%cmake_build_type]
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    unix_folder)]
        argv+=['-B','.']
        ret=run_subprocess(argv,options,close_fds=False)
        if ret.returncode!=0:
            rmtree(unix_folder)
            fatal('init failed')

##########################################################################
##########################################################################

def print_build_suffix_cmd(options):
    run_subprocess(['git','log','-1','--format=%cd-%h','--date=format:%Y%m%d-%H%M%S'],options,check=True)

##########################################################################
##########################################################################
    
def print_build_timestamp_cmd(options):
    run_subprocess(['git','log','-1','--format=%cd','--date=format:%Y%m%d-%H%M%S'],options,check=True)

##########################################################################
##########################################################################

def main(argv):
    def auto_int(x): return int(x,0)

    default_osx_deployment_target=None
    if is_macos():
        try: default_osx_deployment_target=subprocess.check_output(['sw_vers','-productVersion'],text='utf-8').rstrip()
        except CalledProcessError: pass
    
    parser=argparse.ArgumentParser(description='''b2 build automation tool''',
                                   epilog='''Commands with names starting with _ are for internal use''')
    parser.set_defaults(fun=None)

    parser.add_argument('-v','--verbose',dest='g_verbose',action='store_true',help='''be more verbose''')
    parser.add_argument('-j',type=auto_int,metavar='N',dest='g_max_jobs',default=os.cpu_count(),help='''run up to %(metavar)s job(s) at once. Default: %(default)s''')
    parser.add_argument('-w','--working-copy',dest='g_working_copy_path',metavar='PATH',default='.',help='''specify root of b2 working copy. Default: %(default)s''')
    parser.add_argument('--make',dest='g_make_path',default='make',metavar='PATH',help='''use %(metavar)s as path to GNU Make on Linux/macOS. (On Windows, the repo's copy is always used.) Default: %(default)s''')
    parser.add_argument('--ignore-submake',dest='g_ignore_submake',action='store_true',help='''if run from GNU Make, don't do anything special. If running further copies of GNU Make, still pass them -j''')

    subparsers=parser.add_subparsers()

    def add_subparser(name,fun,**kwargs):
        subparser=subparsers.add_parser(name,**kwargs)
        subparser.set_defaults(fun=fun)
        return subparser

    def add_common_init_options(subparser):
        subparser.add_argument('--prefix',metavar='STRING',help='''prepend %(metavar)s to name of any build folder created''')
        subparser.add_argument('--name',metavar='STRING',help='''use %(metavar)s as the build name''')
        subparser.add_argument('--osx-deployment-target',metavar='TARGET',default=default_osx_deployment_target,help='''specify macOS deployment target.'''+('' if default_osx_deployment_target is None else ' Default: %s'%default_osx_deployment_target))

    init_subparser=add_subparser('init',init_cmd,help='''initialise build''')
    init_subparser.add_argument('--unix',action='store_true',help='''initialise Unix-style build''')
    init_subparser.add_argument('--vs2022',action='store_true',help='''initialise VS2022 build''')
    init_subparser.add_argument('--xcode',action='store_true',help='''initialise Xcode build''')
    init_subparser.add_argument('--enable-sanitizers',action='store_true',help='''if building Unix-style, try to use any supported sanitizers''')
    init_subparser.add_argument('--cc',metavar='NAME',help='''if building Unix-style, use %(metavar) as C compiler''')
    init_subparser.add_argument('--cxx',metavar='NAME',help='''if building Unix-style, use %(metavar)s as C++ compiler''')
    add_common_init_options(init_subparser)

    _init_xcode_subparser=add_subparser('_init_xcode',_init_xcode_cmd,help='''initialise Xcode build''')
    add_common_init_options(_init_xcode_subparser)

    _init_unix_subparser=add_subparser('_init_unix',_init_unix_cmd,help='''initialise Unix build''')
    add_common_init_options(_init_unix_subparser)
    _init_unix_subparser.add_argument('--sanitizer',help='''specify sanitizer: '''+'; '.join(['%s (%s)'%(k,v.friendly_name) for k,v in UNIX_SANITIZER_TYPES.items()]))
    _init_unix_subparser.add_argument('build',help='''specify build configuration: '''+'; '.join(['%s (%s)'%(k,v) for k,v in CMAKE_BUILD_TYPES.items()]))
    _init_unix_subparser.add_argument('--keep',action='store_true',help='''don't delete build folder if init fails''')
    _init_unix_subparser.add_argument('--cc',metavar='NAME',help='''use %(metavar) as C compiler''')
    _init_unix_subparser.add_argument('--cxx',metavar='NAME',help='''use %(metavar)s as C++ compiler''')

    print_build_suffix_subparser=add_subparser('print-build-suffix',print_build_suffix_cmd,help='''print build suffix: time, date and hash of head commit''')

    print_build_timestamp_parser=add_subparser('print-build-timestamp',print_build_timestamp_cmd,help='''print build timestamp: time and date of head commit''')

    options=parser.parse_args(argv)
    if options.fun is None:
        parser.print_help()
        sys.exit(1)

    src_b2_path=os.path.join(options.g_working_copy_path,'src/b2')
    if not os.path.isdir(src_b2_path):
        fatal('"%s" folder not found. Is the working copy path correct?'%src_b2_path)

    global g_verbose
    g_verbose=options.g_verbose

    options.fun(options)

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])
