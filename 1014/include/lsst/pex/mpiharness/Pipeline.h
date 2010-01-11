// -*- lsst-c++ -*-
/** \file Pipeline.h
  *
  * \ingroup harness
  *
  * \brief   Pipeline class manages the operation of a multi-stage parallel pipeline.
  *
  * \author  Greg Daues, NCSA
  */

#ifndef LSST_PEX_MPIHARNESS_PIPELINE_H
#define LSST_PEX_MPIHARNESS_PIPELINE_H

#include "mpi.h"

#include <string>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#include "lsst/pex/policy/Policy.h"
#include "lsst/utils/Utils.h"

#include "lsst/pex/harness/TracingLog.h"
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
#include <boost/shared_ptr.hpp>

using namespace lsst::daf::base;
using namespace lsst::pex::harness;

using namespace std;
using namespace lsst;
using namespace boost;

using lsst::pex::logging::Log;

namespace lsst {
namespace pex {
namespace mpiharness {


/**
  * \brief   Pipeline class manages the operation of a multi-stage parallel pipeline.
  *
  *          Pipeline spawns Slice workers and coordinates serial and parallel processing 
  *          between the main thread and the workers by means of MPI communciations.
  *          Pipeline loops over the collection of Stages for processing on Image.
  *          The Pipeline is configured by reading a Policy file.
  */

class Pipeline {
public:
    Pipeline(const std::string& name="unnamed"); // constructor

    ~Pipeline(); // destructor

    void initialize();

    void startSlices();  
    void invokeProcess(int iStage);
    void invokeShutdown();
    void invokeContinue();
    void invokeSyncSlices(); 

    void shutdown();

    int getUniverseSize();

    void setRunId(char* runId);
    char* getRunId();

    void setPolicyName(char* policyName);
    char* getPolicyName();

    void setPipelineName(const std::string& name) {
        _pipename = name;
    }
    const std::string& getPipelineName() {  return _pipename;  }

private:
    void initializeMPI();
    void configurePipeline();  
    void initializeQueues();  
    void initializeStages();  

    int _pid;
    char* _runId;
    char* _policyName;

    MPI_Comm sliceIntercomm;

    int nStages;
    int nSlices;
    int bufferSize;
    int mpiError;
    int rank;
    int size;
    int universeSize;

    std::string _pipename;

    LogUtils _logutils;
};

} // namespace mpiharness 

} // namespace pex 

} // namespace lsst

#endif // LSST_PEX_MPIHARNESS_PIPELINE_H 

