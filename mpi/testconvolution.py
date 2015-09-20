#!/usr/bin/python -u

import sys # so that we can return a value at the end.
import time
from subprocess import * # so that we can run commands
import getopt
import os.path

def main(argv):
    retval = 0
    msg = "MPI convolution unit tests"
    logfile = 'testconvolutions.log'
    print msg

    print "MPI convolution unit test"
    usage = "Usage:\n"\
            "./testtranspose.py\n"\
            "\t-s\t\tSpecify a short run\n"\
            "\t-h\t\tShow usage"

    shortrun = False
    try:
        opts, args = getopt.getopt(argv,"sh")
    except getopt.GetoptError:
        print "Error in arguments"
        print usage
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-s"):
            shortrun = True
        if opt in ("-h"):
            print usage
            sys.exit(0)

    testlist = []
    testlist.append("testcconv3.py")

    print "Log in " + logfile + "\n"
    log = open(logfile, 'w')
    log.write(msg)
    log.write("\n")
    log.close()

    ntests = 0
    nfails = 0
    tstart = time.time()
    for test in testlist:
        ntests += 1
        if not os.path.isfile(test):
            msg = "Error: "+ pname + "not present!"
            print msg
            log = open(logfile, 'a')
            log.write(msg + "\n")
            log.close()
            retval += 1
        else:
            msg = "Running " + test + ": "
            print(msg),
            log = open(logfile, 'a')
            log.write(msg)
            log.close()
            cmd = []
            cmd.append("./" + test)
            if(shortrun):
                cmd.append("-s")

            #print cmd
            proc = Popen(cmd, stdout = PIPE, stderr = PIPE)
            proc.wait() # sets the return code

            prc = proc.returncode
            out, err = proc.communicate() # capture output
            if (prc == 0): # did the process succeed?
                msg = "\tpass"
                print msg
                log = open(logfile, 'a')
                log.write(msg + "\n")
                log.close()
            else:
                msg = "\tFAILED!"
                print msg
                log = open(logfile, 'a')
                log.write(msg + "\n")
                log.write("stdout:\n" + out + "\n")
                log.write("stderr:\n" + err + "\n")
                log.close()
                retval += 1

    print "\n", nfails, "failures out of", ntests, "tests." 

    tend = time.time()
    print "\nElapsed time (s):", tend - tstart

    sys.exit(retval)

if __name__ == "__main__":
    main(sys.argv[1:])