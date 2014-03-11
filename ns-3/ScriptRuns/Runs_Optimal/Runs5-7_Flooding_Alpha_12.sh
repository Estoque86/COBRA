#!/bin/bash

PATHSIM=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/VERIFICHE_SIMULAZIONI/Flooding/Alpha_12
PATHLOG=/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/ns-3/LOG_FILES/Flooding/Alpha_12

######  5  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 5" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=5 --RngSeed=1" > ${PATHLOG}/log5_Flooding_Alpha_12.out 2>&1

echo 'END SIM 5' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  6  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 6" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=6 --RngSeed=1" > ${PATHLOG}/log6_Flooding_Alpha_12.out 2>&1

echo 'END SIM 6' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM



######  7  #######
echo -e '\n' >> ${PATHSIM}/VerificaSIM

echo -e "START SIM 7" >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM

./waf --run "scratch/bloom-filter-routing-scenario --valoreAlpha=12 --contentCatalogFib=104450 --desiredPfpFib=0.05 --cellWidthBfFib=1 --bfScope=fib --bfTypeFib=simple --bfFibInitMethod=optimal --cacheToCatalog=0.003 --simType=1 --ns3::ndn::ConsumerCbr::Randomize=uniform --ns3::ndn::Consumer::LifeTime=2s --ns3::ndn::Consumer::RetxTimer=25ms --RngRun=7 --RngSeed=1" > ${PATHLOG}/log7_Flooding_Alpha_12.out 2>&1

echo 'END SIM 7' >> ${PATHSIM}/VerificaSIM
date >> ${PATHSIM}/VerificaSIM
