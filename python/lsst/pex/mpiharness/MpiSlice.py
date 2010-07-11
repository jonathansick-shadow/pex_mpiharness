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


from lsst.pex.harness.Slice import Slice
from lsst.pex.harness.Queue import Queue
from lsst.pex.harness.Clipboard import Clipboard
from lsst.pex.harness.harnessLib import TracingLog
from lsst.pex.harness.Directories import Directories
from lsst.pex.logging import Log, LogRec, Prop
from lsst.pex.mpiharness import mpiharnessLib as mpiutils

import lsst.pex.policy as policy
import lsst.pex.exceptions as ex

import lsst.daf.base as dafBase
from lsst.daf.base import *
import lsst.daf.persistence as dafPersist
from lsst.daf.persistence import *


import lsst.ctrl.events as events
import lsst.pex.exceptions
from lsst.pex.exceptions import *

import os, sys, re, traceback
import threading


"""
Slice represents a single parallel worker program.  
Slice executes the loop of Stages for processing a portion of an Image (e.g.,
single ccd or amplifier). The processing is synchonized with serial processing
in the main Pipeline via MPI communications.  This Python Slice class accesses 
the C++ Slice class via a python extension module to obtain access to the MPI 
environment. 
A Slice obtains its configuration by reading a policy file. 
Slice has a __main__ portion as it serves as the executable program
"spawned" within the MPI-2 Spawn of parallel workers in the C++ Pipeline 
implementation. 
"""

class MpiSlice(Slice):
    '''Slice: Python Slice class implementation. Wraps C++ Slice'''

    #------------------------------------------------------------------------
    def __init__(self, runId="TEST", pipelinePolicyName=None, name="unnamed"):
        """
        Initialize the Slice: create an empty Queue List and Stage List;
        Import the C++ Slice  and initialize the MPI environment
        """

        # super(MpiSlice, self).__init__()
        Slice.__init__(self, runId, pipelinePolicyName, name)
        # log message levels
        self.cppSlice = mpiutils.Slice(self._pipelineName)
        self.cppSlice.setRunId(runId)
        self.cppSlice.initialize()
        self._rank = self.cppSlice.getRank()
        self.universeSize = self.cppSlice.getUniverseSize()


    def __del__(self):
        """
        Delete the Slice object: cleanup 
        """
        if self.log is not None:
            self.log.log(self.VERB1, 'Python Slice being deleted')


    def startStagesLoop(self): 
        """
        Execute the Stage loop. The loop progressing in step with 
        the analogous stage loop in the central Pipeline by means of
        MPI Bcast and Barrier calls.
        """
        startStagesLoopLog = self.log.traceBlock("startStagesLoop", self.TRACE)
        looplog = TracingLog(self.log, "visit", self.TRACE)
        stagelog = TracingLog(looplog, "stage", self.TRACE-1)

        visitcount = 0
        while True:
            visitcount += 1
            looplog.setPreamblePropertyInt("loopnum", visitcount)
            looplog.start()
            stagelog.setPreamblePropertyInt("loopnum", visitcount)

            self.cppSlice.invokeShutdownTest()
            looplog.log(self.VERB3, "Tested for Shutdown")

            self.startInitQueue()    # place an empty clipboard in the first Queue

            self.errorFlagged = 0
            for iStage in range(1, self.nStages+1):
                stagelog.setPreamblePropertyInt("stageId", iStage)
                stagelog.start(self.stageNames[iStage-1] + " loop")

                stageObject = self.stageList[iStage-1]
                self.handleEvents(iStage, stagelog)

                # if(self.isDataSharingOn):
                #    self.syncSlices(iStage, stagelog) 

                self.tryProcess(iStage, stageObject, stagelog)

                stagelog.done()

            looplog.log(self.VERB2, "Completed Stage Loop")

            # If no error/exception was flagged, 
            # then clear the final Clipboard in the final Queue

            if self.errorFlagged == 0:
                looplog.log(Log.DEBUG,
                            "Retrieving final Clipboard for deletion")
                finalQueue = self.queueList[self.nStages]
                finalClipboard = finalQueue.getNextDataset()
                finalClipboard.close()
                del finalClipboard
                looplog.log(Log.DEBUG, "Deleted final Clipboard")
            else:
                looplog.log(self.VERB3, "Error flagged on this visit")
            looplog.done()

        startStagesLoopLog.done()

    def shutdown(self): 
        """
        Shutdown the Slice execution
        """
        shutlog = Log(self.log, "shutdown", Log.INFO);
        shutlog.log(Log.INFO, "Shutting down Slice")
        self.cppSlice.shutdown()

    def syncSlices(self, iStage, stageLog):
        """
        If needed, performs interSlice communication prior to Stage process
        """
        synclog = stageLog.traceBlock("syncSlices", self.TRACE-1);

        if(self.shareDataList[iStage-1]):
            synclog.log(Log.DEBUG, "Sharing Clipboard data")

            queue = self.queueList[iStage-1]
            clipboard = queue.getNextDataset()
            sharedKeys = clipboard.getSharedKeys()

            synclog.log(Log.DEBUG, "Obtained %d sharedKeys" % len(sharedKeys))

            for skey in sharedKeys:

                synclog.log(Log.DEBUG,
                        "Executing C++ syncSlices for keyToShare: " + skey)

                psPtr = clipboard.get(skey)
                newPtr = self.cppSlice.syncSlices(psPtr)
                valuesFromNeighbors = newPtr.toString(False)

                neighborList = self.cppSlice.getRecvNeighborList()
                for element in neighborList:
                    neighborKey = skey + "-" + str(element)
                    nKey = "neighbor-" + str(element)
                    propertySetPtr = newPtr.getAsPropertySetPtr(nKey)
                    testString = propertySetPtr.toString(False)
                    clipboard.put(neighborKey, propertySetPtr, False)
                    synclog.log(Log.DEBUG,
                                "Added to Clipboard: %s: %s" % (neighborKey,
                                                                testString))
                    
                synclog.log(Log.DEBUG,
                        "Received PropertySet: " + valuesFromNeighbors)

            queue.addDataset(clipboard)

        synclog.done()

    def tryProcess(self, iStage, stage, stagelog):
        """
        Executes the try/except construct for Stage process() call 
        """
        # Important try - except construct around stage process() 
        proclog = stagelog.traceBlock("tryProcess", self.TRACE-2);

        stageObject = self.stageList[iStage-1]
        proclog.log(self.VERB3, "Getting process signal from Pipeline")
        self.cppSlice.invokeBcast(iStage)

        # Important try - except construct around stage process() 
        try:
            # If no error/exception has been flagged, run process()
            # otherwise, simply pass along the Clipboard 
            if (self.errorFlagged == 0):
                processlog = stagelog.traceBlock("process", self.TRACE)
                stageObject.applyProcess()
                processlog.done()
            else:
                proclog.log(self.TRACE, "Skipping process due to error")
                self.transferClipboard(iStage)
  
        ### raise lsst.pex.exceptions.LsstException("Terrible Test Exception")
        except:
            trace = "".join(traceback.format_exception(
                sys.exc_info()[0], sys.exc_info()[1], sys.exc_info()[2]))
            proclog.log(Log.FATAL, trace)

            # Flag that an exception occurred to guide the framework to skip processing
            self.errorFlagged = 1
            # Post the cliphoard that the Stage failed to transfer to the output queue
            self.postOutputClipboard(iStage)


        proclog.log(self.VERB3, "Getting end of process signal from Pipeline")
        self.cppSlice.invokeBarrier(iStage)
        proclog.done()

        
trailingpolicy = re.compile(r'_*(policy|dict)$', re.IGNORECASE)

if (__name__ == '__main__'):
    """
    Slice Main execution 
    """

    pySlice = MpiSlice()

    pySlice.configureSlice()   

    pySlice.initializeQueues()     

    pySlice.initializeStages()   

    pySlice.startInitQueue()

    pySlice.startStagesLoop()

    pySlice.shutdown()

