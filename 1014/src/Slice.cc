// -*- lsst-c++ -*-
/** \file Slice.cc
  *
  * \ingroup mpiharness
  *
  * \brief   Slice represents a single parallel worker program.
  *
  *          Slice executes the loop of Stages for processing a portion of an Image (e.g.,
  *          single ccd or amplifier). The processing is synchonized with serial processing
  *          in the main Pipeline via MPI communications.
  *
  * \author  Greg Daues, NCSA
  */


#include "lsst/pex/mpiharness/Slice.h"
#include "lsst/pex/logging/Log.h"
#include <lsst/pex/policy/Policy.h>

namespace pexPolicy = lsst::pex::policy;

using lsst::pex::logging::Log;

namespace lsst {
namespace pex {
namespace mpiharness {

/** 
 * Constructor.
 * @param pipename   a name to identify the pipeline.  This is used in setting 
 *                      up the logger.
 */
Slice::Slice(const std::string& pipename) 
    : _pid(getpid()), _rank(-2),   _pipename(pipename),  _logutils(LogUtils()) 
{ }

/** Destructor.
 */
Slice::~Slice(void) {
    delete &(_logutils); 
}


/** Initialize the MPI environment of the Slice.
 * In doing so, obtain a reference to the Intercommunicator of the 
 * Pipeline and the Slices. Find and record the Slice rank and the
 * universe size.
 */
void Slice::initializeMPI() {

    mpiError = MPI_Init(NULL, NULL);  
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    mpiError = MPI_Comm_get_parent(&sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    if (sliceIntercomm == MPI_COMM_NULL) {
        MPI_Finalize();
        exit(1);
    }

    int intercommsize;
    int intercommrank;

    mpiError = MPI_Comm_remote_size(sliceIntercomm, &intercommsize);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    if (intercommsize != 1) {
        MPI_Finalize();
        exit(1);
    }

    mpiError = MPI_Comm_rank(sliceIntercomm, &intercommrank);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    _rank = intercommrank;

    int flag;
    int *universeSizep;
    mpiError = MPI_Attr_get(sliceIntercomm, MPI_UNIVERSE_SIZE, &universeSizep, &flag);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }
    universeSize = *universeSizep;

    return;
}

/** Set configuration for the Slice.
 */
void Slice::configureSlice() {

    bufferSize = 256;
    return;
}

/** Initialize the environment of the Slice.
 */
void Slice::initialize() {

    initializeMPI();

    configureSlice();

    return;
}

/** Invoke the Shutdown test from the Pipeline. 
 * This is done by receiving a message from the Pipeline, and if 
 * instructed, running shutdown on ths Slice.
 */
void Slice::invokeShutdownTest() {

    char shutdownCommand[bufferSize];

    mpiError = MPI_Bcast(shutdownCommand, bufferSize, MPI_CHAR, 0, sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }


    if(strcmp(shutdownCommand, "SHUTDOWN")) {
    }
    else {
        shutdown();
    }

}

/** Invoke the MPI_Bcast in coordination with the Pipeline (prior to 
 * running the process() method.)
 */
void Slice::invokeBcast(int iStage //!< The integer index of the current Stage
                        ) {

    char runCommand[bufferSize];
    int kStage;

    Log sliceLog(_logutils.getLogger(), "invokeBcast.cpp");

    Log localLog(sliceLog, "invokeBcast()");    
    localLog.log(Log::INFO, boost::format("Invoking Bcast: %d ") % iStage);

    mpiError = MPI_Bcast(runCommand, bufferSize, MPI_CHAR, 0, sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    mpiError = MPI_Bcast(&kStage, 1, MPI_INT, 0, sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

}

/** Invoke the MPI_Barrier in coordination with the Pipeline (after the 
 * excution of the process() method.)
 */
void Slice::invokeBarrier(int iStage //!< The integer index of the current Stage 
                          ) {

    Log sliceLog(_logutils.getLogger(), "invokeBarrier.cpp");

    Log localLog(sliceLog, "invokeBarrier()");    
    localLog.log(Log::INFO, boost::format("Invoking Barrier: %d ") % iStage);

    mpiError = MPI_Barrier(sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

}


/** Shutdown the Slice by calling MPI_Finalize and then exit(). 
 */
void Slice::shutdown() {

    MPI_Finalize();
    exit(0);
}

/** set method for Slice MPI rank
 */
void Slice::setRank(int rank) {
    _rank = rank;
}

/** get method for Slice MPI rank
 */
int Slice::getRank() {
    return _rank;
}

/** get method for  MPI universe size
 */
int Slice::getUniverseSize() {
    return universeSize;
}

/** set method for the Slice topology, which is described by a Policy 
 */
void Slice::setTopology(pexPolicy::Policy::Ptr policy//!< A smart pointer to a Policy  
                        ) {
    _topologyPolicy = policy;
}

/** set method for the overall runid of the Pipeline plus all Slices
 */
void Slice::setRunId(char* runId) {
    _runId = runId;
}

/** get method for the overall runid of the Pipeline plus all Slices
 */
char* Slice::getRunId() {
    return _runId;
}

/** Get a list of ranks of neighbors Slices from which this Slice receives data
 * @returns a std vector containing integer indices of the ranks of neighbor Slices 
 * from which this Slice receives data
 */
std::vector<int> Slice::getRecvNeighborList() {
    std::vector<int> neighborVec;
    std::list<int>::iterator iter;
    for(iter = recvNeighborList.begin(); iter != recvNeighborList.end(); ) {
       neighborVec.push_back(*iter);
       iter++;
    }
    return neighborVec;
}

/** Calculate the ranks of the neighbors Slices for this Slice.  The calculation 
 * relies on the topology that has been set for the Pipeline plus Slices, 
 * and the result is stored as a list of Slices from which this Slice receives 
 * data (recvNeighborList) and sends (sendNeighborList). 
 */
void Slice::calculateNeighbors() {

    Log sliceLog(_logutils.getLogger(), "calculateNeighbors.cpp");

    Log localLog(sliceLog, "calculateNeighbors()");  

    std::string typeTopology; 
    if (_topologyPolicy->exists("type")) {
        typeTopology = _topologyPolicy->getString("type");  
    }

    localLog.log(Log::INFO,
        boost::format("Checking the topology: %s ") % typeTopology);

    int wrank = world.rank();

    localLog.log(Log::INFO,
        boost::format("Checking the ranks within communicators: sliceIntercomm world  %d  %d ") % _rank % wrank );

    if (typeTopology == "ring") {  
        int commSize, isPeriodic;
        int right_nbr, left_nbr;
        isPeriodic = 1;
        MPI_Comm_size(MPI_COMM_WORLD, &commSize );
        MPI_Cart_create(MPI_COMM_WORLD, 1, &commSize, &isPeriodic, 0, &topologyIntracomm );
        MPI_Cart_shift( topologyIntracomm, 0, 1, &left_nbr, &right_nbr );

        neighborList.push_back(left_nbr);
        neighborList.push_back(right_nbr);

        string mode;
        mode = _topologyPolicy->getString("param1");
        if(mode == "clockwise") {
            recvNeighborList.push_back(left_nbr);
            sendNeighborList.push_back(right_nbr);
            localLog.log(Log::INFO, boost::format("Mode is %s ") % mode );
        }
        if(mode == "counterclockwise") {
            sendNeighborList.push_back(left_nbr);
            recvNeighborList.push_back(right_nbr);
            localLog.log(Log::INFO, boost::format("Mode is %s ") % mode );
        }
    }   

    if (typeTopology == "sliceleaders") {  
        int modulus;
        int leaderRank;
        bool _isLeader;
        modulus = _topologyPolicy->getInt("param1");
        localLog.log(Log::INFO,
               boost::format("sliceleaders  modulus %d  ") % modulus  );
        int groupRank =  _rank % modulus;   /* groupRank is either 0,  or 1, 2, ,,,, N-1 */ 
        if (groupRank == 0) {
            _isLeader = true;
            leaderRank = _rank;
            for (int ii = 1; ii < modulus; ii++) {
                neighborList.push_back(leaderRank+ii);
                recvNeighborList.push_back(leaderRank+ii);
            }
            localLog.log(Log::INFO,
               boost::format("sliceleaders  %d  %d is Leader ") % _rank % leaderRank );
        } 
        else {
            _isLeader = false;
            leaderRank = _rank - groupRank;
            neighborList.push_back(leaderRank);
            sendNeighborList.push_back(leaderRank);
            localLog.log(Log::INFO,
               boost::format("sliceleaders  %d  %d not a Leader ") % _rank % leaderRank );
        }
    }

    if (typeTopology == "focalplane") {  
        int commSize[2], isPeriodic[2];
        int rightx, leftx;
        int righty, lefty;
        isPeriodic[0] = 1;
        isPeriodic[1] = 1;

        commSize[0] = _topologyPolicy->getInt("param1");
        commSize[1] = _topologyPolicy->getInt("param2");

        MPI_Cart_create(MPI_COMM_WORLD, 2, commSize, isPeriodic, 0, &topologyIntracomm );
        MPI_Cart_shift( topologyIntracomm, 0, 1, &leftx, &rightx );
        MPI_Cart_shift( topologyIntracomm, 1, 1, &lefty, &righty );

        neighborList.push_back(leftx);
        neighborList.push_back(rightx);
        neighborList.push_back(lefty);
        neighborList.push_back(righty);

        sendNeighborList.push_back(leftx);
        sendNeighborList.push_back(rightx);
        sendNeighborList.push_back(lefty);
        sendNeighborList.push_back(righty);

        recvNeighborList.push_back(leftx);
        recvNeighborList.push_back(rightx);
        recvNeighborList.push_back(lefty);
        recvNeighborList.push_back(righty);

        localLog.log(Log::INFO, boost::format("calculateNeighbors(): %d leftx %d ") % _rank % leftx);
        localLog.log(Log::INFO, boost::format("calculateNeighbors(): %d rightx %d") % _rank % rightx);
        localLog.log(Log::INFO, boost::format("calculateNeighbors(): %d lefty %d") % _rank % lefty);
        localLog.log(Log::INFO, boost::format("calculateNeighbors(): %d righty %d") % _rank % righty);
    }   

}

/** Perform the interSlice communication, i.e., synchronized the Slices. 
 * @return A smart pointer to the PropertySet of values that has been received 
 */
PropertySet::Ptr Slice::syncSlices(PropertySet::Ptr ps0Ptr //!< A smart pointer to a PropertySet of values to communicate 
                                   ) {
    Log sliceLog(_logutils.getLogger(), "syncSlices.cpp");

    Log localLog(sliceLog, "syncSlices()");    

    char syncCommand[bufferSize];
    localLog.log(Log::INFO, boost::format("InterSlice Communcation Command Bcast: rank %d ") % _rank);

    mpiError = MPI_Bcast(syncCommand, bufferSize, MPI_CHAR, 0, sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    /* Ptr for the return values received from other Slices */ 
    PropertySet::Ptr retPtr(new PropertySet);

    std::vector<std::string> psNames = ps0Ptr->names();
    std::string keyToShare = psNames[0];
    localLog.log(Log::INFO, boost::format("Using keyToShare: %s ") % keyToShare);

    int numRequests, numSendNeighbors, numRecvNeighbors; 
    numSendNeighbors = sendNeighborList.size();
    numRecvNeighbors = recvNeighborList.size();
    numRequests = numSendNeighbors + numRecvNeighbors; 

    localLog.log(Log::INFO, 
        boost::format("Number of Neighbors is: Send  %d Recv %d  ") % numSendNeighbors % numRecvNeighbors);
    localLog.log(Log::INFO, 
        boost::format("Number of Requests: %d ") % numRequests );
    
    int count = 0;

    PropertySet::Ptr recvPtr[numRecvNeighbors];
    mpi::request reqs[numRequests];

    int destSlice, sendCount;
    sendCount = 0;
    std::list<int>::iterator iterSend;
    for(iterSend = sendNeighborList.begin(); iterSend != sendNeighborList.end(); iterSend++) {
        destSlice = (*iterSend);

        localLog.log(Log::INFO, boost::format("Communicating value to Slice %d ") % destSlice);

        reqs[count] = world.isend(destSlice, 0, ps0Ptr);

        count++;
        sendCount++;
    }

    localLog.log(Log::INFO, boost::format("After isends: %d ") % _rank);

    int srcSlice, recvCount;
    recvCount=0;
    std::list<int>::iterator iterRecv;
   
    for(iterRecv = recvNeighborList.begin(); iterRecv != recvNeighborList.end(); iterRecv++) {
        srcSlice = (*iterRecv);

        /* perform Recvs */
        localLog.log(Log::INFO, boost::format("Before recv call from Slice %d ") % srcSlice);
        reqs[count] = world.irecv(srcSlice, 0, recvPtr[recvCount]);
        localLog.log(Log::INFO, boost::format("After recv from  Slice %d recvCount %d ") % srcSlice %  recvCount);

        count++;
        recvCount++;
    }

    mpi::wait_all(reqs, reqs + sendCount + recvCount);

    localLog.log(Log::INFO, boost::format("Past wait_all %d ") % srcSlice);

    /* Combine the array of recvPtr [] into a single retPtr */ 
    int yy = 0; 
    std::list<int>::iterator iterNeighbors;
    for(iterNeighbors = recvNeighborList.begin(); iterNeighbors != recvNeighborList.end(); ) {
        int ni =  (*iterNeighbors);
        std::stringstream newkeyBuffer;
        std::string newkey;
        newkeyBuffer << "neighbor-";
        newkeyBuffer << ni;
        newkeyBuffer >> newkey;
        retPtr->set<PropertySet::Ptr>(newkey, recvPtr[yy]); 

        iterNeighbors++;
        yy++;
    }

    mpiError = MPI_Barrier(sliceIntercomm);
    if (mpiError != MPI_SUCCESS){
        MPI_Finalize();
        exit(1);
    }

    return retPtr; 

}


}
}
}
