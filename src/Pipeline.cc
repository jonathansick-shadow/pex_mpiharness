// -*- lsst-c++ -*-
/** \file Pipeline.cc
  *
  * \ingroup mpiharness
  *
  * \brief   Pipeline class manages the operation of a multi-stage parallel MPI pipeline.
  *
  *          Pipeline spawns Slice workers and coordinates serial and parallel processing
  *          between the main thread and the workers by means of MPI communciations.
  *          Pipeline loops over the collection of Stages for processing on Image.
  *          The Pipeline is configured by reading a Policy file.
  *
  * \author  Greg Daues, NCSA
  */
#include <cstring>
#include <sstream>

#include "lsst/pex/mpiharness/Pipeline.h"

using lsst::pex::logging::Log;

namespace lsst {
namespace pex {
namespace mpiharness {

/** 
 * Constructor.
 * @param name   a name to identify the pipeline.  This is used in setting 
 *                 up the logger.
 */
Pipeline::Pipeline(const std::string& name) 
    : _pid(getpid()),  _pipename(name), _logutils(LogUtils())
{ }

/** Destructor.
 */
Pipeline::~Pipeline(void) {
    delete &(_logutils);
}

/** Initialize the environment of the Pipeline.
 */
void Pipeline::initialize() {

    initializeMPI();

    configurePipeline();

    return;
}


/** Initialize the MPI environment of the Pipeline.
 * Check the rank, size of MPI_COMM_WORLD, and the universe size 
 * prior to the spawning of the Slices. 
 */
void Pipeline::initializeMPI() {
  
  mpiError = MPI_Init (NULL, NULL); 
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }

    mpiError = MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    mpiError = MPI_Comm_size(MPI_COMM_WORLD, &size); 
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    int flag;
    int *universeSizep;
    mpiError = MPI_Attr_get(MPI_COMM_WORLD, MPI_UNIVERSE_SIZE, &universeSizep, &flag);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }
    universeSize = *universeSizep;

    nSlices = universeSize-1;

    return;
}

/** Set configuration for the Pipeline.
 */
void Pipeline::configurePipeline() {
    bufferSize = 256;
    return;
}

/** get method for the universe size
 */ 
int Pipeline::getUniverseSize() {
    return universeSize;
}

/** Spawn the Slice workers for parallel computation. 
 * This is accomplished using MPI_Comm_spawn and creates an Intercommunicator sliceIntercomm.
 * The number of Slices to be spawned nSlices is one less than the designated universe size.
 */ 
void Pipeline::startSlices() {
    int *array_of_errcodes;
    array_of_errcodes = (int *) malloc(4 * sizeof (int));

    std::ostringstream levsb;
    levsb << _logutils.getLogger().getThreshold();
    string levstr(levsb.str());
    char *lev = (char *) malloc(levstr.length());
    strcpy(lev, levstr.c_str());

    int errcodes[nSlices];
    char *myexec  = "runMpiSlice.py";
    char *argv[] = {_policyName, _runId, "-l", lev, NULL};
    if (_logutils.getLogger().sends(Log::DEBUG)) {
        Log log(_logutils.getLogger(), "startSlices.cpp");
        std::ostringstream spawncmd;
        spawncmd << myexec;
        char **arg = argv;
        while (*arg != NULL) 
            spawncmd << " " << *(arg++);
        log.log(Log::DEBUG, spawncmd.str());
    }

    mpiError = MPI_Comm_spawn(myexec, argv, nSlices, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &sliceIntercomm, errcodes); 

    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    return;
}

/** Broadcast a Shutdown message to all of the Slices.
 */
void Pipeline::invokeShutdown() {

    char procCommand[bufferSize];

    std::strcpy(procCommand, "SHUTDOWN");  

    mpiError = MPI_Bcast((void *)procCommand, bufferSize, MPI_CHAR, MPI_ROOT, sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }

    return;

}

/** Broadcast a "Continue" message to all of the Slices. 
 * This is used to tell Slices to continue processing (no shutdown event received). 
 */
void Pipeline::invokeContinue() {

    char procCommand[bufferSize];

    std::strcpy(procCommand, "CONTINUE");  

    mpiError = MPI_Bcast((void *)procCommand, bufferSize, MPI_CHAR, MPI_ROOT, sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }

    return;
}

/** Tell the Slices to perform the interSlice communication, i.e., synchronized the Slices.
 */
void Pipeline::invokeSyncSlices() {


    Log log(_logutils.getLogger(), "invokeSyncSlices.cpp");
    
    log.log(Log::INFO,
        boost::format("Start invokeSyncSlices: rank %d ") % rank);

    log.log(Log::INFO,
        boost::format("InterSlice Communication Command Bcast rank %d ") % rank);

    char procCommand[bufferSize];
    std::strcpy(procCommand, "SYNC");  

    mpiError = MPI_Bcast((void *)procCommand, bufferSize, MPI_CHAR, MPI_ROOT, sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }

    log.log(Log::INFO,
        boost::format("End Bcast rank %d ") % rank);

    mpiError = MPI_Barrier(sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }

    log.log(Log::INFO,
        boost::format("End invokeSyncSlices rank %d ") % rank);
}

/** Tell the Slices to call the process method for the current Stage.
 */
void Pipeline::invokeProcess(int iStage) {

    Log log(_logutils.getLogger(), "invokeProcess.cpp");


    char processCommand[nSlices][bufferSize];
    for (int k = 0 ; k < nSlices; k++) {
        std::strcpy(processCommand[k], "PROCESS");
    }

    char procCommand[bufferSize];

    std::strcpy(procCommand, "PROCESS");  

    mpiError = MPI_Bcast((void *)procCommand, bufferSize, MPI_CHAR, MPI_ROOT, sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }


    mpiError = MPI_Bcast(&iStage, 1, MPI_INT, MPI_ROOT, sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }


    mpiError = MPI_Barrier(sliceIntercomm);
    if (mpiError != MPI_SUCCESS) {
        MPI_Finalize();
        exit(1);
    }

    return;
}

/** Shutdown the Pipeline by calling MPI_Finalize and then exit().
 */
void Pipeline::shutdown() {

    MPI_Finalize(); 
    exit(0);

}

/** set method for the overall runid of the Pipeline plus all Slices
 */
void Pipeline::setRunId(char* runId) {
    _runId = runId;
    return;
}

/** get method for the overall runid of the Pipeline plus all Slices
 */
char* Pipeline::getRunId() {
    return _runId;
}

/** set method for the Pipeline policy filename
 */
void Pipeline::setPolicyName(char* policyName) {
    _policyName = policyName;
    return;
}

/** get method for the Pipeline policy filename
 */
char* Pipeline::getPolicyName() {
    return _policyName;
}


}
}
}
