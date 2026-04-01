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
                        'build'
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

class Target:
    def __init__(self,name):
        assert ' ' not in name,name
        self.name=name
        self.lines=[]
        self.dependencies=[]

    def add_dependency(self,dependency):
        assert isinstance(dependency,Target),type(dependency)
        assert dependency not in self.dependencies
        self.dependencies.append(dependency)
        
    def add_line(self,line):
        assert isinstance(line,str),type(line)
        self.lines.append(line)

class Makefile:
    def __init__(self):
        self._targets=[]

    def add_named_target(self,name):
        target=Target(name)
        self._targets.append(target)
        return target

    def _check(self):
        for target in self._targets:
            for dependency in target.dependencies:
                assert dependency is not target
                assert dependency in self._targets

    def write(self,f):
        self._check()

        f.write(f'''MAKEFLAGS+=--no-print-directory\n''')
        f.write(f'''_V:=$(if $(VERBOSE),,@)\n''')
        f.write(f'''PYTHON:={sys.executable}\n''')
        f.write(f'''.PHONY:default\n''')
        f.write(f'''default:\n''')
        f.write(f'''\t$(error Must specify target)\n''')

        for target in self._targets:
            f.write(f'''.PHONY:{target.name}\n''')
            f.write(f'''{target.name}:{' '.join([dependency.name for dependency in target.dependencies])}\n''')
            for line in target.lines: f.write(f'''\t{line}\n''')

##########################################################################
##########################################################################

UnixCompiler=collections.namedtuple('UnixCompiler','cc cxx')

# TODO: now a misnomer
class BuildMatrix:
    def __init__(self):
        self.xcode=False
        self.vs2022=False
        self.unix_build_types=[]
        self.unix_sanitizers=[]
        self.unix_compilers=[]

##########################################################################
##########################################################################

def get_optional_option(option,value):
    if value is None: return ''
    else: return ' %s "%s"'%(option,value)

##########################################################################
##########################################################################

UnixBuild=collections.namedtuple('UnixBuild','build_type compiler sanitizer target_name_suffix')

CreateBuildMakefileResult=collections.namedtuple('CreateBuildMakefileResult','makefile unix_builds')

# TODO: now a misnomer
def create_init_makefile(matrix,
                         build_folder,
                         options):
    global_options=' '
    global_options+=' $(if $(VERBOSE),--verbose,)'
    global_options+=' --working-copy "%s"'%(os.path.relpath(options.g_working_copy_path,build_folder))

    cmd_options=' '
    cmd_options+=get_optional_option('--prefix',options.prefix)
    cmd_options+=get_optional_option('--name',options.name)
    if is_macos():
        cmd_options+=get_optional_option('--osx-deployment-target',options.osx_deployment_target)
    
    b2build_py_path=os.path.join(options.g_working_copy_path,'bin',b2build_py_basename)
    b2build_py_path=os.path.relpath(b2build_py_path,build_folder)
    
    makefile=Makefile()

    if len(matrix.unix_build_types)>0:
        unix_compilers=matrix.unix_compilers[:]
        if len(unix_compilers)==0: unix_compilers.append(None)

        unix_sanitizers=matrix.unix_sanitizers[:]
        if len(unix_sanitizers)==0: unix_sanitizers.append(None)

    def get_output_path(name):
        return os.path.join('%s%s'%(options.prefix or '',name))

    init_targets=[]
    unix_builds=[]
    for build_type in matrix.unix_build_types:
        # Sigh... does this really have to be inside a loop?
        if is_linux(): os_name='linux'
        elif is_macos(): os_name='osx'
        else: fatal('unexpected OS type')

        for compiler in unix_compilers:
            for sanitizer in unix_sanitizers:
                target_name_suffix='%s%s'%(build_type,sanitizer or '')
                if compiler is not None:
                    # C compiler name is usually sufficient to
                    # identify it.
                    target_name_suffix+='_%s'%compiler.cc

                folder_name='%s%s.%s'%(build_type,sanitizer or '',os_name)
                if compiler is not None:
                    if compiler.cc!='cc': folder_name+='.%s'%compiler.cc

                unix_builds.append(UnixBuild(build_type=build_type,
                                             compiler=compiler.cc if compiler else '(default)',
                                             sanitizer=sanitizer,
                                             target_name_suffix=target_name_suffix))

                output_path=get_output_path(folder_name)
                bin_rel_path=os.path.relpath(
                    os.path.join(options.g_working_copy_path,'bin'),
                    os.path.join(build_folder,output_path))
                
                # Add init target.
                target=makefile.add_named_target('init_unix_%s'%target_name_suffix)
                init_targets.append(target)

                if sanitizer is None: cmd_prefix=''
                else:
                    # It's normal for not all sanitizers to be
                    # available. Try all of them, and ignore
                    # failures. If there's anything actually wrong
                    # with the build, it'll show up in one of the
                    # non-sanitizer cases.
                    cmd_prefix='-'

                line=f'''$(_V){cmd_prefix}$(PYTHON) {b2build_py_path}'''
                line+=global_options
                line+=' _init_unix '
                line+=get_optional_option('--sanitizer',sanitizer)
                line+=get_optional_option('--prefix',options.prefix)
                if compiler is not None:
                    line+=' --cc "%s"'%compiler.cc
                    line+=' --cxx "%s"'%compiler.cxx
                line+=' %s'%build_type

                line+=' "%s"'%output_path

                target.add_line(line)

                # Add build target.
                target=makefile.add_named_target('build_unix_%s'%target_name_suffix)
                j_option=f'''-j {options.g_max_jobs}'''

                target.add_line(f'''$(_V)cd "{output_path}" && ninja {j_option}''')

                # Add test target.
                target=makefile.add_named_target('test_unix_%s'%target_name_suffix)
                target.add_line(f'''$(_V)cd "{output_path}" && ctest --progress {j_option}''')
                target.add_line(f'''$(_V)cd "{output_path}" && $(PYTHON) "{os.path.join(bin_rel_path,'check_ctest_log.py')}" "Testing/Temporary/LastTest.log"''')
                

    if matrix.xcode:
        target=makefile.add_named_target('init_xcode')
        init_targets.append(target)

        target.add_line(f'''$(_V)$(PYTHON) "{b2build_py_path}"{global_options} _init_xcode {cmd_options} {get_output_path('Xcode')}\n''')

    target=makefile.add_named_target('init_all')
    for init_target in init_targets: target.add_dependency(init_target)

    # target=makefile.add_named_target('build_unix_all')
    # if len(build_unix_targets)==0: target.add_line('$(error No Unix targets)')
    # else:
    #     for build_unix_target in build_unix_targets:
    #         target.add_dependency(build_unix_target)

    return CreateBuildMakefileResult(makefile=makefile,
                                     unix_builds=unix_builds)
    
