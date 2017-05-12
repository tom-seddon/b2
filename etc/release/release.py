#!/usr/bin/python
import argparse,os,os.path,sys,stat,subprocess,pipes,time,shutil

##########################################################################
##########################################################################

FOLDERPREFIX="Rel"

##########################################################################
##########################################################################

g_verbose=False

def v(str):
    global g_verbose
    
    if g_verbose:
        sys.stdout.write(str)
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

def fatal(str):
    sys.stderr.write("FATAL: %s"%str)
    if str[-1]!='\n': sys.stderr.write("\n")

    if os.getenv("EMACS") is not None: raise RuntimeError
    else: sys.exit(1)

##########################################################################
##########################################################################

def run(argv,ignore_errors=False):
    print 80*"-"
    print " ".join([pipes.quote(x) for x in argv])
    print 80*"-"

    ret=subprocess.call(argv)

    if not ignore_errors:
        if ret!=0: fatal("process failed")

def capture(argv):
    v("Run: %s\n"%argv)
    process=subprocess.Popen(args=argv,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    output=process.communicate()
    if process.returncode!=0: fatal("process failed")
    return output[0].splitlines()

def bool_str(x): return "YES" if x else "NO"

def makedirs(x):
    if not os.path.isdir(x): os.makedirs(x)

##########################################################################
##########################################################################

def build_win32(timings,options,platform,config,colour):
    folder="0%s.%s"%(FOLDERPREFIX,platform)
    
    start_time=time.clock()
    run(["cmd","/c",
         "color","%x"%colour])
    
    if not options.skip_compile:
        run(["cmd","/c",
             r"etc\release\build_windows.bat",
             "/maxcpucount",
             "/property:MultiProcessorCompilation=true", # http://stackoverflow.com/a/17719445/1618406
             "/property:Configuration=%s"%config,
             os.path.join(folder,"b2.sln")])

    if not options.skip_ctest:
        with ChangeDirectory(folder):
            run(["ctest",
                 "-j",os.getenv("NUMBER_OF_PROCESSORS"),
                 "-C",config])
            
    run(["cmd","/c",
         "color"],ignore_errors=True)

    timings[(platform,config)]=time.clock()-start_time

def create_README(folder,rev_hash):
    with open(os.path.join(folder,"README.txt"),"wt") as f:
        f.write("b2 - a BBC Micro emulator\n\n")
        f.write("For licence information, please consult LICENCE.txt.\n\n")
        f.write("Documentation can be found here: https://github.com/tom-seddon/b2/blob/%s/README.md\n\n"%rev_hash)
    
def main(options):
    global g_verbose
    g_verbose=options.verbose
    
    # Check working copy state.
    wc_dirty=False
    if len(capture(["git","status","--porcelain"])):
        wc_dirty=True
        if not options.ignore_working_copy:
            fatal("working copy is dirty (ignore with --ignore-working-copy)")

    # Check current branch.
    branches=capture(["git","branch"])
    for branch in branches:
        if branch.startswith("*"):
            branch=branch[2:]
            break

    versioned=True
    if not branch.startswith("v"):
        versioned=False
        if not options.ignore_branch:
            fatal("not a version branch (ignore with --ignore-branch)")

    # Get revision hash.
    rev_hash=capture(["git","rev-parse","HEAD"])[0]

    # (does --short do anything more than just strip the end off???)
    rev_hash_short=capture(["git","rev-parse","--short","HEAD"])[0]

    # 
    v("Branch: %s\n"%branch)
    v("Hash: %s\n"%rev_hash)
    v("Dirty working copy: %s\n"%bool_str(wc_dirty))

    if not options.skip_ctest:
        # -j2 makes a hilarious mess of the text output, but the
        # makefile parallelizes perfectly, saving about 1 minute on my
        # laptop.
        run([options.make,"-j2","init","FOLDERPREFIX=%s"%FOLDERPREFIX,"INCLUDE_EXPERIMENTAL=OFF"])

    ifolder=os.path.abspath(os.path.join("0Rel",sys.platform))
    try: shutil.rmtree(ifolder)
    except: pass

    # path that the ZIP contents will be assembled into.
    zip_folder=os.path.join(ifolder,"b2")
    makedirs(zip_folder)

    if sys.platform=="win32":
        timings={}

        build_win32(timings,options,"win64","RelWithDebInfo",0xf0)
        build_win32(timings,options,"win64","Final",0xf4)
        build_win32(timings,options,"win32","Final",0xe0)

        print timings

        shutil.copyfile("./0Rel.win32/src/b2/Final/b2.exe",os.path.join(zip_folder,"b2_32bit.exe"))
        shutil.copyfile("./0Rel.win64/src/b2/Final/b2.exe",os.path.join(zip_folder,"b2.exe"))
        shutil.copyfile("./0Rel.win64/src/b2/RelWithDebInfo/b2.exe",os.path.join(zip_folder,"b2_Debug.exe"))

        # Copy the assets from any output folder... they're all the same.
        shutil.copytree("./0Rel.win64/src/b2/Final/assets",os.path.join(zip_folder,"assets"))
    
        shutil.copyfile("./etc/release/LICENCE.txt",os.path.join(zip_folder,"LICENCE.txt"))

        create_README(zip_folder,rev_hash)


        zip_fname="b2-%s%s.zip"%(branch if versioned else rev_hash_short,
                                 "-local" if wc_dirty else "")
        zip_fname=os.path.join(ifolder,zip_fname)

        # The ZipFile module is a bit annoying to use.
        with ChangeDirectory(ifolder): run(["zip.exe","-9r",zip_fname,"b2"])

##########################################################################
##########################################################################

if __name__=="__main__":
    parser=argparse.ArgumentParser()

    if sys.platform not in ["win32","darwin","linux2"]: fatal("unsupported platform: %s"%sys.platform)
    if sys.platform=="win32":
        if os.getenv("VS140COMNTOOLS") is None: fatal("VS140COMNTOOLS not set - is VS2015 installed?")
        default_make="snmake.exe"
    else:
        default_make="make"

    parser.add_argument("-v","--verbose",action="store_true",help="be more verbose")
    parser.add_argument("--ignore-working-copy",action="store_true",help="ignore dirty working copy")
    parser.add_argument("--ignore-branch",action="store_true",help="ignore non-version branch")
    parser.add_argument("--skip-cmake",action="store_true",help="skip the cmake step")
    parser.add_argument("--skip-compile",action="store_true",help="skip the compile step")
    parser.add_argument("--skip-ctest",action="store_true",help="skip the ctest step")
    parser.add_argument("--make",dest="make",default=default_make,help="use %(metavar)s as GNU make. Default: ``%(default)s''")
    
    main(parser.parse_args(sys.argv[1:]))
