#!/bin/bash

PATHSIM=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/VERIFICHE_SIMULAZIONI/BloomFilter/Stable/Cell_3/Alpha_1
PATHLOG=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/LOG_FILES/BloomFilter/Stable/Cell_3/Alpha_1

######  5  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 5" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=1 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=3 --bfScope=fib --bfTypeFib=stable --bfFibInitMethod=optimal --cacheToCatalog=0.001 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=5 --RngSeed=1" > ${PATHLOG}/log5_BfStable_Cell_3_Alpha_1.out 2>&1

echo 'END SIM 5' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  6  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 6" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=1 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=3 --bfScope=fib --bfTypeFib=stable --bfFibInitMethod=optimal --cacheToCatalog=0.001 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=6 --RngSeed=1" > ${PATHLOG}/log6_BfStable_Cell_3_Alpha_1.out 2>&1

echo 'END SIM 6' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  7  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 7" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=1 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=3 --bfScope=fib --bfTypeFib=stable --bfFibInitMethod=optimal --cacheToCatalog=0.001 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=7 --RngSeed=1" > ${PATHLOG}/log7_BfStable_Cell_3_Alpha_1.out 2>&1

echo 'END SIM 7' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM
