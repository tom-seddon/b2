#!/usr/bin/python3
import sys,glob,os,os.path,gzip

##########################################################################
##########################################################################

def main(argv):
    uef_paths=glob.glob('*.uef')
    for uef_path in uef_paths:
        print(uef_path)
        with open(uef_path,'rb') as f: uef_data=f.read()
        uef_data=gzip.decompress(uef_data)

        uef_no_ext=os.path.splitext(uef_path)[0]
        with open(uef_no_ext+'.uncompressed.uef','wb') as f:
            f.write(uef_data)

##########################################################################
##########################################################################

if __name__=='__main__': main(sys.argv[1:])
