#!/bin/sh

pwd=`pwd`
# LSST_POLICY_DIR=${pwd}/policy
# export LSST_POLICY_DIR
# echo LSST_POLICY_DIR ${LSST_POLICY_DIR} 

# Command line arguments 
echo $0 $@  
# echo $#
if [ "$#" -lt 5 ]; then
   echo "---------------------------------------------------------------------"
   echo "Usage:  $0 <policy-file-name> <runId> <nodelist-file>" \
        "<node-count> <proc-count> [ <verbsity> ] [ <name> ]"
   echo "---------------------------------------------------------------------"
   exit 0
fi

pipelinePolicyName=${1}
runId=${2}
nodelist=${3}
nodes=${4}
usize=${5}
verbosity=${6}
name=${7}

localnode=`hostname | sed -e 's/\..*$//'`
localncpus=`sed -e 's/#.*$//' $nodelist | egrep $localnode'|localhost' | sed -e 's/^.*://'`

# Subtract 1 to the number of slices to get the universe size 
nslices=$(( $usize - 1 ))

echo "nodes ${nodes}"
echo "nslices ${nslices}"
echo "usize ${usize}"
echo "ncpus ${localncpus}"
if [ -z "$name" ]; then
   echo "name: default"
else
   echo "name ${verbosity}"
   name="-n $name"
fi
if [ -z "$verbosity" ]; then
   echo "verbosity: default"
else
   echo "verbosity ${verbosity}"
   verbosity="-L $verbosity"
fi

# MPI commands will be in PATH if mpich2 is in build
echo "Running mpdboot"

echo mpdboot --totalnum=${nodes} --file=$nodelist --ncpus=$localncpus --verbose
mpdboot --totalnum=${nodes} --file=$nodelist --ncpus=$localncpus --verbose

sleep 3s
echo "Running mpdtrace"
echo mpdtrace -l
mpdtrace -l
sleep 2s

echo "Running mpiexec"

echo mpiexec -usize ${usize} -machinefile ${nodelist} -np 1 -envall runMpiPipeline.py ${pipelinePolicyName} ${runId} ${verbosity} ${name} 
mpiexec -usize ${usize}  -machinefile ${nodelist} -np 1 -envall runMpiPipeline.py ${pipelinePolicyName} ${runId} ${verbosity} ${name} 

sleep 1s

echo "Running mpdallexit"
echo mpdallexit
mpdallexit

