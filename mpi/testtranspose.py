#!/usr/bin/python -u

import sys # so that we can return a value at the end.
from subprocess import * # so that we can run commands
import random # for randum number generators

import os.path

retval = 0

if not os.path.isfile("transpose"):
    print "Error: transpose executable not present!"
    retval += 1
else:
    
    Xlist = [4,5]
    Ylist = [4]
    Plist = [1,2,3,4]
    for X in Xlist:
        for Y in Ylist:
            for P in Plist:
                cmd = []
                cmd.append("mpirun.mpich")
                cmd.append("-n")
                cmd.append(str(P))
                cmd.append("./transpose")
                cmd.append("-N0")
                cmd.append("-X" + str(X))
                cmd.append("-Y" + str(Y))
                print " ".join(cmd)

                proc = Popen(cmd, stdout = PIPE, stderr = PIPE)
                proc.wait() # sets the return code
                prc = proc.returncode
                out, err = proc.communicate() # capture output
                if (prc == 0): # did the process succeed?
                    print "\tSuccess"
                else:
                    print out
                    print err
                    print "\t" + " ".join(cmd)
                    print "\tFAILED!"
                    retval += 1

sys.exit(retval)
