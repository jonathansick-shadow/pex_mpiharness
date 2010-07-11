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


from lsst.pex.harness.Pipeline import Pipeline
from lsst.pex.harness.Queue import Queue
from lsst.pex.harness.Clipboard import Clipboard
from lsst.pex.harness.Directories import Directories
from lsst.pex.harness.harnessLib import TracingLog
from lsst.pex.logging import Log, LogRec, cout, Prop
from lsst.pex.harness import harnessLib as logutils
from lsst.pex.mpiharness import mpiharnessLib as mpiutils

import lsst.pex.policy as policy

import lsst.pex.exceptions
from lsst.pex.exceptions import *

import lsst.daf.base as dafBase
from lsst.daf.base import *
import lsst.daf.persistence as dafPersist
from lsst.daf.persistence import *


import lsst.ctrl.events as events

import os, sys, re, traceback, time
import threading

"""
Pipeline class manages the operation of a multi-stage parallel pipeline.
The Pipeline is configured by reading a Policy file.   This Python Pipeline
class imports the C++ Pipeline class via a python extension module in order 
to setup and manage the MPI environment.
Pipeline has a __main__ portion as it serves as the main executable program 
('glue layer') for running a Pipeline. The Pipeline spawns Slice workers 
using an MPI-2 Spawn operation. 
"""

class MpiPipeline(Pipeline):
    '''Python Pipeline class implementation. Contains main pipeline workflow'''

    def __init__(self, runId='-1', pipelinePolicyName=None, name="unnamed"):
        """
        Initialize the Pipeline: create empty Queue and Stage lists;
        import the C++ Pipeline instance; initialize the MPI environment
        """
        # super(MpiPipeline, self).__init__()
        Pipeline.__init__(self, runId, pipelinePolicyName, name)

        self.cppPipeline = mpiutils.Pipeline(self._pipelineName)
        self.cppPipeline.setRunId(runId)
        self.cppPipeline.setPolicyName(pipelinePolicyName)
        self.cppPipeline.initialize()
        self.universeSize = self.cppPipeline.getUniverseSize()
        self._runId = runId
        self.pipelinePolicyName = pipelinePolicyName
        self.forceShutdown = 0
        self.delayTime = 0.01


    def __del__(self):
        """
        Delete the Pipeline object: clean up
        """
        if self.log is not None:
            self.log.log(self.VERB1, 'Python Pipeline being deleted')


    def startSlices(self):
        """
        Initialize the Queue by defining an initial dataset list
        """
        log = self.log.traceBlock("startSlices", self.TRACE-2)
        self.cppPipeline.startSlices()
        log.done()


    def startStagesLoop(self): 
        """
        Method to execute loop over Stages
        """
        startStagesLoopLog = self.log.traceBlock("startStagesLoop", self.TRACE)
        looplog = TracingLog(self.log, "visit", self.TRACE)
        stagelog = TracingLog(looplog, "stage", self.TRACE-1)
        proclog = TracingLog(stagelog, "process", self.TRACE)

        visitcount = 0 

        while True:

            time.sleep(self.delayTime)

            if ((((self.executionMode == 1) and (visitcount == 1)) or self.forceShutdown == 1)):
                LogRec(looplog, Log.INFO)  << "terminating pipeline and slices after one loop/visit "
                self.cppPipeline.invokeShutdown()
                # 
                # Need to shutdown Threads here 
                # 
                break
            else:
                visitcount += 1
                looplog.setPreamblePropertyInt("loopnum", visitcount)
                looplog.start()
                stagelog.setPreamblePropertyInt("loopnum", visitcount)
                proclog.setPreamblePropertyInt("loopnum", visitcount)

                # synchronize at the top of the Stage loop 
                self.cppPipeline.invokeContinue()

                self.startInitQueue()    # place an empty clipboard in the first Queue

                self.errorFlagged = 0
                for iStage in range(1, self.nStages+1):
                    stagelog.setPreamblePropertyInt("stageId", iStage)
                    stagelog.start(self.stageNames[iStage-1] + " loop")
                    proclog.setPreamblePropertyInt("stageId", iStage)

                    stage = self.stageList[iStage-1]

                    self.handleEvents(iStage, stagelog)

                    self.tryPreProcess(iStage, stage, stagelog)

                    # if(self.isDataSharingOn):
                    #     self.invokeSyncSlices(iStage, stagelog)

                    proclog.start("process and wait")
                    self.cppPipeline.invokeProcess(iStage)
                    proclog.done()

                    self.tryPostProcess(iStage, stage, stagelog)

                    stagelog.done()

                    time.sleep(self.delayTime)
                    self.checkExitByStage()

                else:
                    looplog.log(self.VERB2, "Completed Stage Loop")

                time.sleep(self.delayTime)
                self.checkExitByVisit()


            # Uncomment to print a list of Citizens after each visit 
            # print datap.Citizen_census(0,0), "Objects:"
            # print datap.Citizen_census(datap.cout,0)

            looplog.log(Log.DEBUG, 'Retrieving finalClipboard for deletion')
            finalQueue = self.queueList[self.nStages]
            finalClipboard = finalQueue.getNextDataset()
            looplog.log(Log.DEBUG, "deleting final clipboard")
            # delete entries on the clipboard
            finalClipboard.close()
            del finalClipboard

        startStagesLoopLog.log(Log.INFO, "Shutting down pipeline");
        startStagesLoopLog.done()
        self.shutdown()


    def checkExitBySyncPoint(self): 
        log = Log(self.log, "checkExitBySyncPoint")

        if((self._stop.isSet()) and (self.exitLevel == 2)):
            log.log(Log.INFO, "Pipeline stop is set at exitLevel of 2")
            log.log(Log.INFO, "Exit here at a Synchronization point")
            self.forceShutdown = 1

    def checkExitByStage(self): 
        log = Log(self.log, "checkExitByStage")

        if((self._stop.isSet()) and (self.exitLevel == 3)):
            log.log(Log.INFO, "Pipeline stop is set at exitLevel of 3")
            log.log(Log.INFO, "Exit here at the end of the Stage")
            self.forceShutdown = 1

    def checkExitByVisit(self): 
        log = Log(self.log, "checkExitByVisit")

        if((self._stop.isSet()) and (self.exitLevel == 4)):
            log.log(Log.INFO, "Pipeline stop is set at exitLevel of 4")
            log.log(Log.INFO, "Exit here at the end of the Visit")
            self.forceShutdown = 1


    def shutdown(self): 
        """
        Shutdown the Pipeline execution: delete the MPI environment
        Send the Exit Event if required
        """
        if self.exitTopic == None:
            pass
        else:
            oneEventTransmitter = events.EventTransmitter(self.eventBrokerHost, self.exitTopic)
            psPtr = dafBase.PropertySet()
            psPtr.setString("message", str("exiting_") + self._runId )

            oneEventTransmitter.publish(psPtr)

        # Also have to tell the shutdown Thread to stop  
        self.oneShutdownThread.stop()
        self.oneShutdownThread.join()
        self.log.log(self.VERB2, 'Shutdown thread ended ')

        self.log.log(self.VERB2, 'Pipeline calling MPI_Finalize ')
        self.cppPipeline.shutdown()


    def invokeSyncSlices(self, iStage, stagelog):
        """
        If needed, calls the C++ Pipeline invokeSyncSlices
        """
        invlog = stagelog.traceBlock("invokeSyncSlices", self.TRACE-1)
        if(self.shareDataList[iStage-1]):
            self.cppPipeline.invokeSyncSlices(); 
        invlog.done()

trailingpolicy = re.compile(r'_*(policy|dict)$', re.IGNORECASE)