##########################################################################
##########################################################################

def run_make(build_folder,makefile_basename,target,options):
    with ChangeDirectory(build_folder):
        argv=[get_make_path(options)]
        argv+=get_max_jobs_args_for_make(options)
        argv+=['-f',makefile_basename]
        argv+=[target]
        if options.g_verbose: argv+=['VERBOSE=1']
        
        ret=run_subprocess(argv,options,close_fds=False)
        if ret.returncode!=0: fatal('failed with exit code %d: %s'%(ret.returncode,get_copyable_argv(argv)))

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

    matrix=BuildMatrix()

    matrix.vs2022=options.vs2022
    matrix.xcode=options.xcode

    if options.unix:
        matrix.unix_build_types+=[t for t in CMAKE_BUILD_TYPES.keys()]

        if options.cc is not None:
            matrix.unix_compilers.append(UnixCompiler(cc=options.cc,
                                                      cxx=options.cxx))

        if options.enable_sanitizers:
            matrix.unix_sanitizers+=[s for s in UNIX_SANITIZER_TYPES.keys()]

    build_folder=get_build_folder_path('',options)

    result=create_init_makefile(matrix,build_folder,options)

    makefile_basename='Makefile.init.mak'
    makedirs(build_folder)
    makefile_path=os.path.join(build_folder,makefile_basename)
    
    with open(makefile_path,'wt') as f: result.makefile.write(f)

    run_make(build_folder,makefile_basename,'init_all',options)

    print('''Init completed successfully. (It's normal for some errors and warnings to be printed during the process. If you can see this message, it finished successfully and nothing unexpected happened.)''')

##########################################################################
##########################################################################

def _init_xcode_cmd(options):
    if not is_macos(): fatal('Can build with Xcode on macOS only')
    
    rmtree(options.output_path)
    makedirs(options.output_path)

    with ChangeDirectory(options.output_path):
        argv=['cmake','-G','Xcode']
        argv+=get_cmake_defines(options)
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    options.output_path)]
        argv+=['-B','.']
        ret=run_subprocess(argv,options,close_fds=False)
        if ret.returncode!=0: fatal('failed with exit code %d: %s'%(ret.returncode,get_copyable_argv(argv)))

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

    rmtree(options.output_path)
    makedirs(options.output_path)

    # bit ugly, but all the process does is run cmake then quit, so
    # it's not a massive problem having these settings lie around
    # afterwards.
    if options.cc is not None: os.putenv('CC',options.cc)
    if options.cxx is not None: os.putenv('CXX',options.cxx)

    # TODO: might be nice to have the build system configurable?
    with ChangeDirectory(options.output_path):
        argv=['cmake','-G','Ninja']
        argv+=get_cmake_defines(options)
        if options.sanitizer is not None:
            argv+=['-DSANITIZE_%s=On'%sanitizer.cmake_name]
        argv+=['-DCMAKE_BUILD_TYPE=%s'%cmake_build_type]
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    options.output_path)]
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

