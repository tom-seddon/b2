#!/usr/bin/python3
import sys,os,os.path,argparse,shutil,subprocess

##########################################################################
##########################################################################

release_name=None

##########################################################################
##########################################################################

def fatal(msg):
    sys.stderr.write(f'''FATAL: {msg}\n''')
    sys.exit(1)

def rmtree(path):
    if os.path.isfile(path): os.unlink(path)
    if os.path.isdir(path): shutil.rmtree(path)

##########################################################################
##########################################################################

def is_macos(): return sys.platform=='darwin'
def is_windows(): return sys.platform=='win32'
def is_linux(): return sys.platform=='linux'
def is_unix(): return is_macos() or is_linux()

##########################################################################
##########################################################################

# build folder for all the build stuff, config CONFIG.
def get_build_path(config):
    if is_macos(): name='osx'
    else: name='linux'
    return os.path.join('build',f'{config}.{name}.release')

# build folder for b2, config CONFIG.
def get_b2_build_path(config):
    return os.path.join(get_build_path(config),'src/b2/b2')

# path to the b2 executable specifically, config CONFIG.
def get_b2_exe_path(config):
    return os.path.join(get_b2_build_path(config),'b2')

def main2(options):
    if not options.build_b2 and not options.build_b2_with_debugger:
        fatal('nothing to build')

    def configure(config,name):
        build_path=get_build_path(config)
        rmtree(build_path)
        argv=['cmake',
              '-S','.',
              '-B',build_path,
              '-G','Ninja' if options.ninja else 'Unix Makefiles',
              f'''-DCMAKE_BUILD_TYPE={name}''']
        if release_name is not None:
            argv+=[f'''-DRELEASE_NAME={release_name}''']
        if is_macos():
            # work around some CMake versions choosing to build
            # against the latest macOS SDK available, rather than the
            # latest one the local system can run.
            productVersion=subprocess.check_output(['sw_vers','-productVersion'],encoding='utf-8')
            cmake_configure_options+=[f'''-DCMAKE_OSX_DEPLOYMENT_TARGET=${productVersion}''']

        result=subprocess.run(argv)
        if result.returncode!=0:
            rmtree(build_path)
            fatal(f'''failed to configure {name}''')

    if options.build_b2: configure('f','Final')
    if options.build_b2_with_debugger: configure('r','RelWithDebInfo')

    with open('Makefile','wt') as f:
        def w(x): f.write(x)
        def wn(x):
            w(x)
            w('\n')
            
        wn('MAKEFLAGS+=--no-print-directory')

        if is_macos():
            # Source distributions are not intended for use on macOS
            # either, but it hangs together just enough to let me use
            # it for testing purposes...
        
            wn('NPROC:=$(shell sysctl -n hw.cpu)')
        else:
            wn('NPROC:=$(shell nproc)')

        wn('_V:=$(if $(VERBOSE),,@)')

        wn('export VERBOSE')
        wn('export NPROC')

        # where to copy stuff to
        bin_path=os.path.join(options.prefix,'bin')
        share_path=os.path.join(options.prefix,'share','b2')

        # an arbitrary available config, which will be used to copy
        # the assets from (since every build is the same)
        available_config='f' if options.build_b2 else 'r'

        def build(config):
            build_path=get_build_path(config)

            if options.ninja: build_command='ninja'
            else: build_command='$(MAKE)'
            wn(f'''\t$(_V)cd "{build_path}" && {build_command} -j $(NPROC)''')
            if options.run_tests:
                wn(f'''\t$(_V)cd "{build_path}" && ctest $(if $(VERBOSE),--output-on-failure,) --progress -j $(NPROC)''')

        def test(config,name):
            wn(f'''\t$(_V)test -f "{get_b2_exe_path(config)}" || (echo {name} not built && false)''')

        def copy(config,exe_name):
            wn(f'''\t$(_V)cp -v "{get_b2_exe_path(config)}" "{os.path.join(bin_path,exe_name)}"''')
        
        wn('.PHONY:build')
        wn('build:')
        if options.build_b2: build('f')
        if options.build_b2_with_debugger: build('r')

        if not is_macos():
            wn('.PHONY:install')
            wn('install:')
            if options.build_b2: test('f','b2')
            if options.build_b2_with_debugger: test('r','b2 with debugger')
            wn(f'''\t$(_V)mkdir -p "{bin_path}" "{share_path}"''')
            if options.build_b2: copy('f','b2')
            if options.build_b2_with_debugger: copy('r','b2-debug')
            wn(f'''\t$(_V)cp -Rv "{os.path.join(get_b2_build_path(available_config),'assets')}" "{share_path}"''')
    
##########################################################################
##########################################################################

def main(argv):
    if not is_unix: fatal('not supported on Windows')

    if os.path.isfile('Makefile.unix.mak'):
        # this error message is mainly for my benefit.
        fatal('''don't run this from the b2 working copy...!''')
        
    parser=argparse.ArgumentParser()

    def add_bool_arguments(suffix,dest,default,help):
        default_help=''' (default)'''
        parser.add_argument(f'--{suffix}',dest=dest,action='store_true',help=f'''{help}{default_help if default else ''}''')
        parser.add_argument(f'--no-{suffix}',dest=dest,action='store_false',help=f'''don't {help}{default_help if not default else ''}''')
        parser.set_defaults(**{dest:default})

    if not is_macos():
        parser.add_argument('--prefix',default='/usr/local',metavar='PREFIX',help='''use %(metavar)s as installation prefix. Default: %(default)s''')
    add_bool_arguments('build-b2-with-debugger','build_b2_with_debugger',False,help='''build b2 with debugger''')
    add_bool_arguments('build-b2','build_b2',True,'''build b2''')
    add_bool_arguments('run-tests','run_tests',True,'''run tests after building''')
    parser.add_argument('--ninja',action='store_true',help='''have the Makefile defer to Ninja for the build''')

    main2(parser.parse_args(argv))
    
##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])
