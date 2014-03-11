#!/bin/bash

PATHSIM=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/VERIFICHE_SIMULAZIONI/Flooding/Alpha_12
PATHLOG=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/LOG_FILES/Flooding/Alpha_12

######  1  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 1" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=1 --RngSeed=1" > ${PATHLOG}/log1_Flooding_Alpha_12.out 2>&1

echo 'END SIM 1' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  2  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 2" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=2 --RngSeed=1" > ${PATHLOG}/log2_Flooding_Alpha_12.out 2>&1

echo 'END SIM 2' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  3  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 3" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=3 --RngSeed=1" > ${PATHLOG}/log3_Flooding_Alpha_12.out 2>&1

echo 'END SIM 3' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM


######  4  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 4" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=4 --RngSeed=1" > ${PATHLOG}/log4_Flooding_Alpha_12.out 2>&1

echo 'END SIM 4' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM
