#! /usr/bin/env python

# 
# LSST Data Management System
# Copyright 2008, 2009, 2010 LSST Corporation.
# 
# This product includes software developed by the
# LSST Project (http://www.lsst.org/).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the LSST License Statement and 
# the GNU General Public License along with this program.  If not, 
# see <http://www.lsstcorp.org/LegalNotices/>.
#

"""
Usage: runPipeline.py [-L lev] policy runID

Execute the pipeline described by the given policy, assigning it
the given run ID.  This script should only be launched via mpiexec.  

If the pipeline policy references other policy files (via the "@" file 
include format), it is assumed that the given path is relative to the 
current working directory where this script is executed. 
"""
from lsst.pex.harness.Queue import Queue
from lsst.pex.harness.Clipboard import Clipboard
from lsst.pex.harness.Pipeline import Pipeline
from lsst.pex.mpiharness.MpiPipeline import MpiPipeline
import lsst.pex.harness.run as run

import lsst.pex.policy as policy
from lsst.pex.logging import Log

import lsst.daf.base as dafBase
from lsst.daf.base import PropertySet

import lsst.ctrl.events as events

import os
import sys
import optparse, traceback

usage = """Usage: %prog [-V lev] [-n name] policy runID"""
desc = """Execute the pipeline described by the given policy, assigning it
the given run ID.
"""

cl = optparse.OptionParser(usage=usage, description=desc)
cl.add_option("-n", "--name", action="store", default=None, dest="name",
              help="a name for identifying the pipeline")
run.addVerbosityOption(cl)

def main():
    """parse the input arguments and execute the pipeline
    """

    (cl.opts, cl.args) = cl.parse_args()
    logthresh = run.verbosity2threshold(cl.opts.verbosity)

    if(len(cl.args) < 2):
        print >> sys.stderr, \
            "%s: missing required argument(s)." % cl.get_prog_name()
        print cl.get_usage()
        sys.exit(1)

    pipelinePolicyName = cl.args[0]
    runId = cl.args[1]

    runPipeline(pipelinePolicyName, runId, logthresh, cl.opts.name)

def runPipeline(policyFile, runId, logthresh=None, name=None):
    """
    Create the Pipeline object and start the pipeline execution
    @param policyFile   the name of the policy file that defines the pipeline
    @param runId        the string to use as the production run identifier
    @param logthresh    the logging threshold to use to control the verbosity
                           of messages.
    """
    if name is None or name == "None":
        name = os.path.splitext(os.path.basename(policyFile))[0]
    
    pyPipeline = MpiPipeline(runId, policyFile, name)
    if isinstance(logthresh, int):
        pyPipeline.setLogThreshold(logthresh)

    pyPipeline.initializeLogger()   

    pyPipeline.configurePipeline()   

    pyPipeline.initializeQueues()  

    pyPipeline.initializeStages()    

    pyPipeline.startShutdownThread()

    pyPipeline.startSlices()  

    pyPipeline.startStagesLoop()



if (__name__ == '__main__'):
    try:
        main()
    except run.UsageError, e:
        print >> sys.stderr, "%s: %s" % (cl.get_prog_name(), e)
        sys.exit(1)
    except Exception, e:
        log = Log(Log.getDefaultLog(),"runPipeline")
        log.log(Log.FATAL, str(e))
        traceback.print_exc(file=sys.stderr)
        sys.exit(2)
