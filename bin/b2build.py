#!/usr/bin/python3
import sys,os,os.path,argparse,subprocess,shutil,shlex,collections,tempfile,glob,textwrap,datetime,time

##########################################################################
##########################################################################

b2build_py_basename=os.path.basename(__file__)

##########################################################################
##########################################################################

# only relevant if running on macOS.
g_local_osx_deployment_target=None

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

        pv('b2build ChangeDirectory was: %s\n'%self._oldcwd)
        pv('b2build ChangeDirectory now: %s\n'%self._newcwd)

    def __enter__(self):
        os.chdir(self._newcwd)
        return self

    def __exit__(self,*args): os.chdir(self._oldcwd)

    def relpath(self,path): return os.path.relpath(path,self._newcwd)

    def join(self,path): return os.path.join(self._newcwd,path)

##########################################################################
##########################################################################

def rm(path):
    if os.path.isfile(path):
        pv(f'''b2build rm: {path}\n''')
        os.unlink(path)

def makedirs(path):
    if not os.path.isdir(path):
        pv(f'''b2build mkdir: {path}\n''')
        os.makedirs(path)

def rmtree(path):
    if os.path.isdir(path):
        pv(f'''b2build rmtree: {path}\n''')
        shutil.rmtree(path)

def rmfiles(pattern):
    paths=glob.glob(pattern)
    for path in paths: rm(path)

def copyfile(src,dest):
    pv(f'b2build copyfile: {src} -> {dest}\n')
    shutil.copyfile(src,dest)

def copytree(src,dest,**kwargs):
    pv(f'b2build copytree: {src} -> {dest}\n')
    shutil.copytree(src,dest,**kwargs)

def remakedirs(path):
    rmtree(path)
    makedirs(path)

# def move(src,dest,**kwargs):
#     pv(f'b2build move: {src} -> {dest}\n')
#     shutil.move(src,dest,**kwargs)

##########################################################################
##########################################################################

def set_file_timestamps(timestamp,fname):
    if timestamp is None: return

    if os.path.islink(fname):
        # There's a link to /Applications in the dmg, and this is a
        # lame way of avoiding touching it.
        return

    t=timestamp.timestamp()

    try:
        os.utime(fname,(t,t))
    except :
        print("WARNING: failed to set timestamps for: %s"%fname,file=sys.stderr)
        pass

##########################################################################
##########################################################################
    
def set_tree_timestamps(timestamp,root):
    for dirpath,dirnames,filenames in os.walk(root):
        set_file_timestamps(timestamp,dirpath)
        for f in filenames: 
            set_file_timestamps(timestamp,os.path.join(dirpath,f))

##########################################################################
##########################################################################

def is_macos(): return sys.platform=='darwin'

def is_windows(): return sys.platform=='win32'

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

##########################################################################
##########################################################################

FakeProcess=collections.namedtuple('FakeProcess','returncode')

def run_subprocess(argv,options,execute=True,**other_popen_kwargs):
    argv=[arg for arg in argv if arg is not None]

    suffix='(cwd: %s): %s'%(os.getcwd(),get_copyable_argv(argv))

    if execute:
        pv(f'b2build running   {suffix}\n')
            
        process=subprocess.Popen(argv,**other_popen_kwargs)
        process.wait()

        pv(f'b2build completed {suffix} - exit code: {process.returncode}\n')
    else:
        # return process with error exit code. If the caller doesn't
        # check: no problem!
        process=FakeProcess(returncode=1)
        pv(f'b2build running   {suffix} - fake exit code: {process.returncode}\n')
        
    return process

##########################################################################
##########################################################################

def must_run_subprocess(argv,options,execute=True,**other_popen_kwargs):
    argv=[arg for arg in argv if arg is not None]
    result=run_subprocess(argv,options,execute=execute,**other_popen_kwargs)

    if execute:
        if result.returncode!=0:
            fatal('failed with return code %d: %s'%(result.returncode,get_copyable_argv(argv)))
    else:
        # If the caller supplied execute=False, it is presumably alert
        # to the possibilty that nothing might happen.
        pass

##########################################################################
##########################################################################

def must_capture_subprocess(argv,options,**other_popen_kwargs):
    if g_verbose:
        suffix='(cwd: %s): %s'%(os.getcwd(),get_copyable_argv(argv))
        print(f'b2build capturing {suffix}')

    try:
        result=subprocess.check_output(argv,text=True)
        return result.strip()
    except subprocess.CalledProcessError as e:
        fatal('failed with return code %d: %s'%(e.returncode,get_copyable_argv(argv)))

##########################################################################
##########################################################################

def get_head_revision():
    argv=['git','rev-parse','HEAD']
    result=subprocess.check_output(argv,text=True).strip()
    pv('b2build: head revision: %s\n'%result)
    return result

##########################################################################
##########################################################################

def get_current_branch():
    argv=['git','branch','--show-current']
    result=subprocess.check_output(argv,text=True).strip()
    pv('b2build: current branch: %s\n'%result)
    return result

##########################################################################
##########################################################################

def get_make_path(caller_path,options):
    if is_unix(): return options.g_make_path
    else: return os.path.relpath(
            os.path.join(options.g_working_copy_path,'bin/snmake.exe'),
            caller_path)

##########################################################################
##########################################################################

def get_max_jobs_args_for_make(options):
    # if os.getenv('MAKEFLAGS') is not None:
    #     if not options.g_ignore_submake:
    #         # Probably running as part of a submake, so don't specify -j.
    #         pv(f'''b2build: MAKEFLAGS detected. Assuming running from Make. Running make without -j {options.g_max_jobs}\n''')
    #         return []
        
    return ['-j',str(options.g_max_jobs)]

##########################################################################
##########################################################################

def get_build_folder_path(options):
    path=os.path.join(options.g_working_copy_path,'build')
    return path

##########################################################################
##########################################################################

def get_cmake_defines(options):
    defines=[]
    
    if is_macos():
        if getattr(options,'osx_deployment_target',None) is not None:
            defines.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=%s'%options.osx_deployment_target)

        # new requirement for CMake 4.x.
        defines.append('-DCMAKE_OSX_SYSROOT=macosx')

    if options.name is not None:
        defines.append('-DRELEASE_NAME=%s'%options.name)

    return defines

##########################################################################
##########################################################################

