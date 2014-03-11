/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Ilya Moiseenko <iliamo@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-grid.h"
#include "ns3/mobility-module.h"
#include "../src/ndnSIM/helper/ndn-global-routing-helper.h"
#include "../src/ndnSIM/model/ndn-global-router.h"
#include "../src/ndnSIM/model/bloom-filter/bloom-filter.h"
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <fstream>
#include "ns3/random-variable-stream.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <boost/tokenizer.hpp>
#include <boost/ref.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>


using namespace ns3;
using namespace ndn;

NS_LOG_COMPONENT_DEFINE ("ndn.Grid.new");

std::vector<std::vector<bool> > readAdjMat (std::string adj_mat_file_name);
std::vector<std::vector<double> > readCordinatesFile (std::string node_coordinates_file_name);
void InterestScartatiFailureTrace(Ptr<OutputStreamWrapper> stream);
void InterestInviatiRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header, Ptr<const Face> face);
void DataInviatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, bool local, Ptr<const Face> face);
void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, Ptr<const Face> face);
void DataInCaheAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header);
void EntryExpTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header);

//void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, int64_t time, uint32_t dist);
//void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, int64_t time);
void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist);
void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist);

//void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header);
void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent);

//void DataScartatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, uint32_t dropUnsol);

void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent);

void InterestInviatiAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* interest);

/**
 * This scenario simulates COBRA using the GEANT topology as a core network.
 */


