#!/usr/bin/python3
import sys,os,os.path,argparse,subprocess,contextlib,re,collections

##########################################################################
##########################################################################

# build against a bunch of different SDL2 versions on Windows and note
# the results. Hopefully this'll apply to Linux too. (Building older
# revisions with CMake 4 on Linux is a bit annoying, but irrelevant if
# they come supplied by the package manager.)

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

# ensure sorting goes numerically
Tag=collections.namedtuple('Tag','major minor patch name')

def main2(options):
    sdl_path=os.path.join(options.b2_path,'submodules/SDL_official')
    if not os.path.isdir(sdl_path):
        fatal('SDL submodule not found: %s'%sdl_path)

    with working_folder(sdl_path):
        git_tag_list=subprocess.check_output(['git','tag','--list'],
                                             encoding='utf-8').split('\n')

    sdl_tag_re=re.compile(r'''release-(?P<major>[0-9]+)\.(?P<minor>[0-9]+)\.(?P<patch>[0-9]+)''')

    sdl_tags=[]
    for tag in git_tag_list:
        m=sdl_tag_re.match(tag)
        if m is not None:
            sdl_tags.append(Tag(major=int(m.group('major')),
                                minor=int(m.group('minor')),
                                patch=int(m.group('patch')),
                                name=tag))

    sdl_tags.sort(reverse=True)
    print(sdl_tags)

    os.putenv('CMAKE_EXTRA_ARGS','-DCMAKE_POLICY_VERSION_MINIMUM=3.5')

    # 2.0.12 was released in 2020 so minimal effort was expended on
    # versions before that.
    bad=[
        # requires DXSDK_DIR
        (2,0,0), (2,0,1), (2,0,2), (2,0,3),

# doesn't build?
(2,30,6),
    ]

    known_good=[
        (2,32,10),    # tag: release-2.32.10 2025-09-01T14:33:27-07:00
        (2,32,8),     # tag: release-2.32.8 2025-06-02T16:00:39-07:00
        (2,32,6),     # tag: release-2.32.6 2025-05-04T08:52:05-07:00
        (2,32,4),     # tag: release-2.32.4 2025-03-31T09:12:51-07:00
        (2,32,2),     # tag: release-2.32.2 2025-03-01T18:03:11-08:00
        (2,32,0),     # tag: release-2.32.0 2025-02-08T09:10:22-08:00
        (2,30,12),    # tag: release-2.30.12 2025-02-01T15:56:51-08:00
        (2,30,11),    # tag: release-2.30.11 2025-01-01T09:09:38-08:00
        (2,30,10),    # tag: release-2.30.10 2024-12-07T02:05:30+01:00
        (2,30,9),     # tag: release-2.30.9 2024-11-01T14:24:51-07:00
        (2,30,8),     # tag: release-2.30.8 2024-10-01T23:05:24+02:00
        (2,30,7),     # tag: release-2.30.7 2024-09-01T08:11:12-07:00
        (2,30,6),     # tag: release-2.30.6 2024-08-01T14:16:41-07:00
        (2,30,5),     # tag: release-2.30.5 2024-07-03T14:21:42-07:00
        (2,30,4),     # tag: release-2.30.4 2024-06-17T12:32:13-07:00
        (2,30,3),     # tag: release-2.30.3 2024-05-01T09:14:37-07:00
        (2,30,2),     # tag: release-2.30.2 2024-04-01T16:56:15-07:00
        (2,30,1),     # tag: release-2.30.1 2024-03-05T14:33:57-08:00
        (2,30,0),     # tag: release-2.30.0 2024-02-01T16:02:20-08:00
        (2,28,5),     # tag: release-2.28.5 2023-11-02T10:00:33-07:00
        (2,28,4),     # tag: release-2.28.4 2023-10-01T12:05:05-07:00
        (2,28,3),     # tag: release-2.28.3 2023-09-01T11:22:07-07:00
        (2,28,2),     # tag: release-2.28.2 2023-08-02T08:31:42-07:00
        (2,28,1),     # tag: release-2.28.1 2023-07-01T09:56:20-07:00
        (2,28,0),     # tag: release-2.28.0 2023-06-20T11:00:20-07:00
        (2,26,5),     # tag: release-2.26.5 2023-04-05T11:42:08-07:00
        (2,26,4),     # tag: release-2.26.4 2023-03-06T15:52:05-08:00
        (2,26,3),     # tag: release-2.26.3 2023-02-06T14:08:14-08:00
        (2,26,2),     # tag: release-2.26.2 2023-01-03T06:45:18-08:00
        (2,26,1),     # tag: release-2.26.1 2022-12-01T11:54:51-08:00
        (2,26,0),     # tag: release-2.26.0 2022-11-21T16:15:58-08:00
        (2,24,2),     # tag: release-2.24.2 2022-11-01T05:47:12-07:00
        (2,24,1),     # tag: release-2.24.1 2022-10-04T16:48:08-07:00
        (2,24,0),     # tag: release-2.24.0 2022-08-19T08:44:09-07:00

# need to do ``option(SDL_LIBC "" ON)'' and ``option(LIBC "" ON)'' in
# the CMakeLists.txt to build below this point with VS2022. also
# remove the SDL2:: prefix from the SDL2_LIBRARY parts.
#
# (didn't take great notes about which version requires each specific
# thing)
        (2,0,22),       #tag: release-2.0.22 2022-04-25T14:00:04-04:00
        (2,0,20),       #tag: release-2.0.20 2022-01-10T20:40:05Z
        (2,0,18),       #tag: release-2.0.18 2021-11-29T20:20:07-08:00
        (2,0,14),       #tag: release-2.0.14 2020-12-21T09:44:25-08:00
        (2,0,16),       #tag: release-2.0.16 2021-08-10T08:34:07-07:00

# to build below this point, SDL_LIBC and LIBC options must be OFF.
(2,0,12),               #tag: release-2.0.12 2020-03-10T18:35:31-07:00

# SDL_GetTextureScaleMode unavailable below this point.

    ]

    good=[]

    def fatal2(tag,message):
        print('Succeeded: %s'%([t.name for t in good]))
        print('Succeeded: %s'%(['(%d,%d,%d)'%(t.major,t.minor,t.patch) for t in good]))
        fatal('%s: %s'%(message,tag.name))

    first=True
    for sdl_tag in sdl_tags:
        if sdl_tag.major!=2: continue
        if (sdl_tag.major,sdl_tag.minor,sdl_tag.patch) in bad+known_good:
            continue

        with working_folder(sdl_path):
            subprocess.run(['git','checkout',sdl_tag.name],check=True)

        with working_folder(options.b2_path):
            print('Building: %s'%sdl_tag.name)

            # no need to do this on wip/tom!

            argv=['make','precommit_vs2022']

            if first:
                argv.append('REINIT=1')
                first=False
                
            result=subprocess.run(argv,shell=True)
            if result.returncode!=0:
                fatal2(sdl_tag,'precommit_vs2022 failed')

        good.append(sdl_tag)
            
            # result=subprocess.run(['make','precommit','VERBOSE=1','REINIT=1'])
            # if result.returncode!=0:
            #     fatal('failed: %s'%sdl_tag)

##########################################################################
##########################################################################

def auto_int(x): return int(x,0)

def main(argv):
    parser=argparse.ArgumentParser()

    parser.add_argument('--b2-path',default='.',metavar='FOLDER',help='''treat %(metavar)s as b2 path. Default: %(default)s''')

    main2(parser.parse_args(argv))

##########################################################################
##########################################################################
    
if __name__=='__main__': main(sys.argv[1:])
