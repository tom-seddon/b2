#!/usr/bin/python3
import sys,os,os.path,argparse,subprocess,shutil,shlex,collections,tempfile,glob

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

def rmfiles(pattern):
    paths=glob.glob(pattern)
    for path in paths: os.unlink(path)

##########################################################################
##########################################################################

def set_file_timestamps(timestamp,fname):
    if timestamp is None: return

    if os.path.islink(fname):
        # There's a link to /Applications in the dmg, and this is a
        # lame way of avoiding touching it.
        return

    t=time.mktime(timestamp.timetuple())

    try:
        os.utime(fname,(t,t))
    except:
        print("WARNING: failed to set timestamps for: %s"%fname,file=sys.stderr)
        pass

##########################################################################
##########################################################################
    
def set_tree_timestamps(timestamp,root):
    for dirpath,dirnames,filenames in os.walk(root):
        for f in dirnames+filenames: 
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

def run_subprocess(argv,options,**other_popen_kwargs):
    argv=[arg for arg in argv if arg is not None]
    
    if g_verbose:
        suffix='(cwd: %s): %s'%(os.getcwd(),get_copyable_argv(argv))
        print(f'b2build running   {suffix}')

    process=subprocess.Popen(argv,**other_popen_kwargs)
    process.wait()

    if g_verbose:
        print(f'b2build completed {suffix} - exit code: {process.returncode}')
    
    return process

def must_run_subprocess(argv,options,**other_popen_kwargs):
    result=run_subprocess(argv,options,**other_popen_kwargs)

    if result.returncode!=0:
        fatal('failed with return code %d: %s'%(result.returncode,get_copyable_argv(argv)))

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
        if options.osx_deployment_target is not None:
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
        self.unix_configurations=[]
        self.unix_sanitizers=[]
        self.unix_compilers=[]

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

BuildType=collections.namedtuple('BuildType','configuration compiler sanitizer init_target clean_target build_target test_target output_path')

CreateBuildMakefileResult=collections.namedtuple('CreateBuildMakefileResult','makefile build_types')

def create_build_makefile(matrix,
                          build_folder,
                          prefix,
                          options):
    global_options=' '
    global_options+=' $(if $(VERBOSE),--verbose,)'
    global_options+=' --working-copy "%s"'%(os.path.relpath(options.g_working_copy_path,build_folder))

    cmd_options=' '
    # cmd_options+=get_optional_option('--prefix',prefix)
    cmd_options+=get_optional_option('--name',options.name)
    if is_macos():
        cmd_options+=get_optional_option('--osx-deployment-target',options.osx_deployment_target)
    
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
                line+=' _init_unix '
                line+=get_optional_option('--sanitizer',sanitizer)
                # line+=get_optional_option('--prefix',prefix) # TODO: should use cmd_options here??
                if compiler is not None:
                    line+=' --cc "%s"'%compiler.cc
                    line+=' --cxx "%s"'%compiler.cxx
                line+=' %s'%configuration

                line+=' "%s"'%output_path

                init_target.add_line(line)

                # Add clean target
                clean_target=makefile.add_named_target('clean_unix_%s'%target_name_suffix)
                clean_target.add_line(f'''$(_V)cd "{output_path}" && ninja clean''')

                # Add build target.
                build_target=makefile.add_named_target('build_unix_%s'%target_name_suffix)
                build_target.add_line(f'''$(_V)cd "{output_path}" && ninja {j_option}''')

                # Add test target.
                test_target=makefile.add_named_target('test_unix_%s'%target_name_suffix)
                test_target.add_line(f'''$(_V)cd "{output_path}" && ctest --progress {j_option}''')
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

        target.add_line(f'''$(_V)$(PYTHON) "{b2build_py_path}"{global_options} _init_xcode {cmd_options} {get_output_path('Xcode')}''')

        build_types.append(
            BuildType(configuration=None,
                      compiler='Xcode',
                      sanitizer=None,
                      init_target=target,
                      clean_target=None,
                      build_target=None,
                      test_target=None))

    def add_visual_studio_targets(vsver):
        vs_stuff=get_vs_stuff(17)

        output_path=get_output_path(f'vs{vs_stuff.year}')
        
        #init_target=makefile.add_named_target(f'init_vs{vs_stuff.year}')
        init_target=makefile.add_named_target(output_path,phony=False)

        def get_msbuild_bat_path(caller_path):
            return os.path.relpath(
                os.path.join(options.g_working_copy_path,
                             'bin/msbuild_bug_wrapper.bat'),
                caller_path)

        init_target.add_line(f'''$(_V)"{get_msbuild_bat_path(build_folder)}" $(PYTHON) "{b2build_py_path}" {global_options} _init_vs {cmd_options} {vsver} "{output_path}"''')

        for configuration,cmake_build_type in CMAKE_CONFIGURATIONS.items():
            build_target=makefile.add_named_target(f'build_vs{vs_stuff.year}{configuration}')
            msbuild_bat=get_msbuild_bat_path(os.path.join(build_folder,
                                                          output_path))
            
            build_target.add_line(f'''$(_V)cd "{output_path}" && "{msbuild_bat}" "{vs_stuff.devenv_path}" b2.sln /Build "{cmake_build_type}"''')

            clean_target=makefile.add_named_target(f'clean_vs{vs_stuff.year}{configuration}')
            clean_target.add_line(f'''$(_V)cd "{output_path}" && "{msbuild_bat}" "{vs_stuff.devenv_path}" b2.sln /Clean "{cmake_build_type}"''')

            test_target=makefile.add_named_target(f'test_vs{vs_stuff.year}{configuration}')
            test_target.add_line(f'''$(_V)cd "{output_path}" && "{vs_stuff.ctest_path}" {j_option} -C "{cmake_build_type}" --progress''')

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
    matrix.xcode=options.xcode

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
    
    cmake_configuration=CMAKE_CONFIGURATIONS.get(options.build)
    if cmake_configuration is None:
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
        argv+=['-DCMAKE_BUILD_TYPE=%s'%cmake_configuration]
        argv+=['-S',os.path.relpath(options.g_working_copy_path,
                                    options.output_path)]
        argv+=['-B','.']
        ret=run_subprocess(argv,options,close_fds=False)
        if ret.returncode!=0:
            rmtree(unix_folder)
            fatal('init failed')

