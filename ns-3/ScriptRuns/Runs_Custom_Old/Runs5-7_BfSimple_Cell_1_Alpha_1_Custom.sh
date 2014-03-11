#!/bin/bash

PATHSIM=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/VERIFICHE_SIMULAZIONI/BloomFilter/Simple/Cell_1/Alpha_1
PATHLOG=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/LOG_FILES/BloomFilter/Simple/Cell_1/Alpha_1

######  5  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 5" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=1 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=custom --cacheToCatalog=0.003 --customLengthBfFib=325634 --customHashesBfFib=2 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=5 --RngSeed=1" > ${PATHLOG}/log5_BfSimple_Cell_1_Alpha_1.out 2>&1

echo 'END SIM 5' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  6  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 6" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=1 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=custom --cacheToCatalog=0.003 --customLengthBfFib=325634 --customHashesBfFib=2 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=6 --RngSeed=1" > ${PATHLOG}/log6_BfSimple_Cell_1_Alpha_1.out 2>&1

echo 'END SIM 6' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  7  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 7" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=1 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=custom --cacheToCatalog=0.003 --customLengthBfFib=325634 --customHashesBfFib=2 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=7 --RngSeed=1" > ${PATHLOG}/log7_BfSimple_Cell_1_Alpha_1.out 2>&1

echo 'END SIM 7' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM
