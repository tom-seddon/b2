#!/usr/bin/python
import sys,argparse

##########################################################################
##########################################################################

# http://stackoverflow.com/questions/25513043/python-argparse-fails-to-parse-hex-formatting-to-int-type
def auto_int(x): return int(x,0)

##########################################################################
##########################################################################

def main(options):
    for i in range(options.num):
        print "10 REM %d"%i

##########################################################################
##########################################################################

if __name__=="__main__":
    parser=argparse.ArgumentParser()

    parser.add_argument("num",type=auto_int,help="number of lines")
    
    main(parser.parse_args())
    