int
main (int argc, char *argv[6])
{
  // ** [MT] ** Increase the number of files that can be opened at the same time.
  struct rlimit newlimit;
  const struct rlimit * newlimitP;
  newlimitP = NULL;
  newlimit.rlim_cur = 2000;
  newlimit.rlim_max = 2000;
  newlimitP = &newlimit;
  setrlimit(RLIMIT_NOFILE,newlimitP);


  // ** [MT] ** Setting useful global variables, such as simulation type (flooding, bestRoute, BloomFilter) and current run
  static ns3::GlobalValue g_simRun ("g_simRun",
                                   "The number of the current simulation in the same scenario",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());

  static ns3::GlobalValue g_simType ("g_simType",
                                   "The choosen Forwarding Strategy",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());


  Time finishTime = Seconds (115000.0);

  // ** [MT] ** Parameters passed with command line
  uint32_t contentCatalog = 0;          // Estimated Content Catalog Cardinality
  double cacheToCatalog = 0;            // cacheSize/contCatalogCardinality ratio
  uint32_t simType = 1;                 // Simulation Scenario:
  	  	  	  	  	  	  	  	  	    // 1 = FLOODING  (Default)
  	  	  	  	  	  	  	  	  	  	// 2 = BEST ROUTE
  	  	  	  	  	  	  	  	  	    // 3 = BF

  uint32_t simRun = 1;                  // Simulation run.

  CommandLine cmd;
  cmd.AddValue ("contentCatalog", "Total number of contents", contentCatalog);
  cmd.AddValue ("cacheToCatalog", "Cache to Catalog Ratio", cacheToCatalog);
  cmd.AddValue ("simType", "Chosen Simulation Scenario", simType);
  cmd.AddValue ("simRun", "Current Simulation Run", simRun);

  cmd.Parse (argc, argv);

  Config::SetGlobal ("g_simRun", UintegerValue(simRun));
  Config::SetGlobal ("g_simType", UintegerValue(simType));
  
  std::string EXT = ".txt";
  std::stringstream scenario;
  std::string extScenario;  		// Estensione per distinguere i vari scenari simulati

  // ** [MT] ** The point of attachment of the content provider network as well as the number of repos is varied between different runs
  Ptr<UniformRandomVariable> myRand = CreateObject<UniformRandomVariable> ();
  
  uint32_t repAttach = (uint32_t)( (int)( myRand->GetValue()*pow(10,5) ) % 22);   // ** [MT] ** The GEANT network has 22 core routers; so, their ID
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  //            goes from 0 to 21.
  
  uint32_t numRepo = (uint32_t) ( ( (int)( myRand->GetValue()*pow(10,5) ) %3)+1); // ** [MT] ** The number of repos in the content provider network goes
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  //            from 1 to 3 (which is the minimum number of host in an
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  //            edge network.
  
  std::string simulationType;

  // ** [MT] ** String to differentiate outputs.
  switch (simType)
  {
  case(1):
	 simulationType = "Flooding";
  	 scenario << simulationType << "_" << simRun << "_" << cacheToCatalog << "_" << repAttach << "_" << numRepo << "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
  	 extScenario = scenario.str();
  	 scenario.str("");
     break;
  case(2):
	 simulationType = "BestRoute";
  	 scenario << simulationType << "_"  << simRun << "_" << cacheToCatalog << "_" << repAttach << "_" << numRepo << "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
     extScenario = scenario.str();
     scenario.str("");
     break;
  case(3):
	 simulationType = "BloomFilter";
  	 scenario << simulationType << "_" << simRun << "_"  << cacheToCatalog << "_" << repAttach << "_" << numRepo <<  "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
     extScenario = scenario.str();
     scenario.str("");
     break;
  default:
	 NS_LOG_UNCOND ("Insert a valid scenario!");
	 exit(1);
	 break;
  }

  uint64_t run = SeedManager::GetRun();
  std::stringstream ssInit;
  std::string runNumber;
  ssInit << run;
  runNumber = ssInit.str();
  ssInit.str("");

  // ** [MT] ** Calculate the cache size according to the specified ratio.
  uint32_t CacheSize = round((double)contentCatalog * cacheToCatalog);
  ssInit << CacheSize;
  std::string CACHE_SIZE = ssInit.str();
  ssInit.str("");


  // ** [MT] ** Create topology by reading topology file
  AnnotatedTopologyReader topologyReader ("", 10);
  topologyReader.SetFileName ("/home/tortelli/ndnSIM_NuovaVersione/ndnSIM_New_Release/Geant_Topo.txt");
  topologyReader.Read ();

  // ** [MT] ** Total number of nodes in the simulated topology
  uint32_t numNodes = topologyReader.GetNodes().GetN();
  NS_LOG_UNCOND("The total number of nodes is:\t" << numNodes);


  // ** [MT] ** In this experiments, real traces from the Boston University are used. Every request is formed by an URI with the various fields
  //            separated by the '/' character. The number of unique contents in these traces is 104452. At every content is, then, assigned
  //            a number of chunk which varies following a geometric distribution with mean = 100.
  //            The number of contents to be stored in each Repo (i.e., REPO_SIZE) is determined according to the number of repos in the simulation.
  //            The names of these contents are read from specific files.
  std::string REPO_SIZE;
  std::string PATH_REPO;
  uint32_t RepoSize;

  switch(numRepo)
  {
  case(1):
		//RepoSize = contentCatalog;
        RepoSize = 104452;
	    ssInit << RepoSize;
		REPO_SIZE = ssInit.str();
		ssInit.str("");
		PATH_REPO= "/home/tortelli/ndnSIM_NuovaVersione/ndnSIM_New_Release/SEED_COPIES/repository1_1.txt";       // One Repo stores all the contents.
	    break;
  case(2):
		//RepoSize = round(contentCatalog/2) + 1;
        RepoSize = (104452/2) + 1;
		ssInit << RepoSize;
		REPO_SIZE = ssInit.str();
		ssInit.str("");
		PATH_REPO= "/home/tortelli/ndnSIM_NuovaVersione/ndnSIM_New_Release/SEED_COPIES/repository2_";            // Two Repos
		break;
  case(3):
		//RepoSize = round(contentCatalog/3) + 3;
        RepoSize = round(104452/3) + 3;
        ssInit << RepoSize;
		REPO_SIZE = ssInit.str();
		ssInit.str("");
		PATH_REPO= "/home/tortelli/ndnSIM_NuovaVersione/ndnSIM_New_Release/SEED_COPIES/repository3_";            // Three Repos
		break;
  default:
	  NS_LOG_UNCOND("Number of Repos not supported!\t" << numRepo);
	  exit(1);
	  break;
  }

  // ** [MT] ** Check both inserted and calculated parameters.
  NS_LOG_UNCOND("CONTENT CATALOG:\t" << contentCatalog << "\n" << "CACHE/CATALOG:\t" << cacheToCatalog << "\n" << "SIM TYPE:\t" << simType << "\n"
		  << "RNG:\t" << runNumber << "\n" << "CACHE SIZE:\t" << CacheSize << "\n" << "REPO ATTACHMENT:\t" << repAttach << "\n" << "NUM_REPO:\t" << numRepo << "\n"
		  << "REPO SIZE" << REPO_SIZE << "\n" << "NUMERO SIM \t" << simRun << "\n");


  // ** [MT] ** INSTALL THE NDN STACK

  NS_LOG_INFO ("Installing Ndn stack on all nodes");
  ndn::StackHelper ndnHelper;
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;

  // ** [MT] ** Change the Forwarding Strategy according to the simulated scenario
  if(simType == 3)
  {
	  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::SelectiveFlooding");                  // ** [MT] ** Selective flooding for BF scenario
	  ndnHelper.SetDefaultRoutes(true);
  }
  else if(simType == 1)
  {
    ndnHelper.SetForwardingStrategy("ns3::ndn::fw::Flooding");
    ndnHelper.SetDefaultRoutes(true);
  }
  else
  {
    ndnHelper.SetForwardingStrategy("ns3::ndn::fw::Flooding");                       	   // ** [MT] ** Disable default routes for BestRoute scenario (simType == 2)
    //ndnHelper.SetDefaultRoutes(true);
  }


  // ** [MT] ** Identify the nodes that will act as content providers according to the node that represents the point of attachment and to the number of repos.
  uint32_t numCoreRouter = 22;
  uint32_t maxEdge = 6;                   // ** [MT] ** Maximum number of host in an edge network.
  std::string core = "Core";
  std::string edge = "Edge";
  std::stringstream ssEdge;
  std::vector<uint32_t> repositoriesID (numRepo);
  std::string rp;

  NodeContainer producerNodes;
  for(uint32_t i = 0; i < numRepo; i++)
  {
	  ssEdge << edge << (repAttach) << "_" << i;
	  rp = ssEdge.str();
	  repositoriesID[i] = Names::Find<Node>(rp)->GetId();   // ** [MT] ** Retrieve the node ID of the repo.
	  if(simType == 3)
		  producerNodes.Add(Names::Find<Node>(rp));
	  ssEdge.str("");
  }


  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
	  bool rep = false;
	  for (uint32_t j = 0; j < numRepo; j++)
	  {
		  if ((*node)->GetId() == repositoriesID[j])
			  rep = true;
	  }
	  if (rep)
	  {
		  // ** [MT] ** For Repo nodes: Cache Size = 0, that is they store only permanent copies.
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", "0");
		  ndnHelper.SetRepository("ns3::ndn::repo::Persistent", "MaxSize", REPO_SIZE);
	  }
	  else
	  {
		  // ** [MT] ** For clients or routers: Repo Size = 0, that is they do not store permanent copies.
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", CACHE_SIZE);
		  ndnHelper.SetRepository("ns3::ndn::repo::Persistent", "MaxSize", "0");
	  }

	  ndnHelper.Install((*node));


	  // ** [MT] ** Acquire all the parameters necessary to initialize the SBFs of each node.
	  if (simType == 3)
      {
    	    Ptr<BloomFilter> bf_p = (*node)->GetObject<BloomFilter>();
	        uint32_t interf = bf_p->num_interfaces_;                                        // Number of interfaces of the node.
	        double   fpBfStable = bf_p->desired_fpBfStable_;                                // Desired False Positive Probability for the SBFs.
	        unsigned long long int   numElem_BfStable = bf_p->proj_numElem_BfStable_;       // Estimated Content Catalog Cardinality.
	        unsigned int   c_Width = bf_p->cell_Width_;                                     // Number of bits per cell (SBF)

	        // ** [MT] ** Initialization function for the BF
	        //(*node)->GetObject<BloomFilter>()->initBf(interf, numero_comp, fpBfStable, fpBfComp, numElem_BfStable, numElem_BfComp, c_Width);
	        (*node)->GetObject<BloomFilter>()->initBf(interf, fpBfStable, numElem_BfStable, c_Width);
	  }

  }

  if(simType == 2)                             // ** [MT] ** Install the GlobalRoutingHelper in case of BestRoute scenario.
  {
	ndnGlobalRoutingHelper.InstallAll ();
  }


  // ** [MT] ** Initialize Repos with SEED copies. Content names are read from files.

  std::string line;
  Ptr<Node> nd;
  const char *PATH_COMPL;
  std::ifstream fin;
  std::stringstream ss;
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("\t");



  Ptr<Node> nodeAttach;
  Ptr<BloomFilter> bfAttach;

  //UintegerValue rn;
  //std::string r_n = "g_simRun";    // Vado ad identificare il run.
  //ns3::GlobalValue::GetValueByNameFailSafe(r_n, rn);
  //uint64_t run_num = rn.Get();

  switch (numRepo)
  {
  case(1):

	// ** [MT] ** There is only one repo.
	nd = NodeList::GetNode(repositoriesID[0]);
  	PATH_COMPL = PATH_REPO.c_str();

  	fin.open(PATH_COMPL);
  	if(!fin) {
  	   	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n";
  	   	exit(0);
  	}

  	// ** [MT] ** Read seed copies from file and insert them inside the repo.
  	while(std::getline(fin, line))
  	{
  	   static ContentObjectTail tail;
  	   Ptr<ContentObject> header = Create<ContentObject> ();
  	   header->SetName (Create<Name>(line));
  	   header->SetTimestamp (Simulator::Now ());
  	   header->SetSignature(0);

  	   //NS_LOG_INFO ("node("<< ((*node)->GetId()+1) <<") uploading ContentObject:\n" << boost::cref(*header) << std::endl);

  	   // ** Virtual payload of 1024 Byte
  	   Ptr<Packet> packet = Create<Packet> (1024);
  	   packet->AddHeader (*header);
  	   packet->AddTrailer (tail);

  	   // ** Add the content to the repo

  	   nd->GetObject<Repo> ()->Add (header, packet);
    }
  	fin.close();
  	break;

  case(2):   // ** [MT] ** PROCEDURA DA RIPENSARE - Nel caso ci siano 2 o 3 repos le info dei relativi contenuti vengono inserite negli SBF
		     //                                   - delle interfacce del nodo d'attacco. Questo evità ambiguità dovute a metching parziali (non con nomi interi)
  case(3):
     uint32_t interf_nodo_attach;
     if(simType == 3)
     {
        if(numRepo == 2)     // numRepo = 2 lo ho con runNum = 3 e runNum = 6
        {
           if(run == 3)
           {
			  nodeAttach = Names::Find<Node>("Core13");              // Nel run = 3 il nodeAttach è il core13
			  bfAttach = nodeAttach->GetObject<BloomFilter>();
		   }
		   else
		   {
			  nodeAttach = Names::Find<Node>("Core15");
			  bfAttach = nodeAttach->GetObject<BloomFilter>();
		   }
        }
        else             // numRepo = 3 lo ho con runNum = 2 e runNum = 5
        {
		   if(run == 2)
           {
				nodeAttach = Names::Find<Node>("Core0");
				bfAttach = nodeAttach->GetObject<BloomFilter>();
		   }
		   else
		   {
		     	nodeAttach = Names::Find<Node>("Core14");
				bfAttach = nodeAttach->GetObject<BloomFilter>();
		   }
        }
     }

     // ** [MT] ** There are more Repos
     for(uint32_t i = 0; i < repositoriesID.size(); i++)
     {
  		if(simType == 3)                              // Sono le interfacce del punto d'attacco verso i repository.
        {
           if(numRepo == 2 && run == 3)
           {
               if(repositoriesID[i] == 69)
                   interf_nodo_attach = 2;
               else if(repositoriesID[i] == 70)
                   interf_nodo_attach = 3;
           }
           else if(numRepo == 2 && run == 6)
           {
               if(repositoriesID[i] == 77)
                    interf_nodo_attach = 5;
               else if(repositoriesID[i] == 78)
                    interf_nodo_attach = 6;
           }
           else if(numRepo == 3 && run == 2)
           {
               if(repositoriesID[i] == 22)
                    interf_nodo_attach = 2;
               else if(repositoriesID[i] == 23)
            	    interf_nodo_attach = 3;
               else if(repositoriesID[i] == 24)
                    interf_nodo_attach = 4;
           }
           else  //num_repo ==3 && run_num == 5
           {
			   if(repositoriesID[i] == 74)
                   interf_nodo_attach = 6;
               else if(repositoriesID[i] == 75)
                   interf_nodo_attach = 7;
               else if(repositoriesID[i] == 76)
                   interf_nodo_attach = 8;
           }
       }

       nd = NodeList::GetNode(repositoriesID[i]);
       ssInit << PATH_REPO << (i+1) << EXT;
  	   PATH_COMPL = ssInit.str().c_str();
       ssInit.str("");

       fin.open(PATH_COMPL);
  	   if(!fin) {
  	       	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n" << PATH_COMPL << "\t" << repositoriesID.size() << "\n";
  	       	exit(0);
  	   }

  	   // ** [MT] ** Read seed copies from file and insert them inside the repo.
  	   while(std::getline(fin, line))
  	   {
           static ContentObjectTail tail;
  		   Ptr<ContentObject> header = Create<ContentObject> ();
  		   header->SetName (Create<Name>(line));
  		   header->SetTimestamp (Simulator::Now ());
  		   header->SetSignature(0);

  		   //NS_LOG_INFO ("node("<< ((*node)->GetId()+1) <<") uploading ContentObject:\n" << boost::cref(*header) << std::endl);

  		   Ptr<Packet> packet = Create<Packet> (1024);
  		   packet->AddHeader (*header);
  		   packet->AddTrailer (tail);

  		   nd->GetObject<Repo> ()->Add (header, packet);

  		   if(simType == 3)
  		   {
             //  L'obiettivo è quello di inserire le info dei contenuti che carico nei repo nei rispettivi BF del router che rappresenta il punto d'attacco
             //  del repo stesso.
             //  Per esempio, if(numRepo == 2), il numero di Repo è due (NODI 69 e 70) e il NODO di ATTACCO è il 13 (Interfaccia 2 verso il 69 e la 3 verso il 70)
  			 bfAttach-> insert_stable_repoAttach(line,interf_nodo_attach);
  		   }

  		}
  		fin.close();
     }
     break;
  }

  // ** Strings for the output files associated to the different traced parameters.

  //**** INTEREST (Inviati e Ricevuti)
  std::string fnInterestInviati = "RISULTATI/INTEREST/Interest_INVIATI/InterestINVIATI_Nodo_";
  std::string fnInterestRicevuti = "RISULTATI/INTEREST/Interest_RICEVUTI/InterestRICEVUTI_Nodo_";
  //std::string fnInterestScartati = "RISULTATI/INTEREST/Interest_SCARTATI/InterestSCARTATI_Nodo_";
  //std::string fnInterestScartatiFailure = "RISULTATI/INTEREST/Interest_SCARTATI_FAILURE/InterestSCARTATI_FAILURE_Nodo_";
  std::string fnInterestAggregati = "RISULTATI/INTEREST/Interest_AGGREGATI/InterestAGGREGATI_Nodo_";
  std::string fnInterestRtx = "RISULTATI/INTEREST/Interest_RITRASMESSI/InterestRITRASMESSI_Nodo_";
  std::string fnInterestMaxRtx = "RISULTATI/INTEREST/Interest_ELIMINATI/InterestELIMINATI_Nodo_";
  std::string fnInterestFileUncomplete = "RISULTATI/INTEREST/Interest_ELIMINATI/FILE/InterestELIMINATI_FILE_Nodo_";

  //**** DATA (Inviati e Ricevuti)
  std::string fnDataInviati = "RISULTATI/DATA/Data_INVIATI/DataINVIATI_Nodo_";
  std::string fnDataRicevuti = "RISULTATI/DATA/Data_RICEVUTI/DataRICEVUTI_Nodo_";
  std::string fnDataInCacheApp = "RISULTATI/DATA/Data_RICEVUTI/APP/Data_RICEVUTI_APP_Nodo_";
  //std::string fnDataScartati = "RISULTATI/DATA/Data_SCARTATI/DataSCARTATI_Nodo_";

  //**** PIT ENTRY
  //std::string fnExpEntry = "RISULTATI/PIT/Entry_SCADUTE/EntrySCADUTE_Nodo_";

  //**** DOWNLOAD TIME
  //std::string fnDwnTime = "RISULTATI/DOWNLOAD/DownloadTime_Nodo_";
  
  //**** DOWNLOAD TIME FIRST
  std::string fnDwnTimeFirst = "RISULTATI/DOWNLOAD/FIRST/DownloadTime_Nodo_";


  //**** DOWNLOAD TIME FILE
  std::string fnDwnTimeFile = "RISULTATI/DOWNLOAD/FILE/DownloadTimeFile_Nodo_";
  
  //**** INTEREST INVIATI APP
  std::string fnInterestInviatiApp = "RISULTATI/INTEREST/Interest_INVIATI/APP/InterestINVIATI_APP_Nodo_";


  std::stringstream fname;
  AsciiTraceHelper asciiTraceHelper;
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {

	  // ************ NOMI FILE ******************

	  // **** INTEREST
	  fname << fnInterestInviati << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_interest_inviati = fname.str();
	  const char *filename_interest_inviati = nome_file_interest_inviati.c_str();
	  fname.str("");

	  fname << fnInterestRicevuti << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_interest_ricevuti = fname.str();
	  const char *filename_interest_ricevuti = nome_file_interest_ricevuti.c_str();
	  fname.str("");

	  //fname << fnInterestScartati << ((*node)->GetId()+1) << "." << extScenario;
	  //std::string nome_file_interest_scartati = fname.str();
	  //const char *filename_interest_scartati = nome_file_interest_scartati.c_str();
	  //fname.str("");

	  //fname << fnInterestScartatiFailure << ((*node)->GetId()+1) << "." << extScenario;
	  //std::string nome_file_interest_scartati_failure = fname.str();
	  //const char *filename_interest_scartati_failure = nome_file_interest_scartati_failure.c_str();
	  //fname.str("");


	  fname << fnInterestAggregati << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_interest_aggregati = fname.str();
	  const char *filename_interest_aggregati = nome_file_interest_aggregati.c_str();
	  fname.str("");

	  // **** DATA
	  fname << fnDataInviati << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_data_inviati = fname.str();
	  const char *filename_data_inviati = nome_file_data_inviati.c_str();
	  fname.str("");

	  fname << fnDataRicevuti << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_data_ricevuti = fname.str();
	  const char *filename_data_ricevuti = nome_file_data_ricevuti.c_str();
	  fname.str("");
	  
	  fname << fnDataInCacheApp << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_data_in_cache_app = fname.str();
	  const char *filename_data_in_cache_app = nome_file_data_in_cache_app.c_str();
	  fname.str("");


	  //fname << fnDataScartati << ((*node)->GetId()+1) << "." << extScenario;
	  //std::string nome_file_data_scartati = fname.str();
	  //const char *filename_data_scartati = nome_file_data_scartati.c_str();
	  //fname.str("");


	  // **** PIT

	  //fname << fnExpEntry << ((*node)->GetId()+1) << "." << extScenario;
	  //std::string nome_file_entry_exp = fname.str();
	  //const char *filename_entry_exp = nome_file_entry_exp.c_str();
	  //fname.str("");


	  // ******** STREAM DI OUTPUT ***************
	  Ptr<OutputStreamWrapper> streamInterestInviati = asciiTraceHelper.CreateFileStream(filename_interest_inviati);
	  Ptr<OutputStreamWrapper> streamInterestRicevuti = asciiTraceHelper.CreateFileStream(filename_interest_ricevuti);
	  //Ptr<OutputStreamWrapper> streamInterestScartati = asciiTraceHelper.CreateFileStream(filename_interest_scartati);
	  //Ptr<OutputStreamWrapper> streamInterestScartatiFailure = asciiTraceHelper.CreateFileStream(filename_interest_scartati_failure);
	  Ptr<OutputStreamWrapper> streamInterestAggregati = asciiTraceHelper.CreateFileStream(filename_interest_aggregati);
	  Ptr<OutputStreamWrapper> streamDataInviati = asciiTraceHelper.CreateFileStream(filename_data_inviati);
	  Ptr<OutputStreamWrapper> streamDataRicevuti = asciiTraceHelper.CreateFileStream(filename_data_ricevuti);
	  Ptr<OutputStreamWrapper> streamDataInCacheApp = asciiTraceHelper.CreateFileStream(filename_data_in_cache_app);
	  //Ptr<OutputStreamWrapper> streamDataScartati = asciiTraceHelper.CreateFileStream(filename_data_scartati);
	  //Ptr<OutputStreamWrapper> streamEntryExp = asciiTraceHelper.CreateFileStream(filename_entry_exp);


	   // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutInterests", MakeBoundCallback(&InterestInviatiRicevutiTrace, streamInterestInviati));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InInterests", MakeBoundCallback(&InterestInviatiRicevutiTrace, streamInterestRicevuti));
	  //(*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("DropInterests", MakeBoundCallback(&InterestInviatiRicevutiTrace, streamInterestScartati));
	  //(*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("DropInterestFailure", MakeBoundCallback(&UpdateD0D1InviatiTrace_Old, streamInterestScartatiFailure));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("AggregateInterests", MakeBoundCallback(&InterestInviatiRicevutiTrace, streamInterestAggregati));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutData", MakeBoundCallback(&DataInviatiTrace, streamDataInviati));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataRicevutiTrace, streamDataRicevuti));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("DataInCacheApp", MakeBoundCallback(&DataInCaheAppTrace, streamDataInCacheApp));
	  //(*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("DropData", MakeBoundCallback(&DataScartatiTrace, streamDataScartati));
	  //(*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("ExpEntry", MakeBoundCallback(&EntryExpTrace, streamEntryExp));

  }



  // ** [MT] ** Definition of CONSUMER nodes.

  NodeContainer consumerNodes;

  for(uint32_t i = 0; i < numCoreRouter; i++)
  {
     for(uint32_t j = 0; j < maxEdge; j++)
     {
    	 bool isRepo = false;                           // ** [MT] ** Repo nodes are excluded.
    	 ssEdge << edge << i << "_" << j;
    	 std::string cons_edge = ssEdge.str();
    	 ssEdge.str("");
         Ptr<Node> ce = Names::Find<Node>(cons_edge);
    	 if(ce != NULL)
    	 {
    		 // Check if the selected node is a Repo
    		 for(uint32_t z = 0; z < repositoriesID.size(); z++)
    		 {
    			 if (ce->GetId() == repositoriesID[z])
    				 isRepo = true;
    		 }
    		 if(!isRepo)
    			 consumerNodes.Add(ce);
    	 }
    	 else	        // There is not any other client attached to this router.
    		break;

     }
  }

  NS_LOG_UNCOND("The number of consumer is:\t" << consumerNodes.GetN());

  // Install Ndn applications
  NS_LOG_INFO ("Installing Applications");
  std::string prefix = "/prefix";

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");                       // ** Characterization of consumer Application
  consumerHelper.SetPrefix (prefix);
  consumerHelper.SetAttribute ("Frequency", StringValue ("1"));                  // ** Interest generation frequency.
  //consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));      // ** It is specified from the command line.
  consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.000000)));     // ** Clients start to request contents after 2 seconds.
  ApplicationContainer consumers = consumerHelper.Install (consumerNodes);


  // ** [MT] ** App-Level Tracing - (NBBB) Verificare se lo spostamento dei tracers in 'consumer-rtx' funziona.
  std::stringstream fnameCon;
  AsciiTraceHelper asciiTraceHelperCon;

  for (NodeContainer::Iterator node_app = consumerNodes.Begin(); node_app != consumerNodes.End(); ++node_app)
  {
	  fnameCon << fnInterestRtx << ((*node_app)->GetId()+1) << "." << extScenario;
      std::string nome_file_interest_rtx = fnameCon.str();
      const char *filename_interest_rtx = nome_file_interest_rtx.c_str();
      fnameCon.str("");
      Ptr<OutputStreamWrapper> streamInterestRitrasmessi = asciiTraceHelperCon.CreateFileStream(filename_interest_rtx);
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("TimeOutTrace",MakeBoundCallback(&EntryExpTrace, streamInterestRitrasmessi));

      fnameCon << fnInterestMaxRtx << ((*node_app)->GetId()+1) << "." << extScenario;
      std::string nome_file_interest_max_rtx = fnameCon.str();
      const char *filename_interest_max_rtx = nome_file_interest_max_rtx.c_str();
      fnameCon.str("");
      Ptr<OutputStreamWrapper> streamInterestEliminati = asciiTraceHelperCon.CreateFileStream(filename_interest_max_rtx);
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("NumMaxRtx",MakeBoundCallback(&InterestEliminatiTrace, streamInterestEliminati));

      //fnameCon << fnDwnTime << ((*node_app)->GetId()+1) << "." << extScenario;
      //std::string nome_file_download_time = fnameCon.str();
      //const char *filename_download_time = nome_file_download_time.c_str();
      //fnameCon.str("");
      //Ptr<OutputStreamWrapper> streamDownloadTime = asciiTraceHelperCon.CreateFileStream(filename_download_time);
      //(*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTimeAll",MakeBoundCallback(&DownloadTimeTrace, streamDownloadTime));

      fnameCon << fnDwnTimeFirst << ((*node_app)->GetId()+1) << "." << extScenario;
      std::string nome_file_download_time_first = fnameCon.str();
      const char *filename_download_time_first = nome_file_download_time_first.c_str();
      fnameCon.str("");
      Ptr<OutputStreamWrapper> streamDownloadTimeFirst = asciiTraceHelperCon.CreateFileStream(filename_download_time_first);
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTime",MakeBoundCallback(&DownloadTimeTrace, streamDownloadTimeFirst));

      fnameCon << fnDwnTimeFile << ((*node_app)->GetId()+1) << "." << extScenario;
      std::string nome_file_download_time_file = fnameCon.str();
      const char *filename_download_time_file = nome_file_download_time_file.c_str();
      fnameCon.str("");
      Ptr<OutputStreamWrapper> streamDownloadTimeFile = asciiTraceHelperCon.CreateFileStream(filename_download_time_file);
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTimeFile",MakeBoundCallback(&DownloadTimeFileTrace, streamDownloadTimeFile));

      fnameCon << fnInterestFileUncomplete << ((*node_app)->GetId()+1) << "." << extScenario;
      std::string nome_file_interest_file_uncomplete = fnameCon.str();
      const char *filename_interest_file_uncomplete = nome_file_interest_file_uncomplete.c_str();
      fnameCon.str("");
      Ptr<OutputStreamWrapper> streamInterestFileUncomplete = asciiTraceHelperCon.CreateFileStream(filename_interest_file_uncomplete);
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("UncompleteFile",MakeBoundCallback(&UncompleteFileTrace, streamInterestFileUncomplete));
      
      fnameCon << fnInterestInviatiApp << ((*node_app)->GetId()+1) << "." << extScenario;
      std::string nome_file_interest_inviati_app = fnameCon.str();
      const char *filename_interest_inviati_app = nome_file_interest_inviati_app.c_str();
      fnameCon.str("");
      Ptr<OutputStreamWrapper> streamInterestInviatiApp = asciiTraceHelperCon.CreateFileStream(filename_interest_inviati_app);
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("InterestApp",MakeBoundCallback(&InterestInviatiAppTrace, streamInterestInviatiApp));



  }

  //ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  //producerHelper.SetPrefix (prefix);
  //producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));

  // ** [MT] ** In best route scenario calculate the routes.
  if(simType == 2)
  {
      ndnGlobalRoutingHelper.CalculateRoutes ();
  }


  // ** [MT] ** LINK FAILURE IMPLEMENTATION


  Ptr<Node> nodeOrig, nodeDest;                        // ** Nodes at the extremities of the failed link
  Ptr<L3Protocol> ndnOrig, ndnDest;
  Ptr<NetDeviceFace> faceOrig, faceDest;
  Ptr<PointToPointNetDevice> ndOrig, ndDest;
  Ptr<GlobalRouter> grOrig, grDest;
  Ptr<BloomFilter> bfOrig, bfDest;
  uint32_t idFaceOrig, idFaceDest;

  switch(repAttach)                      // ** According to the scenario and the point of attachment of the repo(s), the failed link varies.
  {
  	  case(0):
		  nodeOrig = Names::Find<Node>("Core0");
  	  	  nodeDest = Names::Find<Node>("Core1");
  	  	  ndnOrig = nodeOrig->GetObject<L3Protocol>();
  	  	  ndnDest = nodeDest->GetObject<L3Protocol>();
  	  	  bfOrig = nodeOrig->GetObject<BloomFilter>();
  	  	  bfDest = nodeDest->GetObject<BloomFilter>();
  	  	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(0));
  	      if (faceOrig == 0)
  		  {
  		    NS_LOG_UNCOND ("Error in finding the interface of Core0!");
  		    exit(1);
  		  }
  	      faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
  	      if (faceDest == 0)
  		  {
  		    NS_LOG_UNCOND ("Error in finding the interface of Core1!");
  		    exit(1);
  		  }
  	      idFaceOrig = faceOrig->GetId();
  	      idFaceDest = faceDest->GetId();
  	      ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
  	      ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
   	      grOrig = nodeOrig->GetObject<GlobalRouter> ();
	      grDest = nodeDest->GetObject<GlobalRouter> ();

  	      Simulator::Schedule (Seconds (10000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
  	      Simulator::Schedule (Seconds (10000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
  	      Simulator::Schedule (Seconds (20000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
  	      Simulator::Schedule (Seconds (20000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);
  	      Simulator::Schedule (Seconds (30000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
  	      Simulator::Schedule (Seconds (30000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
   	      Simulator::Schedule (Seconds (40000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
  	      Simulator::Schedule (Seconds (40000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);


  	      if(simType == 3)    // ** In BF scenarios, the BFs of the failed link are cleared (i.e., all bits to 0) when the link fails,
  	    	  	  	  	  	  //    while they are set up (i.e., all the bits to 1) when the link is restored.
  	      {
  	  	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (20005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

  	  	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (40005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

  	      }

  	      if(simType == 2)   // ** In this case the best routes are recalculated both when the link fails and when it is restored.
  	      {
  	    	  Simulator::Schedule (Seconds (10020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (10020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (10020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	    	  Simulator::Schedule (Seconds (20020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (20020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (20020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	    	  Simulator::Schedule (Seconds (30020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (30020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (30020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);
  	    	  
   	    	  Simulator::Schedule (Seconds (40020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (40020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (40020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	      }




  	      break;
  	  case(11):
		  nodeOrig = Names::Find<Node>("Core11");
  	  	  nodeDest = Names::Find<Node>("Core10");
  	  	  ndnOrig = nodeOrig->GetObject<L3Protocol>();
  	  	  ndnDest = nodeDest->GetObject<L3Protocol>();
  	  	  bfOrig = nodeOrig->GetObject<BloomFilter>();
  	  	  bfDest = nodeDest->GetObject<BloomFilter>();
  	  	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(0));
  	      if (faceOrig == 0)
  		  {
  		    NS_LOG_UNCOND ("Error in finding the interface of Core11!");
  		    exit(1);
  		  }
  	      faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(1));
  	      if (faceDest == 0)
  		  {
  		    NS_LOG_UNCOND ("Error in finding the interface of Core10!");
  		    exit(1);
  		  }
  	      idFaceOrig = faceOrig->GetId();
  	      idFaceDest = faceDest->GetId();
  	      ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
  	      ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
   	      grOrig = nodeOrig->GetObject<GlobalRouter> ();
	      grDest = nodeDest->GetObject<GlobalRouter> ();

  	      Simulator::Schedule (Seconds (10000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
  	      Simulator::Schedule (Seconds (10000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
  	      Simulator::Schedule (Seconds (20000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
  	      Simulator::Schedule (Seconds (20000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);
  	      Simulator::Schedule (Seconds (30000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
  	      Simulator::Schedule (Seconds (30000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
   	      Simulator::Schedule (Seconds (40000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
  	      Simulator::Schedule (Seconds (40000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);


  	      if(simType == 3)
  	      {
  	  	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (20005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

  	  	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (40005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

  	      }

  	      if(simType == 2)
  	      {
  	    	  Simulator::Schedule (Seconds (10020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (10020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (10020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	    	  Simulator::Schedule (Seconds (20020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (20020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (20020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	    	  Simulator::Schedule (Seconds (30020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (30020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (30020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);
  	    	  
   	    	  Simulator::Schedule (Seconds (40020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (40020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (40020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	      }

  	      break;
  	  case(13):
		  nodeOrig = Names::Find<Node>("Core13");
  	  	  nodeDest = Names::Find<Node>("Core14");
  	  	  ndnOrig = nodeOrig->GetObject<L3Protocol>();
  	  	  ndnDest = nodeDest->GetObject<L3Protocol>();
  	  	  bfOrig = nodeOrig->GetObject<BloomFilter>();
  	  	  bfDest = nodeDest->GetObject<BloomFilter>();
  	  	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(1));
  	      if (faceOrig == 0)
  		  {
  		    NS_LOG_UNCOND ("Error in finding the interface of Core13!");
  		    exit(1);
  		  }
  	      faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
  	      if (faceDest == 0)
  		  {
  		    NS_LOG_UNCOND ("Error in finding the interface of Core14!");
  		    exit(1);
  		  }
  	      idFaceOrig = faceOrig->GetId();
  	      idFaceDest = faceDest->GetId();
  	      ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
  	      ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
   	      grOrig = nodeOrig->GetObject<GlobalRouter> ();
	      grDest = nodeDest->GetObject<GlobalRouter> ();

  	      Simulator::Schedule (Seconds (10000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
  	      Simulator::Schedule (Seconds (10000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
  	      Simulator::Schedule (Seconds (20000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
  	      Simulator::Schedule (Seconds (20000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);
  	      Simulator::Schedule (Seconds (30000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
  	      Simulator::Schedule (Seconds (30000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
   	      Simulator::Schedule (Seconds (40000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
  	      Simulator::Schedule (Seconds (40000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);


  	      if(simType == 3)
  	      {
  	  	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (20005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

  	  	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (40005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

  	      }

  	      if(simType == 2)
  	      {
  	    	  Simulator::Schedule (Seconds (10020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (10020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (10020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	    	  Simulator::Schedule (Seconds (20020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (20020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (20020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	    	  Simulator::Schedule (Seconds (30020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (30020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (30020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);
  	    	  
   	    	  Simulator::Schedule (Seconds (40020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
  	    	  Simulator::Schedule (Seconds (40020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
  	    	  Simulator::Schedule (Seconds (40020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

  	      }


  	  	  break;
  	  	  
  	  case(10):
  		  nodeOrig = Names::Find<Node>("Core10");
    	  nodeDest = Names::Find<Node>("Core9");
    	  ndnOrig = nodeOrig->GetObject<L3Protocol>();
    	  ndnDest = nodeDest->GetObject<L3Protocol>();
    	  bfOrig = nodeOrig->GetObject<BloomFilter>();
    	  bfDest = nodeDest->GetObject<BloomFilter>();
    	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(0));
    	  if (faceOrig == 0)
    	  {
    		 NS_LOG_UNCOND ("Error in finding the interface of Core10!");
    		 exit(1);
    	  }
    	  faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
    	  if (faceDest == 0)
    	  {
    		 NS_LOG_UNCOND ("Error in finding the interface of Core9!");
    		 exit(1);
    	  }
    	  idFaceOrig = faceOrig->GetId();
    	  idFaceDest = faceDest->GetId();
    	  ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
    	  ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
     	  grOrig = nodeOrig->GetObject<GlobalRouter> ();
  	      grDest = nodeDest->GetObject<GlobalRouter> ();

    	  Simulator::Schedule (Seconds (10000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
    	  Simulator::Schedule (Seconds (10000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
    	  Simulator::Schedule (Seconds (20000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
    	  Simulator::Schedule (Seconds (20000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);
    	  Simulator::Schedule (Seconds (30000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
    	  Simulator::Schedule (Seconds (30000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
     	  Simulator::Schedule (Seconds (40000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
    	  Simulator::Schedule (Seconds (40000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);


    	  if(simType == 3)
    	  {
    	     Simulator::Schedule (Seconds (10005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
    	     Simulator::Schedule (Seconds (10005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

    	     	 Simulator::Schedule (Seconds (20005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

    	  	 Simulator::Schedule (Seconds (30005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
    	  	 Simulator::Schedule (Seconds (30005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                 Simulator::Schedule (Seconds (40005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

    	   }

    	   if(simType == 2)
    	   {
    	  	  Simulator::Schedule (Seconds (10020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (10020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (10020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	   	  Simulator::Schedule (Seconds (20020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (20020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (20020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	   	  Simulator::Schedule (Seconds (30020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (30020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (30020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);
    	    	  
     	   	  Simulator::Schedule (Seconds (40020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (40020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (40020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	   }

   	      break;

  	  case(14):
  		  nodeOrig = Names::Find<Node>("Core14");
    	  nodeDest = Names::Find<Node>("Core15");
    	  ndnOrig = nodeOrig->GetObject<L3Protocol>();
    	  ndnDest = nodeDest->GetObject<L3Protocol>();
    	  bfOrig = nodeOrig->GetObject<BloomFilter>();
    	  bfDest = nodeDest->GetObject<BloomFilter>();
    	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(2));
    	  if (faceOrig == 0)
    	  {
             NS_LOG_UNCOND ("Error in finding the interface of Core14!");
    		 exit(1);
    	  }
    	  faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(2));
    	  if (faceDest == 0)
    	  {
    		 NS_LOG_UNCOND ("Error in finding the interface of Core15!");
    		 exit(1);
    	  }
    	  idFaceOrig = faceOrig->GetId();
    	  idFaceDest = faceDest->GetId();
    	  ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
    	  ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
     	  grOrig = nodeOrig->GetObject<GlobalRouter> ();
  	      grDest = nodeDest->GetObject<GlobalRouter> ();

    	  Simulator::Schedule (Seconds (10000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
    	  Simulator::Schedule (Seconds (10000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
    	  Simulator::Schedule (Seconds (20000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
    	  Simulator::Schedule (Seconds (20000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);
    	  Simulator::Schedule (Seconds (30000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
    	  Simulator::Schedule (Seconds (30000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
     	  Simulator::Schedule (Seconds (40000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
    	  Simulator::Schedule (Seconds (40000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);


    	  if(simType == 3)
    	  {
    	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
    	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                   Simulator::Schedule (Seconds (20005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

    	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
    	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                   Simulator::Schedule (Seconds (40005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

    	  }

    	  if(simType == 2)
    	  {
    	 	  Simulator::Schedule (Seconds (10020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
    	  	  Simulator::Schedule (Seconds (10020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (10020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	   	  Simulator::Schedule (Seconds (20020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (20020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (20020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	   	  Simulator::Schedule (Seconds (30020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (30020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (30020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);
    	    	  
     	   	  Simulator::Schedule (Seconds (40020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
    	   	  Simulator::Schedule (Seconds (40020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
    	   	  Simulator::Schedule (Seconds (40020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	  }

    	  break;
    	  
  	  case(15):
  		  nodeOrig = Names::Find<Node>("Core15");
    	  nodeDest = Names::Find<Node>("Core4");
    	  ndnOrig = nodeOrig->GetObject<L3Protocol>();
    	  ndnDest = nodeDest->GetObject<L3Protocol>();
    	  bfOrig = nodeOrig->GetObject<BloomFilter>();
    	  bfDest = nodeDest->GetObject<BloomFilter>();
    	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(3));
    	  if (faceOrig == 0)
    	  {
    		 NS_LOG_UNCOND ("Error in finding the interface of Core15!");
    		 exit(1);
    	  }
    	  faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
    	  if (faceDest == 0)
    	  {
    	     NS_LOG_UNCOND ("Error in finding the interface of Core4!");
    		 exit(1);
    	  }
    	  idFaceOrig = faceOrig->GetId();
    	  idFaceDest = faceDest->GetId();
    	  ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
    	  ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
     	  grOrig = nodeOrig->GetObject<GlobalRouter> ();
  	      grDest = nodeDest->GetObject<GlobalRouter> ();

    	  Simulator::Schedule (Seconds (10000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
    	  Simulator::Schedule (Seconds (10000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
    	  Simulator::Schedule (Seconds (20000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
    	  Simulator::Schedule (Seconds (20000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);
    	  Simulator::Schedule (Seconds (30000.00), &ns3::PointToPointNetDevice::SetLinkDown, ndOrig);
    	  Simulator::Schedule (Seconds (30000.01), &ns3::PointToPointNetDevice::SetLinkDown, ndDest);
     	  Simulator::Schedule (Seconds (40000.00), &ns3::PointToPointNetDevice::SetLinkUp, ndOrig);
    	  Simulator::Schedule (Seconds (40000.01), &ns3::PointToPointNetDevice::SetLinkUp, ndDest);


    	  if(simType == 3)
    	  {
    	     Simulator::Schedule (Seconds (10005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
    	     Simulator::Schedule (Seconds (10005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                  Simulator::Schedule (Seconds (20005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

    	     Simulator::Schedule (Seconds (30005.00), &ns3::ndn::BloomFilter::clear_stable, bfOrig, idFaceOrig);
    	     Simulator::Schedule (Seconds (30005.10), &ns3::ndn::BloomFilter::clear_stable, bfDest, idFaceDest);

                  Simulator::Schedule (Seconds (40005.00), &ns3::ndn::BloomFilter::set_stable, bfDest, idFaceDest);

    	  }

    	  if(simType == 2)
    	  {
    	     Simulator::Schedule (Seconds (10020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
    	     Simulator::Schedule (Seconds (10020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
    	     Simulator::Schedule (Seconds (10020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	     Simulator::Schedule (Seconds (20020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
    	     Simulator::Schedule (Seconds (20020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
    	     Simulator::Schedule (Seconds (20020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	     Simulator::Schedule (Seconds (30020.0), &ns3::ndn::GlobalRouter::RemoveIncidency, grOrig, faceOrig, grDest);
    	     Simulator::Schedule (Seconds (30020.1), &ns3::ndn::GlobalRouter::RemoveIncidency, grDest, faceDest, grOrig);
    	     Simulator::Schedule (Seconds (30020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);
    	    	  
     	     Simulator::Schedule (Seconds (40020.0), &ns3::ndn::GlobalRouter::AddIncidency, grOrig, faceOrig, grDest);
    	     Simulator::Schedule (Seconds (40020.1), &ns3::ndn::GlobalRouter::AddIncidency, grDest, faceDest, grOrig);
    	     Simulator::Schedule (Seconds (40020.5), &ns3::ndn::GlobalRoutingHelper::CalculateRoutes, & ndnGlobalRoutingHelper);

    	  }


    	  	  break;

  	  default:
  	  	  NS_LOG_UNCOND("Impossible to find the point of attachment of the Repo(s)!");
  	  	  exit(1);
  	  	  break;

  }


  Simulator::Stop (finishTime);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();

  //   ** [MT] ** Print the cache of each node at the end of the simulation.
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
     std::cout << "Node #" << ((*node)->GetId ()+1) << std::endl;
     (*node)->GetObject<ndn::ContentStore> ()->Print (std::cout);
     std::cout << std::endl;
  }

  Simulator::Destroy ();
  NS_LOG_INFO ("Done!");

  return 0;
}

// ** Function used to read the file containing the Adjacency matrix
std::vector<std::vector<bool> > readAdjMat (std::string adj_mat_file_name)
{
  std::ifstream adj_mat_file;
  adj_mat_file.open (adj_mat_file_name.c_str (), std::ios::in);
  if (adj_mat_file.fail ())
    {
      NS_FATAL_ERROR ("File " << adj_mat_file_name.c_str () << " not found");
    }
  std::vector<std::vector<bool> > array;
  int i = 0;
  int n_nodes = 0;

  while (!adj_mat_file.eof ())
    {
      std::string line;
      getline (adj_mat_file, line);
      if (line == "")
        {
          NS_LOG_WARN ("WARNING: Ignoring blank row in the array: " << i);
          break;
        }

      std::istringstream iss (line);
      bool element;
      std::vector<bool> row;
      int j = 0;

      while (iss >> element)
        {
          row.push_back (element);
          j++;
        }

      if (i == 0)
        {
          n_nodes = j;
        }

      if (j != n_nodes )
        {
          NS_LOG_ERROR ("ERROR: Number of elements in line " << i << ": " << j << " not equal to number of elements in line 0: " << n_nodes);
          NS_FATAL_ERROR ("ERROR: The number of rows is not equal to the number of columns! in the adjacency matrix");
        }
      else
        {
          array.push_back (row);
        }
      i++;
    }

  if (i != n_nodes)
    {
      NS_LOG_ERROR ("There are " << i << " rows and " << n_nodes << " columns.");
      NS_FATAL_ERROR ("ERROR: The number of rows is not equal to the number of columns! in the adjacency matrix");
    }

  adj_mat_file.close ();
  return array;

}

// ** Function used to read the file with the coordinates of each node
std::vector<std::vector<double> > readCordinatesFile (std::string node_coordinates_file_name)
{
  std::ifstream node_coordinates_file;
  node_coordinates_file.open (node_coordinates_file_name.c_str (), std::ios::in);
  if (node_coordinates_file.fail ())
    {
      NS_FATAL_ERROR ("File " << node_coordinates_file_name.c_str () << " not found");
    }
  std::vector<std::vector<double> > coord_array;
  int m = 0;

  while (!node_coordinates_file.eof ())
    {
      std::string line;
      getline (node_coordinates_file, line);

      if (line == "")
        {
          NS_LOG_WARN ("WARNING: Ignoring blank row: " << m);
          break;
        }

      std::istringstream iss (line);
      double coordinate;
      std::vector<double> row;
      int n = 0;
      while (iss >> coordinate)
        {
          row.push_back (coordinate);
          n++;
        }

      if (n != 2)
        {
          NS_LOG_ERROR ("ERROR: Number of elements at line#" << m << " is "  << n << " which is not equal to 2 for node coordinates file");
          exit (1);
        }

      else
        {
          coord_array.push_back (row);
        }
      m++;
    }
  node_coordinates_file.close ();
  return coord_array;

}

void
InterestInviatiRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header, Ptr<const Face> face)
{
	 *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << face->GetId() << std::endl;
	 //NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<  header->GetName() << face->GetId());
}

void DataInviatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, bool local, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << local << "\t" <<  face->GetId() << std::endl;
}

void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" <<  face->GetId() << std::endl;
}


void
EntryExpTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
}

void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *header << "\t" << time_sent << "\t" << time << "\t" << dist << std::endl;
}

void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *header << "\t" << time_sent << "\t" << time << "\t" << dist << std::endl;
}


//void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header)
//{
//	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
//}

void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *header << "\t" << time_sent << "\t" << std::endl;
}

void DataScartatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, uint32_t dropUnsol)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << dropUnsol << std::endl;
}

void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *header << "\t" << time_sent << std::endl;
}

void InterestInviatiAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* interest)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *interest << std::endl;
}

void DataInCaheAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
}

