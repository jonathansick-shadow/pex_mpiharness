#! /usr/bin/env python
#
from __future__ import with_statement
import re, sys, os, os.path, shutil, subprocess
import optparse, traceback
from lsst.pex.logging import Log
from lsst.pex.policy import Policy
import lsst.pex.harness.run as run

usage = """usage: %prog policy_file runid [pipelineName] [-vqsd] [-L lev] [-n file]"""

desc = """
Launch a pipeline with a given policy and Run ID.  If a node list file is not 
provided via the -n option, a file called "nodelist.scr" in the current 
directory will be used.  If the policy_file refers to other policy files, 
the path to those files will taken to be relative to the current directory.
If a log verbosity is not specified, the default will be taken from the 
policy file.
"""

cl = optparse.OptionParser(usage=usage, description=desc)
run.addAllVerbosityOptions(cl)
cl.add_option("-n", "--nodelist", action="store", dest="nodelist", 
              metavar="file", help="file containing the MPI machine list")

# command line results
cl.opts = {}
cl.args = []

pkgdirvar = "PEX_HARNESS_DIR"

def createLog():
    log = Log(Log.getDefaultLog(), "harness.launchPipeline")
    return log

def setVerbosity(verbosity):
    logger.setThreshold(run.verbosity2threshold(verbosity, -1))  

logger = createLog()

def main():
    try:
        (cl.opts, cl.args) = cl.parse_args();
        setVerbosity(cl.opts.verbosity)

        if len(cl.args) < 1:
            print usage
            raise RuntimeError("Missing arguments: pipeline_policy_file runId")
        if len(cl.args) < 2:
            print usage
            raise RuntimeError("Missing argument: runid")

        name = None
        if len(cl.args) > 2:
            name = cl.args[2]
    
        launchPipeline(cl.args[0], cl.args[1], name, cl.opts.verbosity)

    except SystemExit:
        pass
    except:
        tb = traceback.format_exception(sys.exc_info()[0],
                                        sys.exc_info()[1],
                                        sys.exc_info()[2])
        logger.log(Log.FATAL, tb[-1].strip())
        logger.log(Log.DEBUG, "".join(tb[0:-1]).strip())
        sys.exit(1)

def launchPipeline(policyFile, runid, name=None, verbosity=None):
    if not os.environ.has_key(pkgdirvar):
        raise RuntimeError(pkgdirvar + " env. var not setup")

    nodesfile = "nodelist.scr"
    if cl.opts.nodelist is not None:
        nodesfile = cl.opts.nodelist

    # ensure we have .mpd.conf files deployed on all nodes
    nodes_set = []
    nnodes = 0
    nprocs = 0
    with file(nodesfile) as nodelist:
        for node in nodelist:
            node = node.strip()
            if len(node)==0 or node.startswith('#'): continue
            if node.find(':') >= 0:
                (node, n) = node.split(':')
            else:
                n = ''
            nnodes += 1
            n = n.strip()
            if n != '':
                nprocs += int(n)
            else:
                nprocs += 1

            if node in nodes_set: continue

    cmd = "runPipelin.sh.py %s %s %s %d %d" % \
          (policyFile, runid, nodesfile, nnodes, nprocs)
    if name is not None:
        cmd += " %s" % name
    if verbosity is not None:
        cmd += " %s" % verbosity
    logger.log(Log.DEBUG, "exec " + cmd)
    os.execvp("runPipeline.sh", cmd.split())

    raise RuntimeError("Failed to exec runPipeline.sh")

def getNode(nodeentry):
    colon = nodeentry.find(':')
    if colon < 1:  
        return nodeentry
    else:
        return nodeentry[0:colon]

if __name__ == "__main__":
    main()
    
