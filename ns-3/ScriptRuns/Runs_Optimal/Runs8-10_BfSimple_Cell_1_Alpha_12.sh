#!/bin/bash

PATHSIM=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/VERIFICHE_SIMULAZIONI/BloomFilter/Simple/Cell_1/Alpha_12
PATHLOG=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/LOG_FILES/BloomFilter/Simple/Cell_1/Alpha_12

######  8  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 8" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=8 --RngSeed=1" > ${PATHLOG}/log8_BfSimple_Cell_1_Alpha_12.out 2>&1

echo 'END SIM 8' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  9  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 9" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=9 --RngSeed=1" > ${PATHLOG}/log9_BfSimple_Cell_1_Alpha_12.out 2>&1

echo 'END SIM 9' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  10  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 10" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=3 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=10 --RngSeed=1" > ${PATHLOG}/log10_BfSimple_Cell_1_Alpha_12.out 2>&1

echo 'END SIM 10' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM
