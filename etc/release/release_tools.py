import sys,os,os.path,subprocess,time

##########################################################################
##########################################################################

g_verbose=False

##########################################################################
##########################################################################

def set_verbose(verbose):
    global g_verbose
    g_verbose=verbose

##########################################################################
##########################################################################

def v(str):
    global g_verbose
    
    if g_verbose:
        sys.stdout.write(str)
        sys.stdout.flush()

##########################################################################
##########################################################################

def fatal(str):
    sys.stderr.write("FATAL: %s"%str)
    if str[-1]!='\n': sys.stderr.write("\n")

    if os.getenv("EMACS") is not None: raise RuntimeError
    else: sys.exit(1)

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
    
def run(argv,ignore_errors=False,**other_popen_kwargs):
    def quote(x):
        if not x.startswith('"') and ' ' in x: return '"%s"'%x
        else: return x

    print(80*"-")
    print(" ".join([quote(x) for x in argv]))
    print(80*"-")

    ret=subprocess.call(argv,**other_popen_kwargs)

    if not ignore_errors:
        if ret!=0: fatal("process failed: %s"%argv)

##########################################################################
##########################################################################

def capture(argv):
    v("Run: %s\n"%argv)
    process=subprocess.Popen(args=argv,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    output=process.communicate()
    if process.returncode!=0: fatal("process failed: %s"%argv)
    return output[0].decode('utf8').splitlines()

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

def makedirs(x):
    if not os.path.isdir(x): os.makedirs(x)