##########################################################################
##########################################################################

def _init_vs_cmd(options):
    vs_stuff=get_vs_stuff(options.version)

    rmtree(options.output_path)
    makedirs(options.output_path)

    with ChangeDirectory(options.output_path):
        argv=[vs_stuff.cmake_path,'-G',f'Visual Studio {options.version} {vs_stuff.year}']
        argv+=get_cmake_defines(options)
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
        matrix.unix_configurations+=[t for t in CMAKE_CONFIGURATIONS.keys()]

        if len(options.compilers)>0:
            for compiler in options.compilers:
                matrix.unix_compilers.append(UnixCompiler(cc=compiler[0],
                                                          cxx=compiler[1]))

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

def release_source_linux_cmd(options):
    if is_windows(): fatal('not supported on Windows')
    
    def run(temp):
        # temp folder contents during process:
        #
        # _b2-_pass1.tar
        # b2-<<name>>/
        # _b2_test_install/
        # <<output file basename>>
        #
        # You could name things just so, and make a mess - or not.
        
        # Prepare tar file with contents of interest.
        pass1_tar_path=os.path.join(temp,'_b2-pass1.tar')
        with ChangeDirectory(options.g_working_copy_path):
            paths=glob.glob('*')

            i=0
            while i<len(paths):
                if paths[i]=='build' and os.path.isdir(paths[i]): del paths[i]
                else: i+=1

            must_run_subprocess(['7z',
                                 'a',
                                 os.path.relpath(pass1_tar_path,
                                                 options.g_working_copy_path)]+
                                paths,options)

        # Create work folder with appropriate name.
        release_folder_name='b2-%s'%options.name
        work_path=os.path.join(temp,release_folder_name)
        makedirs(work_path)

        # Extract contents of interest to work folder.
        with ChangeDirectory(work_path):
            must_run_subprocess(['7z','x',os.path.relpath(pass1_tar_path,
                                                          work_path)],
                                options)

        # Fix stuff up in place.
        with ChangeDirectory(work_path):
            rmfiles('bin/*.exe')
            rmfiles('bin/*.bat')
            rmtree('etc/64tass-1.52.1237')
            rmtree('etc/ImageMagick-7.0.5.4-portable-Q16-x64')
            rmfiles('make.bat')
            rmfiles('Makefile')
            rmfiles('Makefile.osx.mak')
            rmfiles('Makefile.unix.mak')
            rmfiles('Makefile.windows.mak')
            shutil.copyfile('etc/release/Makefile.release.mak','Makefile')
            rmtree('submodules/curl') # only used on Windows
            
            if is_linux():
                # Remove the dependencies that are intended to be
                # supplied by the package manager. It all adds up!
                #
                # Don't do this on macOS, as these dependencies are
                # built from source.
                rmtree('submodules/libuv')
                rmtree('submodules/SDL_official')

        # Set timestamps.
        if options.timestamp is not None:
            with ChangeDirectory(work_path):
                set_tree_timestamps(options.timestamp,'.')

        # Create final tar file in the temp folder.
        if options.output_path is not None:
            with ChangeDirectory(temp):
                must_run_subprocess(['7z',
                                     'a',
                                     '-mx=9',
                                     os.path.basename(options.output_path),
                                     release_folder_name],
                                    options)

        # Do any tests.
        if options.build or options.test or options.install:
            with ChangeDirectory(work_path):
                if options.g_verbose: verbose_arg='VERBOSE=1'
                else: verbose_arg=None
                
                # all imply configure.
                must_run_subprocess(['make','configure',verbose_arg],
                                    options)

                # all imply build, possibly with test.
                must_run_subprocess(['make',
                                     'build',
                                     verbose_arg,
                                     'RUN_TESTS=1' if options.test else None],
                                    options)

                if options.install:
                    must_run_subprocess(['make',
                                         'install',
                                         verbose_arg,
                                         'PREFIX=../_b2_test_install'],
                                        options)

        # Copy the archive.
        if options.output_path is not None:
            shutil.copyfile(os.path.join(temp,
                                         os.path.basename(options.output_path)),
                            options.output_path)

    if options.temp is None:
        with tempfile.TemporaryDirectory() as temp: run(temp)
    else:
        rmtree(options.temp)
        makedirs(options.temp)
        run(options.temp)

