#!/usr/bin/python3
import sys,os,os.path,argparse,tarfile,collections,datetime,glob,tempfile,shutil
import release_tools

##########################################################################
##########################################################################

# Intended for use from a fresh working copy on a POSIX-type system.
#
# It does not check .gitignore, and will happily include useless stuff
# in the archive.
#
# For testing purposes, it does skip the build folder in the root of
# the working copy (as there's probably a pile of stuff in there). But
# that's all.

##########################################################################
##########################################################################

def remove_dot_names(names):
    i=0
    while i<len(names):
        if names[i].startswith('.'): del names[i]
        else: i+=1

##########################################################################
##########################################################################

def rmfiles(pattern):
    paths=glob.glob(pattern)
    for path in paths: os.unlink(path)

def main2(options):
    def run(temp):
        # Prepare tar file of contents of interest.
        pass1_tar_path=os.path.join(temp,'b2-pass1.tar')
        with release_tools.ChangeDirectory(options.input_path):
            paths=glob.glob('*')

            i=0
            while i<len(paths):
                if paths[i]=='build' and os.path.isdir(paths[i]): del paths[i]
                else: i+=1

            release_tools.run(['7z','a',pass1_tar_path]+paths)

        # Create folder with appropriate name.
        release_folder_name='b2-%s'%options.release_name
        work_path=os.path.join(temp,release_folder_name)
        release_tools.makedirs(work_path)

        # Extract contents of interest to folder.
        with release_tools.ChangeDirectory(work_path):
            release_tools.run(['7z','x',pass1_tar_path])

        # Fix stuff up in place.
        with release_tools.ChangeDirectory(work_path):
            os.rename('Makefile','Makefile.default.mak')
            shutil.copyfile('etc/release/Makefile.release.mak',
                            'Makefile')

            rmfiles('bin/*.exe')
            rmfiles('bin/*.bat')
            shutil.rmtree('etc/64tass-1.52.1237')
            shutil.rmtree('etc/ImageMagick-7.0.5-4-portable-Q16-x64')
            rmfiles('make.bat')

        # Set timestamp, if required.
        if options.timestamp is not None:
            with release_tools.ChangeDirectory(work_path):
                release_tools.set_tree_timestamps(options.timestamp,'.')

        # Create final tar file.
        if options.output_path is not None:
            with release_tools.ChangeDirectory(temp):
                release_tools.run(['7z',
                                   'a',
                                   '-mx=9',
                                   options.output_path,
                                   release_folder_name])
    
    if options.temp is None:
        with tempfile.TemporaryDirectory() as temp: run(temp)
    else:
        if os.path.isdir(options.temp): shutil.rmtree(options.temp)
        release_tools.makedirs(options.temp)
        run(options.temp)

##########################################################################
##########################################################################

def timestamp(x): return datetime.datetime.strptime(x,"%Y%m%d-%H%M%S")

def main(argv):
    parser=argparse.ArgumentParser()

    parser.add_argument('-o',metavar='FILE',dest='output_path',help='''write uncompressed tar archive to %(metavar)s''')

    parser.add_argument('--timestamp',metavar='TIMESTAMP',default=None,type=timestamp,help='''set archived files' atime/mtime to %(metavar)s - format must be YYYYMMDD-HHMMSS''')
    
    parser.add_argument('-i',metavar='PATH',dest='input_path',default='.',help='''treat %(metavar)s as root of b2 working copy. Default: %(default)s''')

    parser.add_argument('--temp',metavar='PATH',default=None,help='''use %(metavar)s as temp folder. Will create if required. Will create (and subsequently delete) a folder if not specified''')

    parser.add_argument('release_name',metavar='NAME',help='''use %(metavar)s to name the build''')

    main2(parser.parse_args(argv))

##########################################################################
##########################################################################
    
if __name__=='__main__': main(sys.argv[1:])