CMAKE_CONFIGURATIONS={
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
    def __init__(self,name,phony=True):
        assert ' ' not in name,name
        self.name=name
        self.lines=[]
        self.dependencies=[]
        self.phony=phony

    def add_dependency(self,dependency):
        assert isinstance(dependency,Target),type(dependency)
        if dependency not in self.dependencies:
            self.dependencies.append(dependency)
        
    def add_line(self,line):
        assert isinstance(line,str),type(line)
        self.lines.append(line)

class Makefile:
    def __init__(self):
        self._targets=[]

    def add_named_target(self,name,phony=True):
        target=Target(name,phony)
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
            if target.phony: f.write(f'''.PHONY:{target.name}\n''')
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
        self.vs2022_architecture=None
        self.unix_configurations=[]
        self.unix_sanitizers=[]
        self.unix_compilers=[]
        self.architectures=[]
        self.ffmpeg=True
        self.osx_deployment_target=None

##########################################################################
##########################################################################

def get_optional_option(option,value):
    if value is None: return ''
    else: return ' %s "%s"'%(option,value)

##########################################################################
##########################################################################

VSStuff=collections.namedtuple('VSStuff','year install_path devenv_path cmake_path ctest_path')

def get_vs_stuff(version):
    if version==17: year=2022
    else:
        # TODO: add more cases as required.
        fatal('unrecognised Visual Studio version: %s'%version)
    
    argv=[r'''C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe''',
          '-version',str(version),
          '-property','installationPath']
    vswhere_subprocess=subprocess.Popen(argv,
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.DEVNULL)
    vswhere_subprocess.wait()
    if vswhere_subprocess.returncode!=0:
        fatal('failed with exit code %d: %s'%(ret.returncode,get_copyable_argv(argv)))

    # TODO: do you need to specify an encoding here?
    data=vswhere_subprocess.stdout.read()
    install_path=data.strip().decode()

    cmake_bin_path=os.path.join(install_path,
                                'common7',
                                'IDE',
                                'CommonExtensions',
                                'Microsoft',
                                'CMake',
                                'CMake',
                                'bin')

    def get_required_file_path(*parts):
        path=os.path.join(*parts)
        if not os.path.isfile(path): fatal('file not found: %s'%path)
        return path

    cmake_path=get_required_file_path(cmake_bin_path,'cmake.exe')
    ctest_path=get_required_file_path(cmake_bin_path,'ctest.exe')
    devenv_path=get_required_file_path(install_path,'Common7/IDE/devenv.com')

    return VSStuff(year=year,
                   install_path=install_path,
                   devenv_path=devenv_path,
                   cmake_path=cmake_path,
                   ctest_path=ctest_path)

##########################################################################
##########################################################################

BuildType=collections.namedtuple(
    'BuildType',
    [
        # configuration: d/r/f
        'configuration',
        # human-readable compiler name
        'compiler',
        # sanitizer: None, or a/t/m/u
        'sanitizer',
        # Makefile target for init. (May be shared between multiple
        # configurations: scrape the list to detect this.)
        'init_target',
        # Makefile target for clean
        'clean_target',
        # Makefile target for build
        'build_target',
        # Makefile target for test
        'test_target',
        # output path for init results. (May be shared between
        # multiple configurations, same as init_target.)
        'output_path'
    ])

CreateBuildMakefileResult=collections.namedtuple('CreateBuildMakefileResult','makefile build_types')

def create_build_makefile(matrix,
                          build_folder,
                          prefix,
                          options):
    global_options=' '
    global_options+=' $(if $(VERBOSE),--verbose,)'
    global_options+=' --working-copy "%s"'%(os.path.relpath(options.g_working_copy_path,build_folder))

    b2build_py_path=os.path.join(options.g_working_copy_path,'bin',b2build_py_basename)
    b2build_py_path=os.path.relpath(b2build_py_path,build_folder)
    
    makefile=Makefile()

    if len(matrix.unix_configurations)>0:
        unix_compilers=matrix.unix_compilers[:]
        if len(unix_compilers)==0: unix_compilers.append(None)

        unix_sanitizers=matrix.unix_sanitizers[:]
        if len(unix_sanitizers)==0: unix_sanitizers.append(None)

    # relative to build folder
    def get_output_path(name):
        return os.path.join('%s%s'%(prefix or '',name))

    build_types=[]

    cmd_options=''
    cmd_options+=get_optional_option('--name',options.name)
    if is_macos():
        cmd_options+=get_optional_option('--osx-deployment-target',
                                         matrix.osx_deployment_target)

    # as used to pass -j down to ninja or ctest.
    j_option=f'''-j {options.g_max_jobs}'''
    
    for configuration in matrix.unix_configurations:
        # Sigh... does this really have to be inside a loop?
        if is_linux(): os_name='linux'
        elif is_macos(): os_name='osx'
        else: fatal('unexpected OS type')

        for compiler in unix_compilers:
            for sanitizer in unix_sanitizers:
                target_name_suffix='%s%s'%(configuration,sanitizer or '')
                if compiler is not None:
                    # C compiler name is usually sufficient to
                    # identify it.
                    target_name_suffix+='_%s'%compiler.cc

                folder_name='%s%s.%s'%(configuration,sanitizer or '',os_name)
                if compiler is not None:
                    if compiler.cc!='cc': folder_name+='.%s'%compiler.cc

                output_path=get_output_path(folder_name)
                bin_rel_path=os.path.relpath(
                    os.path.join(options.g_working_copy_path,'bin'),
                    os.path.join(build_folder,output_path))
                
                # Add init target.
                init_target=makefile.add_named_target('init_unix_%s'%target_name_suffix)
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
                line+=' _init_unix'
                line+=get_optional_option('--sanitizer',sanitizer)
                line+=' '+cmd_options

                if compiler is not None:
                    line+=' --cc "%s"'%compiler.cc
                    line+=' --cxx "%s"'%compiler.cxx
                if not matrix.ffmpeg: line+=' --no-ffmpeg'
                
                line+=' %s'%configuration

                line+=' "%s"'%output_path

                init_target.add_line(line)

                # Add clean target
                clean_target=makefile.add_named_target('clean_unix_%s'%target_name_suffix)
                clean_target.add_line(f'''$(_V)cd "{output_path}" && ninja clean $(if $(VERBOSE),--verbose,)''')

                # Add build target.
                build_target=makefile.add_named_target('build_unix_%s'%target_name_suffix)
                build_target.add_line(f'''$(_V)cd "{output_path}" && ninja {j_option} $(if $(VERBOSE),--verbose,)''')

                # Add test target.
                #
                # If VERBOSE=1, don't suppl/y --verbose: the output
                # is... a lot. --output-on-failure will produce only
                # the interesting stuff.
                test_target=makefile.add_named_target('test_unix_%s'%target_name_suffix)
                test_target.add_line(f'''$(_V)cd "{output_path}" && ctest --progress {j_option} $(if $(VERBOSE),--output-on-failure,)''')
                test_target.add_line(f'''$(_V)cd "{output_path}" && $(PYTHON) "{os.path.join(bin_rel_path,'check_ctest_log.py')}" "Testing/Temporary/LastTest.log"''')
                
                build_types.append(
                    BuildType(configuration=configuration,
                              compiler=compiler.cc if compiler else '(default)',
                              sanitizer=sanitizer,
                              init_target=init_target,
                              clean_target=clean_target,
                              build_target=build_target,
                              test_target=test_target,
                              output_path=output_path))

    if matrix.xcode:
        target=makefile.add_named_target('init_xcode')

        output_path=get_output_path('Xcode')
        target.add_line(f'''$(_V)$(PYTHON) "{b2build_py_path}"{global_options} _init_xcode {cmd_options} {output_path}''')

        build_types.append(
            BuildType(configuration=None,
                      compiler='Xcode',
                      sanitizer=None,
                      init_target=target,
                      clean_target=None,
                      build_target=None,
                      test_target=None,
                      output_path=output_path))

    def add_visual_studio_targets(vsver):
        vs_stuff=get_vs_stuff(17)

        output_path_name=f'vs{vs_stuff.year}'
        if matrix.vs2022_architecture is not None:
            output_path_name+=f'_{matrix.vs2022_architecture}'
        output_path=get_output_path(output_path_name)
        
        #init_target=makefile.add_named_target(f'init_vs{vs_stuff.year}')
        init_target=makefile.add_named_target(output_path,phony=False)

        bin_rel_path=os.path.relpath(
            os.path.join(options.g_working_copy_path,'bin'),
            os.path.join(build_folder,output_path))

        def get_msbuild_bat_path(caller_path):
            return os.path.relpath(
                os.path.join(options.g_working_copy_path,
                             'bin/msbuild_bug_wrapper.bat'),
                caller_path)

        init_target.add_line(f'''$(_V)"{get_msbuild_bat_path(build_folder)}" $(PYTHON) "{b2build_py_path}" {global_options} _init_vs {get_optional_option('--name',options.name)} {get_optional_option('--architecture',matrix.vs2022_architecture)} {vsver} "{output_path}"''')

        for configuration,cmake_build_type in CMAKE_CONFIGURATIONS.items():
            build_target=makefile.add_named_target(f'build_vs{vs_stuff.year}{configuration}')
            msbuild_bat=get_msbuild_bat_path(os.path.join(build_folder,
                                                          output_path))
            
            build_target.add_line(f'''$(_V)cd "{output_path}" && "{msbuild_bat}" "{vs_stuff.devenv_path}" b2.sln /Build "{cmake_build_type}"''')

            clean_target=makefile.add_named_target(f'clean_vs{vs_stuff.year}{configuration}')
            clean_target.add_line(f'''$(_V)cd "{output_path}" && "{msbuild_bat}" "{vs_stuff.devenv_path}" b2.sln /Clean "{cmake_build_type}"''')

            test_target=makefile.add_named_target(f'test_vs{vs_stuff.year}{configuration}')
            test_target.add_line(f'''$(_V)cd "{output_path}" && "{vs_stuff.ctest_path}" {j_option} -C "{cmake_build_type}" --progress''')
            test_target.add_line(f'''$(_V)cd "{output_path}" && $(PYTHON) "{os.path.join(bin_rel_path,'check_ctest_log.py')}" "Testing/Temporary/LastTest.log"''')

            build_types.append(
                BuildType(configuration=configuration,
                          compiler=f'VS{vs_stuff.year}',
                          sanitizer=None,
                          init_target=init_target,
                          clean_target=clean_target,
                          build_target=build_target,
                          test_target=test_target,
                          output_path=output_path))

    if matrix.vs2022: add_visual_studio_targets(17)

    # init_all depends on all init targets.
    target=makefile.add_named_target('init_all')
    for build_type in build_types:
        if build_type.init_target is not None:
            target.add_dependency(build_type.init_target)

    return CreateBuildMakefileResult(makefile=makefile,
                                     build_types=build_types)

##########################################################################
##########################################################################

def clean_output_paths(build_folder,build_types):
    with ChangeDirectory(build_folder):
        for build_type in build_types: rmtree(build_type.output_path)

##########################################################################
##########################################################################

def run_make(build_folder,makefile_basename,target,options):
    with ChangeDirectory(build_folder):
        argv=[get_make_path(build_folder,options)]
        argv+=get_max_jobs_args_for_make(options)
        argv+=['-f',makefile_basename]
        argv+=[target]
        if options.g_verbose: argv+=['VERBOSE=1']
        
        must_run_subprocess(argv,options,close_fds=False)

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
    matrix.vs2022_architecture=options.vs2022_architecture
    matrix.xcode=options.xcode
    matrix.osx_deployment_target=options.osx_deployment_target

    if options.unix:
        matrix.unix_configurations+=[t for t in CMAKE_CONFIGURATIONS.keys()]

        if options.cc is not None:
            matrix.unix_compilers.append(UnixCompiler(cc=options.cc,
                                                      cxx=options.cxx))

        if options.enable_sanitizers:
            matrix.unix_sanitizers+=[s for s in UNIX_SANITIZER_TYPES.keys()]

    build_folder=get_build_folder_path(options)

    result=create_build_makefile(matrix,
                                 build_folder,
                                 options.prefix,
                                 options)

    makefile_basename='Makefile.init.mak'
    makedirs(build_folder)
    makefile_path=os.path.join(build_folder,makefile_basename)
    
    with open(makefile_path,'wt') as f: result.makefile.write(f)

    if not options.reinit:
        clean_output_paths(build_folder,result.build_types)

    run_make(build_folder,makefile_basename,'init_all',options)

    print('''Init completed successfully. (It's normal for some errors and warnings to be printed during the process. If you can see this message, it finished successfully and nothing unexpected happened.)''')

##########################################################################
##########################################################################

def _init_xcode_cmd(options):
    if not is_macos(): fatal('Can build with Xcode on macOS only')
    
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
    
    cmake_configuration=CMAKE_CONFIGURATIONS.get(options.build)
    if cmake_configuration is None:
        fatal('unknown build type: %s'%options.build)

    if options.sanitizer is None: sanitizer=None
    else:
        sanitizer=UNIX_SANITIZER_TYPES.get(options.sanitizer)
        if sanitizer is None:
            fatal('unknown sanitizer type: %s'%options.sanitizer)

    # bit ugly, but all the process does is run cmake then quit, so
    # it's not a massive problem having these settings lie around
    # afterwards.
    if options.cc is not None: os.putenv('CC',options.cc)
    if options.cxx is not None: os.putenv('CXX',options.cxx)

    makedirs(options.output_path)

    # TODO: might be nice to have the build system configurable?
    with ChangeDirectory(options.output_path):
        argv=['cmake','-G','Ninja']
        argv+=get_cmake_defines(options)
        if options.sanitizer is not None:
            argv+=['-DSANITIZE_%s=On'%sanitizer.cmake_name]
        argv+=['-DCMAKE_BUILD_TYPE=%s'%cmake_configuration]
        if not options.ffmpeg: argv+=['-DUSE_FFMPEG=OFF']
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    options.output_path)]
        argv+=['-B','.']
        ret=run_subprocess(argv,options,close_fds=False)
    if ret.returncode!=0:
        rmtree(options.output_path)
        fatal('init failed for CMake configuration: %s'%cmake_configuration)

