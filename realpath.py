#!/usr/bin/python
import os.path,sys

def main():
    for arg in sys.argv[1:]: print os.path.realpath(arg)

if __name__=='__main__': main()
