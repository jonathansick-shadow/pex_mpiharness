// -*- lsst-c++ -*-

/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */
 
/** \file Slice.h
  *
  * \ingroup harness
  *
  * \brief   Slice represents a single parallel worker program.  
  *
  * \author  Greg Daues, NCSA
  */

#ifndef LSST_PEX_MPIHARNESS_SLICE_H
#define LSST_PEX_MPIHARNESS_SLICE_H


#include <string>
#include <unistd.h>
#include <list>
#include <vector>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#include "lsst/pex/harness/TracingLog.h"
#include "lsst/pex/policy/Policy.h"
#include "lsst/utils/Utils.h"

#include "lsst/daf/base/PropertySet.h"
#include "lsst/pex/logging/Component.h"
#include "lsst/pex/logging/LogRecord.h"
#include "lsst/pex/logging/LogDestination.h"
#include "lsst/pex/logging/LogFormatter.h"
#include "lsst/pex/logging/Log.h"
#include "lsst/pex/logging/DualLog.h"
#include "lsst/pex/logging/ScreenLog.h"
#include "lsst/ctrl/events/EventLog.h"
#include "lsst/pex/harness/LogUtils.h"
#include "lsst/pex/exceptions.h"

#include <boost/mpi.hpp>
#include <boost/mpi/allocator.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "mpi.h"

using namespace lsst::daf::base;
using namespace lsst::pex::harness;
using namespace lsst::pex::policy;

using namespace std;
using namespace lsst;
using namespace boost;

namespace lsst {
namespace pex {
namespace mpiharness {

/**
  * \brief   Slice represents a single parallel worker program.  
  *
  *          Slice executes the loop of Stages for processing a portion of an Image (e.g.,
  *          single ccd or amplifier). The processing is synchonized with serial processing 
  *          in the main Pipeline via MPI communications.    
  */

class Slice {

public:
    Slice(const std::string& pipename="unnamed"); // constructor

    ~Slice(); // destructor

    void initialize();

    void invokeBcast(int iStage);
    void invokeBarrier(int iStage);
    void invokeShutdownTest();
    void shutdown();
    void setRank(int rank);
    int getRank();
    int getUniverseSize();
    void setTopology(Policy::Ptr policy); 
    void setRunId(char* runId);
    char* getRunId();
    void calculateNeighbors();
    std::vector<int> getRecvNeighborList();
    PropertySet::Ptr syncSlices(PropertySet::Ptr dpt);

    void setPipelineName(const std::string& name) {
        _pipename = name;
    }
    const std::string& getPipelineName() {  return _pipename;  }


private:
    void initializeMPI();
    void configureSlice();

    int _pid;
    int _rank;
    Policy::Ptr _topologyPolicy; 
    char* _runId;

    MPI_Comm sliceIntercomm;
    MPI_Comm topologyIntracomm;
    boost::mpi::communicator world;

    int mpiError;
    int nStages;
    int universeSize;
    int bufferSize;
    std::list<int> neighborList;
    std::list<int> sendNeighborList;
    std::list<int> recvNeighborList;
    string neighborString;

    std::string _pipename;

    LogUtils _logutils;
};

    	} // namespace mpiharness

    } // namespace pex

} // namespace lsst

#endif // LSST_PEX_MPIHARNESS_SLICE_H

