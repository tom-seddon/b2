#!/usr/bin/python3
import sys,os,os.path,glob,fnmatch

##########################################################################
##########################################################################

sys.path.append(os.path.join(os.path.dirname(sys.argv[0]),'../../submodules/beeb/bin'))
import ssd_create

##########################################################################
##########################################################################

def main():
    src_paths=glob.glob(os.path.join(os.path.dirname(sys.argv[0]),
                                     '0/T.*'))
    src_paths=[src_path for src_path in src_paths if not fnmatch.fnmatch(src_path,'*.inf')]

    dest_folder=os.path.join(os.path.dirname(sys.argv[0]),
                               '../../build/b2_tests')
    if not os.path.isdir(dest_folder): os.makedirs(dest_folder)

    for src_path in src_paths:
        dest_name=os.path.basename(src_path)
        dest_name=dest_name.replace('#2e','.')
        beeb_name=dest_name
        assert dest_name.lower().startswith('t.')
        dest_name=dest_name[2:]
        dest_path='{}.ssd'.format(os.path.join(dest_folder,dest_name))
        # print '{} -> {}'.format(src_path,dest_path)

        ssd_create.main(['-o',dest_path,
                         '-b','CHAIN "{}"'.format(beeb_name),
                         src_path])

##########################################################################
##########################################################################

if __name__=='__main__': main()
