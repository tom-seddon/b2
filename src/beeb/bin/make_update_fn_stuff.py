#!/usr/bin/python3
import sys,os,os.path,argparse,re,contextlib,shutil

##########################################################################
##########################################################################

def fatal(msg):
    sys.stderr.write('FATAL: %s\n'%msg)
    sys.exit(1)

##########################################################################
##########################################################################

@contextlib.contextmanager
def output_file(path):
    if path=='-':
        sys.stdout.write('\n%s\n\n'%('-'*75))
        yield sys.stdout
    else:
        f=open(path,'wt')
        try: yield f
        finally: f.close()

##########################################################################
##########################################################################

def main2(options):
    num_mfns_re=re.compile(r'''NUM_BBCMICRO_UPDATE_MFNS\s*=\s*(?P<n>[0-9]+)\s*;''')
    num_update_mfns=None
    with open(os.path.join(options.b2_root,
                           'src/beeb/include/beeb/BBCMicro.h'),
              'rt') as f:
        for line in f.readlines():
            m=num_mfns_re.search(line.rstrip())
            if m is not None:
                num_update_mfns=int(m.group('n'))
                break
    if num_update_mfns is None: fatal('failed to find NUM_BBCMICRO_UPDATE_MFNS')

    if num_update_mfns%options.num_update_groups!=0:
        fatal('num update mfns not divisible by num update groups')

    if not os.path.isfile(os.path.join(options.b2_root,'common.cmake')):
        fatal('not obviously b2 root: %s'%options.b2_root)

    output_path=os.path.join(options.b2_root,'src/beeb/generated')
    if os.path.isdir(output_path): shutil.rmtree(output_path)
    os.makedirs(output_path)

    @contextlib.contextmanager
    def open_generated_file(name):
        f=open(os.path.join(output_path,name),'wt')
        try:
            f.write('// automatically generated. do not edit.\n')
            yield f
        finally: f.close()

    def write_static_assert(f):
        f.write('static_assert(NUM_BBCMICRO_UPDATE_MFNS==%d,"must re-run make update_mfns");\n'%num_update_mfns)

    with open_generated_file('BBCMicro.groups.generated.h') as f:
        for i in range(options.num_update_groups):
            f.write('static const UpdateMFn ms_update_mfns%d[%d];\n'%
                    (i,
                     num_update_mfns/options.num_update_groups))

    with open_generated_file('BBCMicro.groups.generated.inl') as f:
        for i in range(options.num_update_groups):
            f.write('BBCMicro::ms_update_mfns%d,\n'%i)

    for i in range(options.num_update_groups):
        with open_generated_file('BBCMicro.group%d.generated.cpp'%i) as f:
            f.write('#include "../src/BBCMicro_Update.inl"\n')
            f.write('\n')
            write_static_assert(f)
            f.write('\n')
            f.write('const BBCMicro::UpdateMFn BBCMicro::ms_update_mfns%d[]={\n'%i)
            for j in range(num_update_mfns//options.num_update_groups):
                k=j*options.num_update_groups+i
                f.write('    &BBCMicro::UpdateTemplated<::GetNormalizedBBCMicroUpdateFlags(%d)>,\n'%k)
            f.write('};\n')

    # touch appropriate CMakeLists.txt to force a rebuild.
    os.utime(os.path.join(options.b2_root,'src/beeb/CMakeLists.txt'))
    
##########################################################################
##########################################################################

def main(argv):
    def auto_int(x): return int(x,0)
    
    parser=argparse.ArgumentParser()

    parser.add_argument('b2_root',metavar='PATH',help='''specify working copy root for b2''')
    parser.add_argument('-n','--num-update_groups',metavar='N',type=auto_int,default=1,help='''generate %(metavar)s upgrade group(s). Default: %(default)s''')

    main2(parser.parse_args(argv))

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])