def batch_cmd(options):
    matrix=BuildMatrix()

    if is_windows(): matrix.vs2022=True

    if is_unix():
        matrix.unix_build_types+=[t for t in CMAKE_BUILD_TYPES.keys()]

        if len(options.compilers)>0:
            for compiler in options.compilers:
                matrix.unix_compilers.append(UnixCompiler(cc=compiler[0],
                                                          cxx=compiler[1]))

    build_folder=get_build_folder_path('',options)

    result=create_init_makefile(matrix,build_folder,options)

    makefile_basename='Makefile.batch.mak'
    makedirs(build_folder)
    makefile_path=os.path.join(build_folder,makefile_basename)

    time_jobs=f'''$(PYTHON) "{os.path.relpath(os.path.join(options.g_working_copy_path,'bin/time_jobs.py'),build_folder)}" -f "./time_jobs.txt"'''

    target=result.makefile.add_named_target('batch')

    target.add_line(f'''$(_V){time_jobs} init''')

    def do_unix_build_actions(target_name_prefix,action_name):
        target.add_line(f'''$(_V){time_jobs} push Action "{action_name}"''')
        for i,unix_build in enumerate(result.unix_builds):
            message=f'''{i+1}/{len(result.unix_builds)}: Action={action_name}; Configuration={unix_build.build_type}; Compiler={unix_build.compiler}; Sanitizer={unix_build.sanitizer}'''
            
            target.add_line(f'''$(_V)echo "{message}"\n''')
            
            target.add_line(f'''$(_V){time_jobs} push Compiler "{unix_build.compiler}"''')
            target.add_line(f'''$(_V){time_jobs} push Config "{unix_build.build_type}"''')
            if unix_build.sanitizer is not None:
                target.add_line(f'''$(_V){time_jobs} push Sanitizer "{unix_build.sanitizer}"''')

            target.add_line(f'''$(_V)$(MAKE) -f "{makefile_basename}" {target_name_prefix}{unix_build.target_name_suffix}''')

            if unix_build.sanitizer is not None:
                target.add_line(f'''$(_V){time_jobs} pop''')

            target.add_line(f'''$(_V){time_jobs} pop''')
            target.add_line(f'''$(_V){time_jobs} pop''')
        target.add_line(f'''$(_V){time_jobs} pop''')
    
    do_unix_build_actions('build_unix_','Build')
    do_unix_build_actions('test_unix_','Test')

    target.add_line(f'''$(_V){time_jobs} print -s Action -s Config -s Compiler''')
    
    with open(makefile_path,'wt') as f: result.makefile.write(f)

    run_make(build_folder,makefile_basename,'init_all',options)
    run_make(build_folder,makefile_basename,'batch',options)

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
        subparser.add_argument('--osx-deployment-target',metavar='TARGET',default=default_osx_deployment_target,help='''specify macOS deployment target.'''+('' if default_osx_deployment_target is None else ' Default: %s'%default_osx_deployment_target))
        subparser.add_argument('--prefix',metavar='STRING',help='''prepend %(metavar)s to name of any build folder created''')
        subparser.add_argument('--name',metavar='STRING',help='''use %(metavar)s as the build name''')

    def add_common_target_options(subparser):
        subparser.add_argument('--unix',action='store_true',help='''initialise Unix-style build''')
        subparser.add_argument('--vs2022',action='store_true',help='''initialise VS2022 build''')
        subparser.add_argument('--xcode',action='store_true',help='''initialise Xcode build''')
        subparser.add_argument('--enable-sanitizers',action='store_true',help='''if building Unix-style, try to use any supported sanitizers''')

    init_subparser=add_subparser('init',init_cmd,help='''initialise build''')
    init_subparser.add_argument('--cc',metavar='NAME',help='''if building Unix-style, use %(metavar) as C compiler''')
    init_subparser.add_argument('--cxx',metavar='NAME',help='''if building Unix-style, use %(metavar)s as C++ compiler''')
    add_common_init_options(init_subparser)
    add_common_target_options(init_subparser)

    _init_xcode_subparser=add_subparser('_init_xcode',_init_xcode_cmd,help='''initialise Xcode build''')
    add_common_init_options(_init_xcode_subparser)
    _init_xcode_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')

    _init_unix_subparser=add_subparser('_init_unix',_init_unix_cmd,help='''initialise Unix build''')
    add_common_init_options(_init_unix_subparser)
    _init_unix_subparser.add_argument('--sanitizer',help='''specify sanitizer: '''+'; '.join(['%s (%s)'%(k,v.friendly_name) for k,v in UNIX_SANITIZER_TYPES.items()]))
    _init_unix_subparser.add_argument('build',help='''specify build configuration: '''+'; '.join(['%s (%s)'%(k,v) for k,v in CMAKE_BUILD_TYPES.items()]))
    _init_unix_subparser.add_argument('--keep',action='store_true',help='''don't delete build folder if init fails''')
    _init_unix_subparser.add_argument('--cc',metavar='NAME',help='''use %(metavar) as C compiler''')
    _init_unix_subparser.add_argument('--cxx',metavar='NAME',help='''use %(metavar)s as C++ compiler''')
    _init_unix_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')

    batch_subparser=add_subparser('batch',batch_cmd,help='''do batch builds/tests''')
    batch_subparser.add_argument('--cc-cxx',metavar='CC CXX',nargs=2,action='append',dest='compilers',default=[],help='''use %(metavar)s as C and C++ compiler respectively for Unix builds. Can specifiy multiple times''')
    add_common_init_options(batch_subparser)

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
