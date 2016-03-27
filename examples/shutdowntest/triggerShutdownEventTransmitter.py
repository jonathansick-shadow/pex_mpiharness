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


import threading
import lsst.daf.base as dafBase
from lsst.daf.base import *

import lsst.ctrl.events as events
import time

if __name__ == "__main__":
    print "starting...\n"

    shutdownTopic = "triggerShutdownA"
    eventBrokerHost = "lsst8.ncsa.uiuc.edu"

    externalEventTransmitter = events.EventTransmitter(eventBrokerHost, shutdownTopic)

    root = PropertySet()
    # Shutdown at level 1 : stop immediately by killing process (ugly)
    # Shutdown at level 2 : exit in a clean manner (Pipeline and Slices) at a synchronization point
    # Shutdown at level 3 : exit in a clean manner (Pipeline and Slices) at the end of a Stage
    # Shutdown at level 4 : exit in a clean manner (Pipeline and Slices) at the end of a Visit
    root.setInt("level", 3)

    externalEventTransmitter.publish(root)

