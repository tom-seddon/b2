#!python3
import os,os.path,sys,argparse,fnmatch

##########################################################################
##########################################################################

def main3(f,options):
    patterns=[
        '*.c',
        '*.cpp',
        '*.h',
        '*.inl',
        '*.m',
        '*.mm',
    ]

    file_paths=set()
    for path in options.paths:
        for dirpath,dirnames,filenames in os.walk(os.path.abspath(path)):
            for filename in filenames:
                match=False
                for pattern in patterns:
                    if fnmatch.fnmatch(filename,pattern):
                        match=True
                        break

                if match: file_paths.add(os.path.join(dirpath,filename))

    file_paths=list(file_paths)

    # group files into command lines of ~1500 chars each
    argvs=[]
    i=0
    argv=''
    while True:
        if i==len(file_paths) or len(argv)>1500:
            argvs.append(argv)
            argv=''

        if i==len(file_paths): break

        argv+=' "%s"'%file_paths[i]
        i+=1

    prefix='files'
    all_file_targets=' '.join(['%s%d'%(prefix,i) for i in range(len(argvs))])
    
    f.write('.PHONY:all %s\n'%all_file_targets)
    f.write('all: %s\n'%all_file_targets)

    for i,argv in enumerate(argvs):
        f.write('%s%d:\n'%(prefix,i))
        f.write('\t\"%s\" -style=file -i%s\n'%(options.exe,argv))

    f.write('.PHONY:revert\n')
    f.write('revert:\n')
    for argv in argvs: f.write('\tgit checkout --%s\n'%argv)

##########################################################################
##########################################################################

def main2(options):
    if options.output_path is None: main3(sys.stdout,options)
    else:
        with open(options.output_path,'wt') as f: main3(f,options)

##########################################################################
##########################################################################

def main(argv):
    parser=argparse.ArgumentParser()

    parser.add_argument('-o','--output',dest='output_path',metavar='PATH',help='''write output to %(metavar)s (stdout if not specified)''')
    parser.add_argument('-e','--exe',metavar='PATH',default='clang-format',help='''use %(metavar)s as clang-format. Default: %(default)s''')
    parser.add_argument('paths',nargs='+',metavar='PATH',help='''process C++ file(s) in %(metavar)s''')

    main2(parser.parse_args(argv))

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])