##########################################################################
##########################################################################

def release_binary_windows_cmd(options):
    if not is_windows(): fatal('only supported on Windows')

    build_folder=get_build_folder_path(options)

    # TODO: more logic here.
    matrix=BuildMatrix()
    matrix.vs2022=True

    prefix='release_binary_windows'

    result=create_build_makefile(matrix,
                                 build_folder,
                                 prefix,
                                 options)

    makefile_basename=f'Makefile.{prefix}.mak'
    makedirs(build_folder)
    with open(os.path.join(build_folder,makefile_basename),'wt') as f:
        result.makefile.write(f)

    configurations=['r','f']

    if options.init: clean_output_paths(build_folder,result.build_types)

    run_make(build_folder,
             makefile_basename,
             'init_all',
             options)

    for build_type in result.build_types:
        if build_type.configuration in configurations:
            run_make(build_folder,
                     makefile_basename,
                     build_type.clean_target.name,
                     options)
            run_make(build_folder,
                     makefile_basename,
                     build_type.build_target.name,
                     options)
            if options.test: run_make(build_folder,
                                      makefile_basename,
                                      build_type.test_target.name,
                                      options)
    
##########################################################################
##########################################################################

def main(argv):
    def auto_int(x): return int(x,0)
    def timestamp(x): return datetime.datetime.strptime(x,"%Y%m%d-%H%M%S")

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
    # parser.add_argument('--ignore-submake',dest='g_ignore_submake',action='store_true',help='''if run from GNU Make, don't do anything special. If running further copies of GNU Make, still pass them -j''')

    subparsers=parser.add_subparsers()

    def add_subparser(name,fun,**kwargs):
        subparser=subparsers.add_parser(name,**kwargs)
        subparser.set_defaults(fun=fun)
        return subparser

    def add_common_init_options(subparser):
        subparser.add_argument('--osx-deployment-target',metavar='TARGET',default=default_osx_deployment_target,help='''specify macOS deployment target'''+('' if default_osx_deployment_target is None else ' Default: %s'%default_osx_deployment_target))
        subparser.add_argument('--name',metavar='STRING',help='''use %(metavar)s as the build name''')

    def add_common_target_options(subparser):
        subparser.add_argument('--unix',action='store_true',help='''initialise Unix-style build''')
        subparser.add_argument('--vs2022',action='store_true',help='''initialise VS2022 build''')
        subparser.add_argument('--xcode',action='store_true',help='''initialise Xcode build''')
        subparser.add_argument('--enable-sanitizers',action='store_true',help='''if building Unix-style, try to use any supported sanitizers''')

    init_subparser=add_subparser('init',init_cmd,help='''initialise build''')
    init_subparser.add_argument('--cc',metavar='NAME',help='''if building Unix-style, use %(metavar)s as C compiler''')
    init_subparser.add_argument('--cxx',metavar='NAME',help='''if building Unix-style, use %(metavar)s as C++ compiler''')
    add_common_init_options(init_subparser)
    add_common_target_options(init_subparser)
    init_subparser.add_argument('--reinit',action='store_true',help='''reinit when output folder exists''')
    init_subparser.add_argument('--prefix',metavar='STRING',help='''prepend %(metavar)s to name of any build folder created''')

    _init_xcode_subparser=add_subparser('_init_xcode',_init_xcode_cmd,help='''initialise Xcode build''')
    add_common_init_options(_init_xcode_subparser)
    _init_xcode_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')

    _init_unix_subparser=add_subparser('_init_unix',_init_unix_cmd,help='''initialise Unix build''')
    add_common_init_options(_init_unix_subparser)
    _init_unix_subparser.add_argument('--sanitizer',help='''specify sanitizer: '''+'; '.join(['%s (%s)'%(k,v.friendly_name) for k,v in UNIX_SANITIZER_TYPES.items()]))
    _init_unix_subparser.add_argument('build',help='''specify build configuration: '''+'; '.join(['%s (%s)'%(k,v) for k,v in CMAKE_CONFIGURATIONS.items()]))
    _init_unix_subparser.add_argument('--keep',action='store_true',help='''don't delete build folder if init fails''')
    _init_unix_subparser.add_argument('--cc',metavar='NAME',help='''use %(metavar)s as C compiler''')
    _init_unix_subparser.add_argument('--cxx',metavar='NAME',help='''use %(metavar)s as C++ compiler''')
    _init_unix_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')

    _init_vs_subparser=add_subparser('_init_vs',_init_vs_cmd,help='''initialise Visual Studio build''')
    _init_vs_subparser.add_argument('version',type=auto_int,help='''specify Visual Studio version''')
    _init_vs_subparser.add_argument('output_path',metavar='PATH',help='''put output in %(metavar)s (will be deleted first, no questions asked)''')
    add_common_init_options(_init_vs_subparser)

    batch_subparser=add_subparser('batch',batch_cmd,help='''do batch builds/tests''')
    batch_subparser.add_argument('--cc-cxx',metavar='CC CXX',nargs=2,action='append',dest='compilers',default=[],help='''use %(metavar)s as C and C++ compiler respectively for Unix builds. Can specifiy multiple times''')
    add_common_init_options(batch_subparser)
    batch_subparser.add_argument('--no-init',dest='init',action='store_false',help='''don't init (unless obviously required)''')
    batch_subparser.add_argument('--no-clean',dest='clean',action='store_false',help='''don't clean before building''')
    batch_subparser.add_argument('--no-test',dest='test',action='store_false',help='''don't run tests after building''')
    batch_subparser.add_argument('--prefix',metavar='STRING',help='''prepend %(metavar)s to name of any build folder created''')

    print_build_suffix_subparser=add_subparser('print-build-suffix',print_build_suffix_cmd,help='''print build suffix: time, date and hash of head commit''')

    print_build_timestamp_parser=add_subparser('print-build-timestamp',print_build_timestamp_cmd,help='''print build timestamp: time and date of head commit''')

    set_submodule_upstreams_parser=add_subparser('set-submodule-upstreams',set_submodule_upstreams_cmd,help='''set upstream remotes for b2 submodules''')

    def add_common_release_options(subparser):
        subparser.add_argument('--timestamp',metavar='TIMESTAMP',dest='timestamp',default=None,type=timestamp,help='''set files' atime/mtime to %(metavar)s. Format must be YYYYMMDD-HHMMSS''')

    release_source_linux_subparser=add_subparser('release-source-linux',release_source_linux_cmd,help='''make Linux source code release''',epilog='''Not functional on Windows. Unsupported on macOS''')
    add_common_release_options(release_source_linux_subparser)
    release_source_linux_subparser.add_argument('-o',metavar='FILE',dest='output_path',help='''write output file to %(metavar)s. Format can be anything 7z can create''')
    release_source_linux_subparser.add_argument('name',help='''name for build''')
    release_source_linux_subparser.add_argument('--build',action='store_true',help=''''do a test build (process will fail if build fails)''')
    release_source_linux_subparser.add_argument('--test',action='store_true',help=''''run tests after creating the archive (implies --build) (process will fail if tests fail)''')
    release_source_linux_subparser.add_argument('--install',action='store_true',help='''do a test install (implies --build) (process will fail if install fails)''')
    release_source_linux_subparser.add_argument('--temp',metavar='PATH',default=None,help='''use %(metavar)s as temp folder. If specified, will recreate if required, then leave as-is at end of build; if not specified, will create a temp folder and delete at end of build.''')

    release_binary_windows_subparser=add_subparser('release-binary-windows',release_binary_windows_cmd,help='''make Windows binary release''')
    release_binary_windows_subparser.add_argument('-o',metavar='FILE',dest='output_path',help='''write output file to %(metavar)s. Format can be anything 7z can create''')
    release_binary_windows_subparser.add_argument('name',help='''name for build''')
    release_binary_windows_subparser.add_argument('--no-init',dest='init',action='store_false',help='''don't init (unless obviously required)''')
    release_binary_windows_subparser.add_argument('--no-test',action='store_false',dest='test',help='''don't run tests''')
    add_common_release_options(release_binary_windows_subparser)

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