##########################################################################
##########################################################################

def _init_vs_cmd(options):
    vs_stuff=get_vs_stuff(options.version)

    if options.architecture is not None:
        if options.architecture.lower()=='x64': cmake_architecture='x64'
        elif options.architecture.lower()=='arm': cmake_architecture='ARM64'
        else: fatal('unknown architecture: %s'%options.architecture)
    else: cmake_architecture=None

    makedirs(options.output_path)

    with ChangeDirectory(options.output_path):
        argv=[vs_stuff.cmake_path,'-G',f'Visual Studio {options.version} {vs_stuff.year}']
        if cmake_architecture is not None: argv+=['-A',cmake_architecture]
        argv+=get_cmake_defines(options)
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    options.output_path)]
        argv+=['-B','.']
        ret=run_subprocess(argv,options,close_fds=False)
    if ret.returncode!=0:
        rmtree(options.output_path)
        fatal('init failed')

##########################################################################
##########################################################################

# get timestamp for build: time of HEAD, in UTC
def get_build_timestamp(options):
    with ChangeDirectory(options.g_working_copy_path):
        s=must_capture_subprocess(['git','log','-1','--format=%cd','--date=iso-strict'],options)
    dt=datetime.datetime.fromisoformat(s.strip())
    dt=dt.astimezone(tz=datetime.timezone.utc)
    return dt

