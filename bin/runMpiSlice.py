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


from lsst.pex.harness.Queue import Queue
from lsst.pex.harness.Clipboard import Clipboard
from lsst.pex.harness.Slice import Slice
from lsst.pex.mpiharness.MpiSlice import MpiSlice

import lsst.pex.policy as policy
from lsst.pex.logging import Log

import lsst.daf.base as dafBase

import lsst.ctrl.events as events

import os
import sys
import optparse, traceback

usage = """Usage: %prog [-l lev] [-n name] policy runID"""
desc = """Execute a slice worker process for a pipeline described by the
given policy, assigning it the given run ID.  This should not be executed
outside the context of a pipline harness process.  
"""

cl = optparse.OptionParser(usage=usage, description=desc)
cl.add_option("-l", "--log-threshold", type="int", action="store",
              dest="logthresh", default=None, metavar="lev",
              help="the logging message level threshold")
cl.add_option("-n", "--name", action="store", default=None, dest="name",
              help="a name for identifying the pipeline")

def main():
    """parse the input arguments and execute the pipeline
    """

    (cl.opts, cl.args) = cl.parse_args()

    if(len(cl.args) < 2):
        print >> sys.stderr, \
            "%s: missing required argument(s)." % cl.get_prog_name()
        print cl.get_usage()
        sys.exit(1)

    pipelinePolicyName = cl.args[0]
    runId = cl.args[1]

    runSlice(pipelinePolicyName, runId, cl.opts.logthresh, cl.opts.name)

def runSlice(policyFile, runId, logthresh=None, name="unnamed"):
    """
    runSlice: MpiSlice Main execution 
    """
    if name is None or name == "None":
        name = os.path.splitext(os.path.basename(policyFile))[0]
    
    pySlice = MpiSlice(runId, policyFile, name)
    if isinstance(logthresh, int):
        pySlice.setLogThreshold(logthresh)

    pySlice.initializeLogger()

    pySlice.configureSlice()   

    pySlice.initializeQueues()     

    pySlice.initializeStages()   

    pySlice.startStagesLoop()

    pySlice.shutdown()


if (__name__ == '__main__'):
    try:
        main()
    except Exception, e:
        log = Log(Log.getDefaultLog(),"runSlice")
        log.log(Log.FATAL, str(e))
        traceback.print_exc(file=sys.stderr)
        sys.exit(2);