##########################################################################
##########################################################################

def print_build_suffix_cmd(options):
    t=get_build_timestamp(options)
    h=must_capture_subprocess(['git','log','-1','--format=%h'],options)
    print(f'''{t.strftime('%Y%m%d-%H%M%S')}-{h}''')

##########################################################################
##########################################################################
    
def print_build_timestamp_cmd(options):
    t=get_build_timestamp(options)
    print(t.isoformat())

##########################################################################
##########################################################################

def batch_cmd(options):
    matrix=BuildMatrix()

    if is_windows(): matrix.vs2022=True

    if is_unix():
        matrix.unix_configurations+=[t for t in CMAKE_CONFIGURATIONS.keys()]

        if len(options.compilers)>0:
            for compiler in options.compilers:
                matrix.unix_compilers.append(UnixCompiler(cc=compiler[0],
                                                          cxx=compiler[1]))

    if is_macos():
        matrix.osx_deployment_target=g_local_osx_deployment_target

    build_folder=get_build_folder_path(options)

    result=create_build_makefile(matrix,
                                 build_folder,
                                 options.prefix,
                                 options)

    makefile_basename='Makefile.batch.mak'
    makedirs(build_folder)
    makefile_path=os.path.join(build_folder,makefile_basename)

    time_jobs=f'''$(PYTHON) "{os.path.relpath(os.path.join(options.g_working_copy_path,'bin/time_jobs.py'),build_folder)}" -f "./time_jobs.txt"'''

    target=result.makefile.add_named_target('batch')

    target.add_line(f'''$(_V){time_jobs} init''')

    def time_jobs_push(key,value):
        target.add_line(f'''$(_V){time_jobs} push "{key}" "{value}"''')

    def time_jobs_pop():
        target.add_line(f'''$(_V){time_jobs} pop''')

    def do_build_actions(action_name,attr):
        for build_type_index,build_type in enumerate(result.build_types):
            if (build_type.clean_target is None or
                build_type.build_target is None or
                build_type.test_target is None):
                # this target is not buildable from the command line.
                continue

            configuration_name=CMAKE_CONFIGURATIONS[build_type.configuration]
            
            message=f'''{build_type_index+1}/{len(result.build_types)}: Action={action_name}; Configuration={configuration_name}; Compiler={build_type.compiler}; Sanitizer={build_type.sanitizer}'''

            if is_windows(): quotes=''
            else: quotes='"'
            target.add_line(f'''$(_V)echo {quotes}{message}{quotes}''')

            time_jobs_push('Action',action_name)
            time_jobs_push('Compiler',build_type.compiler)
            time_jobs_push('Config',configuration_name)
            if build_type.sanitizer is not None:
                time_jobs_push('Sanitizer',build_type.sanitizer)

            target.add_line(f'''$(_V)$(MAKE) -f "{makefile_basename}" {getattr(build_type,attr).name}''')

            if build_type.sanitizer is not None: time_jobs_pop()
            time_jobs_pop()
            time_jobs_pop()
            time_jobs_pop()

    if options.clean: do_build_actions('Clean','clean_target')
    do_build_actions('Build','build_target')
    if options.test: do_build_actions('Test','test_target')

    target.add_line(f'''$(_V){time_jobs} print -s Action -s Config -s Compiler''')

    with open(makefile_path,'wt') as f: result.makefile.write(f)

    if options.init:
        clean_output_paths(build_folder,result.build_types)
        
    run_make(build_folder,makefile_basename,'init_all',options)
    run_make(build_folder,makefile_basename,'batch',options)

##########################################################################
##########################################################################

def set_submodule_upstreams_cmd(options):
    def set_submodule_upstream(submodule,url):
        path=os.path.join(options.g_working_copy_path,'submodules',submodule)
        if not os.path.isdir(path): fatal('not found: %s'%path)
        with ChangeDirectory(path):
            argv=['git','remote','remove','upstream']
            run_subprocess(argv,options,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

            argv=['git','remote','add','upstream',url]
            ret=run_subprocess(argv,options)
            if ret.returncode!=0: fatal('failed with exit code %d: %s'%(ret.returncode,get_copyable_argv(argv)))
        
    # submodules with upstreams on on GitHub
    for submodule,owner,repo in [
            ('Remotery','Celtoys','Remotery'),
            ('SDL_official','libsdl-org','SDL'),
            ('curl','curl','curl'),
            ('imgui','ocornut','imgui'),
            ('imgui_club','ocornut','imgui_club'),
            ('libuv','libuv','libuv'),
            ('macdylibbundler','auriamg','macdylibbundler'),
            ('perfect6502','mist64','perfect6502'),
            ('rapidjson','Tencent','rapidjson'),
            ('relacy','dvyukov','relacy'),
            ('salieri','nemequ','salieri'),
            ('visual6502','trebonian','visual6502'),
            ('imgui_test_engine','ocornut','imgui_test_engine'),
            ('6502Timing','dp111','6502Timing')]:
        set_submodule_upstream(submodule,
                               f'https://github.com/{owner}/{repo}')

##########################################################################
##########################################################################

def create_binary_release_README(rev_hash,options):
    with open("README.txt","wt") as f:
        f.write(f'''b2 - a BBC Micro emulator - {options.name}\n\n''')
        f.write(f'''For licence information, please consult LICENCE.txt.\n\n''')
        f.write(f'''Documentation can be found here: https://github.com/tom-seddon/b2/blob/{rev_hash}/README.md\n\n''')

##########################################################################
##########################################################################

def gh_release(release_files,options):
    with ChangeDirectory(options.g_working_copy_path):
        prerelease=get_current_branch()!='master'

        # Get head revision for the release process.
        hash=get_head_revision()

    if prerelease:
        notes='''Don't download: it's not ready yet! Please download an earlier pre-release, or get the current latest release from https://github.com/tom-seddon/b2/releases/latest'''
    else:
        notes='''Release notes to follow.'''

    # Form GitHub release name.
    release_name='b2-'+options.name
    if prerelease: release_name+='-prerelease'

    # Assume any errors from the creation process are due to the
    # release already existing. Worst case, that's wrong - and gh
    # release will fail.
    run_subprocess(['gh',
                    'release',
                    'create',
                    release_name,
                    '--notes',notes,
                    '--target',hash,
                    '--title',release_name,
                    '--prerelease' if prerelease else None],
                   options,
                   execute=options.gh_release)
    
    for release_file in release_files:
        must_run_subprocess(
            ['gh','release','upload',release_name,release_file],
            options,
            execute=options.gh_release)

##########################################################################
##########################################################################

# TODO: replace with some other mechanism, as part of the doc revamp
def fix_up_md(md_path,options):
    with open(md_path,'rt') as f:
        input_lines=[line.rstrip() for line in f.readlines()]

    index=0
    cmd_prefix='<!-- '
    cmd_suffix=' -->'

    cmd_begin_region='<< '
    cmd_end_region='>> '
    cmd_loc='@@ '

    def get_source_release_README():

        return extra_lines

    output_lines=[]
    output=True
    
    index=0
    region=None
    
    def fatal2(msg): fatal('%s(%d): %s'%(md_path,1+index,msg))

    while index<len(input_lines):
        input_line=input_lines[index]

        if (input_line.startswith(cmd_prefix) and
            input_line.endswith(cmd_suffix)):
            cmd=input_line[len(cmd_prefix):-len(cmd_suffix)].strip()
            pv('%s(%d): got cmd: %s\n'%(md_path,1+index,cmd))
            if cmd.startswith(cmd_begin_region):
                if region is not None: fatal2('''regions can't nest''')
                region=cmd[len(cmd_begin_region):].strip()
            elif cmd.startswith(cmd_end_region):
                if region is None: fatal2('''not in a region''')
                name=cmd[len(cmd_end_region):].strip()
                if name!=region: fatal2(f'''region end name mismatch: expected: {region}; got: {name}''')
                region=None
            elif cmd.startswith(cmd_loc):
                name=cmd[len(cmd_loc):].strip()
                if name=='source_release_README':
                    with ChangeDirectory(options.g_working_copy_path) as p:
                        rev_hash=get_head_revision()

                    output_lines=[]
                    output_lines+=textwrap.wrap(f'''This is the Linux source distribution for version: {options.name}. For licence information, please consult [`LICENCE.txt`](./LICENCE.txt).''')
                    output_lines+=['']
                    output_lines+=textwrap.wrap(f'''This documentation can also be found on GitHub: https://github.com/tom-seddon/b2/blob/{rev_hash}/README.md''')
                else: fatal2(f'''unknown name: {name}''')
            else: fatal2(f'''unrecognised cmd: {cmd}''')
                    
        else:
            if region=='not_source_release':
                pass
            else: output_lines.append(input_line)

        index+=1

    with open(md_path,'wt') as f: f.write('\n'.join(output_lines))

##########################################################################
##########################################################################
        
# imenu doesn't find the following function, but here's one that it
# does.
def release_source_linux_cmd_(options): pass

def release_source_linux_cmd(options):
    if is_windows(): fatal('not supported on Windows')

    if is_macos():
        # This isn't really designed for use on macOS, but macOS is
        # near enough Linux for some testing purposes...
        #
        # Linux source releases prepared on macOS are not valid.
        pass

    # stick the temp stuff in the build folder. It's excluded from the
    # archive so 
    temp=os.path.join(get_build_folder_path(options),
                      'release_source_linux_temp')

    remakedirs(temp)
    
    # temp folder contents during process:
    #
    # _b2-_pass1.tar
    # b2-<<name>>/
    # b2-<<name>>.tar.bz2
    # _b2_test_install/

    # Create work folder with appropriate name.
    release_folder_name='b2-%s'%options.name
    work_path=os.path.join(temp,release_folder_name)

    # Fill work folder. Copy everything except .git/ and build/.
    def copytree_ignore(path,contents):
        if path==options.g_working_copy_path: return ['.git','build']
        else: return []

    copytree(options.g_working_copy_path,
             work_path,
             symlinks=True,
             ignore=copytree_ignore)
    
    # Fix stuff up in place.
    with ChangeDirectory(work_path) as p:
        rmfiles('bin/*.exe')
        rmfiles('bin/*.bat')
        rmtree('etc/64tass-1.52.1237')
        rmtree('etc/ImageMagick-7.0.5-4-portable-Q16-x64')
        rmfiles('make.bat')
        rmfiles('Makefile')
        rmfiles('Makefile.osx.mak')
        rmfiles('Makefile.unix.mak')
        rmfiles('Makefile.windows.mak')
        rmtree('submodules/curl') # only used on Windows
        copyfile('etc/release/LICENCE.txt','LICENCE.txt')
        copyfile('etc/release/configure.py','configure')
        shutil.copymode('etc/release/configure.py','configure')
        if is_linux():
            # Remove the dependencies that are intended to be
            # supplied by the package manager. It all adds up!
            #
            # Don't do this on macOS, as these dependencies are
            # built from source.
            rmtree('submodules/libuv')
            rmtree('submodules/SDL_official')

            # Remove the release stuff.
            #
            # Don't do this on macOS, as the build process copies
            # files from this folder into the app bundle.
            rmtree('etc/release')

        # Fix up the docs a bit.
        fix_up_md('README.md',options)
        for md_path in glob.glob('doc/*.md'): fix_up_md(md_path,options)

        if options.name is not None:
            # Bake the release name into the configure script.
            with open('configure','rt') as f: text=f.read()
            new_text=text.replace("release_name=None",
                                  f'''release_name=r\'\'\'{options.name}\'\'\'''',
                                  1)
            assert new_text!=text
            with open('configure','wt') as f: f.write(new_text)

    # Set timestamps.
    if options.timestamp is not None:
        if not options.tar_mtime:
            with ChangeDirectory(temp):
                set_tree_timestamps(options.timestamp,release_folder_name)

    # Create tar file in the temp folder. Don't compress it until any
    # tests succeed.
    with ChangeDirectory(temp) as p:
        tar_name='b2-linux-source-%s.tar'%options.name

        argv=['tar',
              'cf',tar_name]
        
        if is_linux():
            # assume GNU tar.
            argv.append('--sort=name') # and why not...

            if options.timestamp is not None:
                if options.tar_mtime:
                    argv.append(f'--mtime={options.timestamp.isoformat()}')

        argv.append(release_folder_name)
        
        must_run_subprocess(argv,options)

    # Stop BeebLink being able to find the volumes that are lying
    # around in the temp folder.
    with ChangeDirectory(work_path) as p:
        # Stop BeebLink finding any of this stuff.
        with open('.beeblink-ignore','wb') as f: pass

    # Do any tests.
    if options.build or options.test or options.install:
        with ChangeDirectory(work_path) as p:
            if options.g_verbose: verbose_arg='VERBOSE=1'
            else: verbose_arg=None

            # all imply configure. Also, always build everything.
            must_run_subprocess(['./configure',
                                 '--build-b2',
                                 '--build-b2-with-debugger',
                                 '--prefix','../_b2_test_install',
                                 '--run-tests' if options.test else '--no-run-tests',
                                 '--ninja' if options.ninja else None],
                                options)

            # all imply build.
            must_run_subprocess(['make',
                                 verbose_arg],
                                options)

            if options.install:
                must_run_subprocess(['make',
                                     'install',
                                     verbose_arg],
                                    options)

    # Looks good.
    bz2_path=os.path.join(temp,tar_name+'.bz2')
    if not options.no_bzip2:
        with ChangeDirectory(temp) as p:
            must_run_subprocess(['bzip2','-9',tar_name],options)

        set_file_timestamps(options.timestamp,bz2_path)

    gh_release([bz2_path],options)

##########################################################################
##########################################################################

def release_binary_windows_cmd(options):
    if not is_windows(): fatal('only supported on Windows')

    build_folder=get_build_folder_path(options)

    with ChangeDirectory(options.g_working_copy_path):
        head_revision=get_head_revision()

    # TODO: more logic here.
    matrix=BuildMatrix()
    matrix.vs2022=True

    # Build stuff in build/release_binary_windowvs2022 (or similar).
    prefix='release_binary_windows'

    result=create_build_makefile(matrix,
                                 build_folder,
                                 prefix,
                                 options)

    makefile_basename=f'Makefile.{prefix}.mak'
    makedirs(build_folder)
    with open(os.path.join(build_folder,makefile_basename),'wt') as f:
        result.makefile.write(f)

    # If initing, clean all output paths first, to ensure it starts
    # from scratch. if reiniting, let it reuse any it finds.
    if options.init: clean_output_paths(build_folder,result.build_types)

    run_make(build_folder,
             makefile_basename,
             'init_all',
             options)

    pv(str(result.build_types))

    # Find RelWithDebInfo and Final configurations, and build/test as
    # required.
    build_type_by_configuration={}
    for build_type in result.build_types:
        assert build_type.configuration not in build_type_by_configuration,build_type.configuration
        build_type_by_configuration[build_type.configuration]=build_type
        if build_type.configuration in 'rf':
            # no need to clean: init doesn't take long, and it's
            # undesirable when --no-init specified.
            
            # run_make(build_folder,
            #          makefile_basename,
            #          build_type.clean_target.name,
            #          options)
            run_make(build_folder,
                     makefile_basename,
                     build_type.build_target.name,
                     options)
            if options.test: run_make(build_folder,
                                      makefile_basename,
                                      build_type.test_target.name,
                                      options)

    # Assemble zip contents in build/release_binary_windows_temp.
    temp_path=os.path.join(build_folder,'release_binary_windows_temp')
    rmtree(temp_path)
    makedirs(temp_path)
    
    zip_folder_path=os.path.join(temp_path,'b2')
    makedirs(zip_folder_path)

    b2_zip_path=os.path.join(temp_path,'b2-windows-%s.zip'%options.name)
    symbols_7z_path=os.path.join(temp_path,'symbols.b2-windows-%s.7z'%options.name)

    # pv(f'b2_zip_path: {b2_zip_path}\n')
    # pv(f'symbols_7z_path: {symbols_7z_path}\n')
    # pv(f'temp_path: {temp_path}\n')
    # pv(f'zip_folder_path: {zi_path}\n')

    with ChangeDirectory(zip_folder_path) as p:
        copyfile(p.relpath(os.path.join(build_folder,
                                        build_type_by_configuration['f'].output_path,
                                        "src/b2/b2/Final/b2.exe")),
                 "b2.exe")
        
        copyfile(p.relpath(os.path.join(build_folder,
                                        build_type_by_configuration['r'].output_path,
                                        "src/b2/b2/RelWithDebInfo/b2.exe")),
                 "b2_Debug.exe")
        
        copyfile(p.relpath(os.path.join(options.g_working_copy_path,
                                        "etc/release/LICENCE.txt")),
                 "LICENCE.txt")
        
        copyfile(p.relpath(os.path.join(build_folder,
                                        build_type_by_configuration['r'].output_path,
                                        "src/b2/b2/RelWithDebInfo/WinPixEventRuntime.dll")),
                 "WinPixEventRuntime.dll")
        
        create_binary_release_README(head_revision,options)

        copytree(p.relpath(os.path.join(build_folder,
                                        build_type_by_configuration['r'].output_path,
                                        "src/b2/b2/Final/assets")),
                 "assets")

        set_tree_timestamps(options.timestamp,'.')

    with ChangeDirectory(temp_path) as p:
        must_run_subprocess(['7z',
                             'a',
                             '-mx=9',
                             p.relpath(b2_zip_path),
                             'b2'],
                            options)

    set_file_timestamps(options.timestamp,b2_zip_path)

    with ChangeDirectory(temp_path) as p:
        copyfile(p.relpath(os.path.join(build_folder,
                                        build_type_by_configuration['r'].output_path,
                                        'src/b2/b2/RelWithDebInfo/b2.pdb')),
                 'b2 Debug.pdb')
        
        copyfile(p.relpath(os.path.join(build_folder,
                                        build_type_by_configuration['f'].output_path,
                                        'src/b2/b2/Final/b2.pdb')),
                 'b2.pdb')

        must_run_subprocess(['7z',
                             'a',
                             '-mx=9',
                             p.relpath(symbols_7z_path),
                             'b2 Debug.pdb',
                             'b2.pdb'],
                            options)

    gh_release([b2_zip_path,symbols_7z_path],options)

##########################################################################
##########################################################################

AppBundle=collections.namedtuple('AppBundle','configuration name')

def release_binary_macos_cmd(options):
    if not is_macos(): fatal('only supported on macOS')

    with ChangeDirectory(options.g_working_copy_path):
        head_revision=get_head_revision()

    arch=must_capture_subprocess(['uname','-m'],
                                 options)
    if arch=='x86_64': arch='intel'
    elif arch=='arm64': arch='applesilicon'
    else: fatal(f'''unknown architecture from uname -m: {arch}''')

    build_folder=get_build_folder_path(options)

    with ChangeDirectory(options.g_working_copy_path):
        head_revision=get_head_revision()

    matrix=BuildMatrix()
    matrix.unix_configurations+=['r','f']
    matrix.ffmpeg=options.ffmpeg
    matrix.osx_deployment_target=options.osx_deployment_target

    prefix='release_binary_macos.'

    # The temp folder always gets cleaned out before every build.
    temp_folder_path=os.path.join(build_folder,f'''{prefix}temp''')
    remakedirs(temp_folder_path)

    result=create_build_makefile(matrix,
                                 build_folder,
                                 prefix,
                                 options) 

    makefile_basename=f'Makefile.{prefix}mak'
    makedirs(build_folder)
    with open(os.path.join(build_folder,makefile_basename),'wt') as f:
        result.makefile.write(f)

    # If initing, clean all output paths first, to ensure it starts
    # from scratch. if reiniting, let it reuse any it finds.
    if options.init: clean_output_paths(build_folder,result.build_types)

    run_make(build_folder,
             makefile_basename,
             'init_all',
             options)

    # Building Unix-style, only requested configurations are present.
    # So building everything covers only RelWithDebInfo and Final, as
    # desired.
    #
    # Run dylibbundler from the output path too.
    #
    # Test too, if required.
    build_type_by_configuration={}
    for build_type in result.build_types:
        build_type_by_configuration[build_type.configuration]=build_type
        run_make(build_folder,
                 makefile_basename,
                 build_type.build_target.name,
                 options)
        output_path=os.path.join(build_folder,build_type.output_path)
        with ChangeDirectory(output_path) as p:
            if build_type.configuration=='r':
                info_plist_path='src/b2/b2/b2.app/Contents/Info.plist'
                output=must_capture_subprocess(['/usr/libexec/PlistBuddy',
                                                '-c',
                                                'print CFBundleIdentifier',
                                                info_plist_path],
                                               options,
                                               encoding='utf-8')
                identifier=output.splitlines()[0].strip()
                must_run_subprocess(['/usr/libexec/PlistBuddy',
                                     '-c',
                                     f'''set CFBundleIdentifier {identifier}-debug''',
                                     info_plist_path],
                                    options)

            must_run_subprocess(
                ['./submodules/macdylibbundler/dylibbundler',
                 '--create-dir',
                 '--bundle-deps',
                 '--fix-file','src/b2/b2/b2.app/Contents/MacOS/b2',
                 '--dest-dir','src/b2/b2/b2.app/Contents/libs/'],
                options)
                
        if options.test: run_make(build_folder,
                                  makefile_basename,
                                  build_type.test_target.name,
                                  options)

    # Form DMG name stem.
    stem='b2-macos-'
    if options.osx_deployment_target is not None:
        stem+=options.osx_deployment_target+'-'
    stem+=f'''{arch}-{options.name}'''

    # Form various paths.
    temp_dmg=os.path.join(temp_folder_path,f'''{stem}_temp.dmg''')
    final_dmg=os.path.join(temp_folder_path,f'''{stem}.dmg''')
    symbols_temp_folder_path=os.path.join(temp_folder_path,'symbols_temp')
    symbols_zip_path=os.path.join(temp_folder_path,f'''symbols.{stem}.7z''')

    # TODO: Can't remember why this is its own thing?
    mount=final_dmg

    # Copy template DMG to temp DMG.
    copyfile(os.path.join(options.g_working_copy_path,
                          'etc/release/template.dmg'),
             temp_dmg)

    # Resize temp DMG.
    must_run_subprocess(['hdiutil','resize','-size','500m',temp_dmg],
                        options)

    # Mount temp DMG.
    must_run_subprocess(['hdiutil','attach',temp_dmg,'-mountpoint',mount],
                        options)


    # Copy each app bundle to the appropriate place. Verify it with
    # codesign (why not...), and use dsymutil+ditto to move the .dSYM
    # folder so it can be archived.
    app_bundles=[AppBundle(configuration='r',name='b2 Debug'),
                 AppBundle(configuration='f',name='b2')]

    try:
        copyfile(os.path.join(options.g_working_copy_path,
                              'etc/release/LICENCE.txt'),
                 os.path.join(mount,'LICENCE.txt'))

        with ChangeDirectory(mount):
            create_binary_release_README(head_revision,options)

        for bundle in app_bundles:
            src_app_path=os.path.join(build_folder,
                                      build_type_by_configuration[bundle.configuration].output_path,
                                      'src/b2/b2/b2.app')

            dest_app_path=os.path.join(mount,f'''{bundle.name}.app''')
            
            must_run_subprocess(['ditto',src_app_path,dest_app_path],
                                options)

            must_run_subprocess(['codesign','-v',dest_app_path],
                                options)

            must_run_subprocess(['dsymutil',
                                 os.path.join(src_app_path,
                                              'Contents/MacOS/b2')],
                                options)

            dsym_path=os.path.join(src_app_path,
                                   'Contents/MacOS/b2.dSYM')

            must_run_subprocess(['ditto',
                                 dsym_path,
                                 os.path.join(symbols_temp_folder_path,
                                              f'''{bundle.name}.dSYM''')],
                                options)

            # don't leave the .dSYM lying around, in case a second
            # build is run with --no-init. This doesn't matter so much
            # for correctness (--no-init does not create valid
            # releases), but they are quite large and might exhaust
            # the dmg capacity.
            rmtree(dsym_path)

        set_tree_timestamps(options.timestamp,
                            mount)

        must_run_subprocess(['diskutil','rename',mount,stem],
                            options)
    finally:
        # Unmount temp DMG.
        must_run_subprocess(['hdiutil','detach',mount],
                            options)

    # Convert temp DMG into final DMG, set timestamps, and remove temp
    # DMG.
    must_run_subprocess(['hdiutil',
                         'convert',
                         temp_dmg,
                         '-format','UDBZ',
                         '-o',final_dmg],
                        options)
    rm(temp_dmg)
    
    set_file_timestamps(options.timestamp,
                        final_dmg)

    # Assemble symbols zip.
    with ChangeDirectory(symbols_temp_folder_path) as p:
        must_run_subprocess(['7z',
                             'a',
                             '-mx=9',
                             p.relpath(symbols_zip_path),
                             '*'],
                            options)

    gh_release([final_dmg,symbols_zip_path],options)

##########################################################################
##########################################################################

def main(argv):
    def auto_int(x): return int(x,0)
    def timestamp(x): return datetime.datetime.fromisoformat(x)

    global g_local_osx_deployment_target
    if is_macos():
        try: g_local_osx_deployment_target=subprocess.check_output(['sw_vers','-productVersion'],text='utf-8').rstrip()
        except CalledProcessError: pass
    
    parser=argparse.ArgumentParser(description='''b2 build automation tool''',
                                   epilog='''Commands with names starting with _ are for internal use''')
    parser.set_defaults(fun=None)

    parser.add_argument('-v','--verbose',dest='g_verbose',action='store_true',help='''be more verbose''')
    parser.add_argument('-j',type=auto_int,metavar='N',dest='g_max_jobs',default=os.cpu_count(),help='''run up to %(metavar)s job(s) at once. Default: %(default)s''')
    parser.add_argument('-w','--working-copy',dest='g_working_copy_path',metavar='PATH',default='.',help='''specify root of b2 working copy. Default: %(default)s''')
    parser.add_argument('--make',dest='g_make_path',default='make',metavar='PATH',help='''use %(metavar)s as path to GNU Make on Linux/macOS. (On Windows, the repo's copy is always used.) Default: %(default)s''')
    # parser.add_argument('--ignore-submake',dest='g_ignore_submake',action='store_true',help='''if run from GNU Make, don't do anything special. If running further copies of GNU Make, still pass them -j''')

    subparsers=parser.add_subparsers()

    def add_subparser(name,fun,**kwargs):
        subparser=subparsers.add_parser(name,**kwargs)
        subparser.set_defaults(fun=fun)
        return subparser

    def add_common_init_options(subparser):
        subparser.add_argument('--name',metavar='STRING',help='''use %(metavar)s as the build name''')

    def add_common_target_options(subparser):
        subparser.add_argument('--unix',action='store_true',help='''initialise Unix-style build''')
        subparser.add_argument('--vs2022',action='store_true',help='''initialise VS2022 build''')
        subparser.add_argument('--vs2022-architecture',metavar='ARCH',default=None,help='''initialise VS2022 build for architecture %(metavar)s, one of x64 or ARM. A default will be chosen if not specified''')
        subparser.add_argument('--xcode',action='store_true',help='''initialise Xcode build''')
        subparser.add_argument('--enable-sanitizers',action='store_true',help='''if building Unix-style, try to use any supported sanitizers''')

    def add_macos_specific_target_options(subparser):
        subparser.add_argument('--osx-deployment-target',metavar='TARGET',default=g_local_osx_deployment_target,help='''specify macOS deployment target'''+('' if g_local_osx_deployment_target is None else ' Default: %s'%g_local_osx_deployment_target))

    init_subparser=add_subparser('init',init_cmd,help='''initialise build''')
    init_subparser.add_argument('--cc',metavar='NAME',help='''if building Unix-style, use %(metavar)s as C compiler''')
    init_subparser.add_argument('--cxx',metavar='NAME',help='''if building Unix-style, use %(metavar)s as C++ compiler''')
    add_common_init_options(init_subparser)
    add_common_target_options(init_subparser)
    add_macos_specific_target_options(init_subparser)
    init_subparser.add_argument('--reinit',action='store_true',help='''reinit when output folder exists''')
    init_subparser.add_argument('--prefix',metavar='STRING',help='''prepend %(metavar)s to name of any build folder created''')

    _init_xcode_subparser=add_subparser('_init_xcode',_init_xcode_cmd,help='''initialise Xcode build''')
    add_common_init_options(_init_xcode_subparser)
    add_macos_specific_target_options(_init_xcode_subparser)
    _init_xcode_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')

    _init_unix_subparser=add_subparser('_init_unix',_init_unix_cmd,help='''initialise Unix build''')
    add_common_init_options(_init_unix_subparser)
    add_macos_specific_target_options(_init_unix_subparser)
    _init_unix_subparser.add_argument('--sanitizer',help='''specify sanitizer: '''+'; '.join(['%s (%s)'%(k,v.friendly_name) for k,v in UNIX_SANITIZER_TYPES.items()]))
    _init_unix_subparser.add_argument('build',help='''specify build configuration: '''+'; '.join(['%s (%s)'%(k,v) for k,v in CMAKE_CONFIGURATIONS.items()]))
    _init_unix_subparser.add_argument('--keep',action='store_true',help='''don't delete build folder if init fails''')
    _init_unix_subparser.add_argument('--cc',metavar='NAME',help='''use %(metavar)s as C compiler''')
    _init_unix_subparser.add_argument('--cxx',metavar='NAME',help='''use %(metavar)s as C++ compiler''')
    _init_unix_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')
    _init_unix_subparser.add_argument('--no-ffmpeg',dest='ffmpeg',action='store_false',help='''don't look for ffmpeg''')

    _init_vs_subparser=add_subparser('_init_vs',_init_vs_cmd,help='''initialise Visual Studio build''')
    _init_vs_subparser.add_argument('--architecture',metavar='ARCH',default=None,help='''configure for architecture %(metavar)s. A default will be chosen if not specified''')
    _init_vs_subparser.add_argument('version',type=auto_int,help='''specify Visual Studio version''')
    _init_vs_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')
    add_common_init_options(_init_vs_subparser)

    batch_subparser=add_subparser('batch',batch_cmd,help='''do batch builds/tests''')
    batch_subparser.add_argument('--cc-cxx',metavar='CC CXX',nargs=2,action='append',dest='compilers',default=[],help='''use %(metavar)s as C and C++ compiler respectively for Unix builds. Can specifiy multiple times''')
    add_common_init_options(batch_subparser)
    # don't bother adding macOS-specific target options. Batch builds
    # only have to be good enough to run on the local system.
    batch_subparser.add_argument('--no-init',dest='init',action='store_false',help='''don't init (unless obviously required)''')
    batch_subparser.add_argument('--no-clean',dest='clean',action='store_false',help='''don't clean before building''')
    batch_subparser.add_argument('--no-test',dest='test',action='store_false',help='''don't run tests after building''')
    batch_subparser.add_argument('--prefix',metavar='STRING',help='''prepend %(metavar)s to name of any build folder created''')

    print_build_suffix_subparser=add_subparser('print-build-suffix',print_build_suffix_cmd,help='''print build suffix: path-friendly date, time and hash of head commit. (Date and time are in UTC)''')

    print_build_timestamp_parser=add_subparser('print-build-timestamp',print_build_timestamp_cmd,help='''print build timestamp: UTC date and time of head commit, in ISO format''')

    set_submodule_upstreams_parser=add_subparser('set-submodule-upstreams',set_submodule_upstreams_cmd,help='''set upstream remotes for b2 submodules''')

    def add_common_release_options(subparser):
        subparser.add_argument('--timestamp',metavar='TIMESTAMP',dest='timestamp',default=None,type=timestamp,help='''set files' atime/mtime to %(metavar)s. Format must be ISO8601 as output by print-build-timestamp''')
        subparser.add_argument('--gh-release',action='store_true',help='''create GitHub release (or prerelease if not on master branch) and upload artefacts''')

    release_source_linux_subparser=add_subparser('release-source-linux',release_source_linux_cmd,help='''make Linux source code release''',epilog='''Not functional on Windows. Unsupported on macOS''')
    add_common_release_options(release_source_linux_subparser)
    release_source_linux_subparser.add_argument('name',help='''name for build''')
    release_source_linux_subparser.add_argument('--build',action='store_true',help='''do a test build (process will fail if build fails)''')
    release_source_linux_subparser.add_argument('--test',action='store_true',help='''run tests after creating the archive (implies --build) (process will fail if tests fail)''')
    release_source_linux_subparser.add_argument('--install',action='store_true',help='''do a test install (implies --build) (process will fail if install fails)''')
    release_source_linux_subparser.add_argument('--ninja',action='store_true',help='''if doing a build, use Ninja rather than GNU Make''')
    release_source_linux_subparser.add_argument('--tar-mtime',action='store_true',help='''if running on Linux, use tar --mtime to update file timestamps in archive''')
    release_source_linux_subparser.add_argument('--no-bzip2',action='store_true',help='''don't bother bzip2'ing the tar file''')
    
    release_binary_windows_subparser=add_subparser('release-binary-windows',release_binary_windows_cmd,help='''make Windows binary release''')
    release_binary_windows_subparser.add_argument('name',help='''name for build''')
    release_binary_windows_subparser.add_argument('--no-init',dest='init',action='store_false',help='''don't init (unless obviously required)''')
    release_binary_windows_subparser.add_argument('--no-test',action='store_false',dest='test',help='''don't run tests''')
    add_common_release_options(release_binary_windows_subparser)

    release_binary_macos_subparser=add_subparser('release-binary-macos',release_binary_macos_cmd,help='''make macOS binary release''')
    release_binary_macos_subparser.add_argument('name',help='''name for build''')
    release_binary_macos_subparser.add_argument('--no-init',dest='init',action='store_false',help='''don't init (unless obviously required)''')
    release_binary_macos_subparser.add_argument('--no-test',action='store_false',dest='test',help='''don't run tests''')
    add_common_release_options(release_binary_macos_subparser)
    add_macos_specific_target_options(release_binary_macos_subparser)
    release_binary_macos_subparser.add_argument('--no-ffmpeg',dest='ffmpeg',action='store_false',help='''don't try to find ffmpeg''')
    
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
