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
#include "../src/ndnSIM/model/bloom-filter/ndn-bloom-filter-base.h"
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

NS_LOG_COMPONENT_DEFINE ("ndn.NewRouting");

std::vector<std::vector<bool> > readAdjMat (std::string adj_mat_file_name);
std::vector<std::vector<double> > readCordinatesFile (std::string node_coordinates_file_name);
void InterestScartatiFailureTrace(Ptr<OutputStreamWrapper> stream);
void InterestInviatiRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header, Ptr<const Face> face);
void DataInviatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, bool local, Ptr<const Face> face);
void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, Ptr<const Face> face);
void DataInCaheAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header);
void UpdateD0D1InviatiTrace(Ptr<OutputStreamWrapper> stream, uint32_t locOrForw, uint32_t numInterf);
void UpdateD0D1InviatiTrace_Old(Ptr<OutputStreamWrapper> stream);
void UpdateD0D1RicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Face> face);
void PercD0D1Trace(Ptr<OutputStreamWrapper> stream, const std::vector<double>* componenti);
void EntryExpTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header);

//void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist);
void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist);

void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent);

void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent);

void InterestInviatiAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* interest);

// ***  FUNZIONI PER IL NUOVO TRACING
void InterestTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header, Ptr<const Face> face, std::string eventType);
void DataTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, bool local, Ptr<const Face> face, std::string eventType);
void DataAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, std::string eventType);
void InterestAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, std::string eventType);
void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t downloadTime, uint32_t dist, std::string eventType);


int
main (int argc, char *argv[19])
{
  struct rlimit newlimit;
  const struct rlimit * newlimitP;
  newlimitP = NULL;
  newlimit.rlim_cur = 2000;
  newlimit.rlim_max = 2000;
  newlimitP = &newlimit;
  //pid_t pid = 0;
  //prlimit(pid,RLIMIT_NOFILE,newlimitP,&oldlimit);
  setrlimit(RLIMIT_NOFILE,newlimitP);

  static ns3::GlobalValue g_simRun ("g_simRun",
                                   "The number of the current simulation in the same scenario",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());

  static ns3::GlobalValue g_simType ("g_simType",
                                   "The choosen Forwarding Strategy",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());


  static ns3::GlobalValue g_alphaValue ("g_alphaValue",
                                   "Alpha - Zipf's Parameter",
                                   ns3::DoubleValue(1),
                                   ns3::MakeDoubleChecker<uint32_t> ());

  static ns3::GlobalValue g_scopeOfBf ("g_scopeOfBf",
                                   "The scope of the Bloom Filter",
                                   ns3::StringValue(""),
                                   ns3::MakeStringChecker());



  //uint32_t nGrid = 3;
  Time finishTime = Seconds (11500.0);


  // ** [MT] ** Parameters passed with command line
  uint64_t contentCatalogCache = 0;          // Estimated Content Catalog Cardinality to dimension the Bloom Filter of the Cache.
  uint64_t contentCatalogFib = 0;            // Estimated Content Catalog Cardinality to dimension the Bloom Filter for the FIB.
  double desiredPfpCache = 0.001;			 // Desired Probability of False Positives for the BF of the Cache.
  double desiredPfpFib = 0.001;			 	 // Desired Probability of False Positives for the BF for the FIB.
  uint32_t cellWidthBfCache = 1;             // Number of bits per cell of the BF of the Cache.
  uint32_t cellWidthBfFib = 1;               // Number of bits per cell of the BF for the FIB.
  std::string bfScope = "";					 // Desired BF scope (cache, fib, both).
  std::string bfTypeCache = "";				 // Desired BF type for the cache (simple or stable).
  std::string bfTypeFib = "";				 // Desired BF type for the fib (simple or stable).
  std::string bfCacheInitMethod = "";        // BF Cache initialization method (optimal or custom).
  std::string bfFibInitMethod = "";          // BF FIB initialization method (optimal or custom).
  uint64_t customLengthBfCache = 1000;       // Custom Length [bit] of the BF of the cache.
  uint64_t customLengthBfFib = 1000;       	 // Custom Length [bit] of the BF for the FIB.
  uint32_t customHashesBfCache = 1;       	 // Custom number of Hash Functions of the BF of the cache.
  uint32_t customHashesBfFib = 1;       	 // Custom number of Hash Functions of the BFs for the FIB.
  double cacheToCatalog = 0;            	 // The cacheSize/contentCatalogFib ratio.
  uint32_t simType = 1;                 	 // Simulation Scenario:
  	  	  	  	  	  	  	  	  	    	 // 1 = FLOODING  (Default)
  	  	  	  	  	  	  	  	  	  		 // 2 = BEST ROUTE
  	  	  	  	  	  	  	  	  	    	 // 3 = BF
  double alpha = 1;


  CommandLine cmd;
  cmd.AddValue ("contentCatalogCache", "Estimated Content Catalog Cardinality for the Bloom Filter of the Cache", contentCatalogCache);
  cmd.AddValue ("contentCatalogFib", "Estimated Content Catalog Cardinality for the Bloom Filter for the Fib", contentCatalogFib);
  cmd.AddValue ("desiredPfpCache", "Desired Probability of False Positives for the BF of the Cache", desiredPfpCache);
  cmd.AddValue ("desiredPfpFib", "Desired Probability of False Positives for the BF for the FIB", desiredPfpFib);
  cmd.AddValue ("cellWidthBfCache", "Number of bits per cell of the BF of the Cache", cellWidthBfCache);
  cmd.AddValue ("cellWidthBfFib", "Number of bits per cell of the BF for the FIB", cellWidthBfFib);
  cmd.AddValue ("bfScope", "Desired BF scope (cache, fib, both)", bfScope);
  cmd.AddValue ("bfTypeCache", "Desired BF type for the cache (simple or stable)", bfTypeCache);
  cmd.AddValue ("bfTypeFib", "Desired BF type for the FIB (simple or stable)", bfTypeFib);
  cmd.AddValue ("bfCacheInitMethod", "BF Cache initialization method (optimal or custom)", bfCacheInitMethod);
  cmd.AddValue ("bfFibInitMethod", "BF Fib initialization method (optimal or custom)", bfFibInitMethod);
  cmd.AddValue ("customLengthBfCache", "Custom Length [bit] of the BF of the cache", customLengthBfCache);
  cmd.AddValue ("customLengthBfFib", "Custom Length [bit] of the BF for the Fib", customLengthBfFib);
  cmd.AddValue ("customHashesBfCache", "Custom number of Hash Functions of the BF of the cache", customHashesBfCache);
  cmd.AddValue ("customHashesBfFib", "Custom number of Hash Functions of the BFs for the FIB", customHashesBfFib);
  cmd.AddValue ("cacheToCatalog", "Cache to Catalog Ratio", cacheToCatalog);
  cmd.AddValue ("simType", "Chosen Simulation Scenario", simType);
  cmd.AddValue ("valoreAlpha", "Zipf's Parameter", alpha);

  cmd.Parse (argc, argv);

  uint64_t simRun = SeedManager::GetRun();
  //NS_LOG_UNCOND("SIMULATION RUN:\t" << simRun);
  std::stringstream ss;
  std::string runNumber;
  ss << simRun;
  runNumber = ss.str();
  ss.str("");


  Config::SetGlobal ("g_simRun", UintegerValue(simRun));
  Config::SetGlobal ("g_simType", UintegerValue(simType));
  Config::SetGlobal ("g_alphaValue", DoubleValue(alpha));
  Config::SetGlobal ("g_scopeOfBf", StringValue(bfScope));

  // ** [MT] ** Calculate the cache size according to the specified ratio.
  uint32_t cacheSize = round((double)contentCatalogFib * cacheToCatalog);
  ss << cacheSize;
  std::string CACHE_SIZE = ss.str();
  ss.str("");


  std::string EXT = ".txt";
  std::string stringScenario;  		// Estensione per distinguere i vari scenari simulati


  std::string simulationType;

  switch (simType)
  {
  case(1):
	 simulationType = "Flooding";
  	 ss << simulationType << "_alpha_" << alpha << "_" << SeedManager::GetRun();
  	 stringScenario = ss.str();
  	 ss.str("");
     break;
  case(3):
	 simulationType = "BloomFilter";
	 ss << simulationType << "_alpha_" << alpha << "_" << "cell_" << cellWidthBfFib << "_run_" << SeedManager::GetRun();
	 stringScenario = ss.str();
     ss.str("");
     break;
  default:
	 NS_LOG_UNCOND ("Inserire una FORWARDING STRATEGY VALIDA!!");
	 exit(1);
	 break;
  }


  // ** [MT] ** Create topology by reading topology file
  AnnotatedTopologyReader topologyReader ("", 10);
  topologyReader.SetFileName ("/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/TOPOLOGIES/Geant_Topo_Only.txt");
  topologyReader.Read ();

  // ** [MT] ** Total number of nodes in the simulated topology
  uint32_t numNodes = topologyReader.GetNodes().GetN();
  NS_LOG_UNCOND("The total number of nodes is:\t" << numNodes);
  
  // ** [MT] ** Extract and dimension random repos

  uint32_t numRepo = 2;
  UniformVariable m_SeqRng (0, numNodes-1);                              // The maximum random number is equal to the number of nodes
  bool complete = false;

  std::vector<uint32_t>* repositoriesID = new std::vector<uint32_t>(numRepo) ;      // So far there are 2 Repos for each scenario
  for(uint32_t i=0; i<repositoriesID->size(); i++)
  {
	  repositoriesID->operator[](i) = numNodes+1;
  }

  uint32_t repo_num = 0;
  uint32_t rand_repo;

  while(!complete)
  {
	  bool already_extracted = false;
	  while(!already_extracted)
	  {

		  rand_repo = (uint32_t)m_SeqRng.GetValue();
		  for(uint32_t i=0; i<repositoriesID->size(); i++)
		  {
			  if(repositoriesID->operator[](i)==rand_repo)
			  {
				already_extracted = true;
				break;
			  }
		  }
		  if(already_extracted==true)
			already_extracted = false;        //per far continuare il ciclo
		  else
			already_extracted = true;
	  }
	  repositoriesID->operator[](repo_num) = rand_repo;
	  NS_LOG_UNCOND("CHOSEN REPOSITORY:\t" << repositoriesID->operator[](repo_num));
	  repo_num = repo_num + 1;
	  if(repo_num == repositoriesID->size())
		complete = true;
  }

  std::string REPO_SIZE;
  std::string PATH_REPO;
  uint32_t RepoSize;

  RepoSize = (uint32_t)((contentCatalogFib/2) + 1);
  ss << RepoSize;
  std::string repoSizeString = ss.str();
  ss.str("");
  //PATH_REPO= "/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/SEED_COPIES/NEW/repository2_";         // Two files (2_1 e 2_2) with equal number of contents.


  std::string SCOPE, INIT, CATALOG, PFP, WIDTH, CUSTOMLENGTH, CUSTOMHASHES;
  std::string INITCACHE, CATALOGCACHE, PFPCACHE, WIDTHCACHE, CUSTOMLENGTHCACHE, CUSTOMHASHESCACHE;

  ss << bfScope;
  SCOPE = ss.str();
  ss.str("");

  ss << bfFibInitMethod;
  INIT = ss.str();
  ss.str("");

  ss << bfCacheInitMethod;
  INITCACHE = ss.str();
  ss.str("");

  ss << contentCatalogFib;
  CATALOG = ss.str();
  ss.str("");

  ss << contentCatalogCache;
  CATALOGCACHE = ss.str();
  ss.str("");

  ss << desiredPfpFib;
  PFP = ss.str();
  ss.str("");

  ss << desiredPfpCache;
  PFPCACHE = ss.str();
  ss.str("");

  ss << cellWidthBfFib;
  WIDTH = ss.str();
  ss.str("");

  ss << cellWidthBfCache;
  WIDTHCACHE = ss.str();
  ss.str("");

  ss << customLengthBfFib;
  CUSTOMLENGTH = ss.str();
  ss.str("");

  ss << customLengthBfCache;
  CUSTOMLENGTHCACHE = ss.str();
  ss.str("");

  ss << customHashesBfFib;
  CUSTOMHASHES = ss.str();
  ss.str("");

  ss << customHashesBfCache;
  CUSTOMHASHESCACHE = ss.str();
  ss.str("");




  // ** [MT] ** INSTALL THE NDN STACK

  NS_LOG_INFO ("Installing Ndn stack on all nodes");
  ndn::StackHelper ndnHelper;

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


  NodeContainer producerNodes;
  NodeContainer consumerNodes;

  for(uint32_t i = 0; i < numRepo; i++)
  {
	  producerNodes.Add(NodeList::GetNode(repositoriesID->operator[](i)));
  }

  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
	  bool rep = false;
	  for (uint32_t j = 0; j < numRepo; j++)
	  {
		  if ((*node)->GetId() == repositoriesID->operator[](j))
			  rep = true;
	  }
	  if (rep)
	  {
		  // ** [MT] ** For Repo nodes: Cache Size = 0, that is they store only permanent copies.
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", "0");
		  ndnHelper.SetRepository("ns3::ndn::rp::Persistent", "MaxSize", repoSizeString);
	  }
	  else
	  {
		  // ** [MT] ** For clients or routers: Repo Size = 0, that is they do not store permanent copies.
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", CACHE_SIZE);
		  ndnHelper.SetRepository("ns3::ndn::rp::Persistent", "MaxSize", "0");
		  consumerNodes.Add((*node));
	  }

	  // *** Initialize Bloom Filters
      if (simType == 3)
      {
    	  if (bfScope.compare("fib") == 0)
    	  {
        	  if(bfTypeFib.compare("simple") == 0)
        	  {
        		  ndnHelper.SetBfFib("ns3::ndn::BloomFilterSimple", "Scope", SCOPE, "InitMethod", INIT,
        				  "ContentCatalog", CATALOG, "DesiredPfp", PFP,
        				  "CellWidth", WIDTH, "FilterCustomLength", CUSTOMLENGTH,
        				  "FilterCustomHashes", CUSTOMHASHES);
        	  }
        	  else if(bfTypeFib.compare("stable") == 0)
        	  {
        		  ndnHelper.SetBfFib("ns3::ndn::BloomFilterStable", "Scope", SCOPE, "InitMethod", INIT,
        				  "ContentCatalog", CATALOG, "DesiredPfp", PFP,
        				  "CellWidth", WIDTH, "FilterCustomLength", CUSTOMLENGTH,
        				  "FilterCustomHashes", CUSTOMHASHES);
        	  }
        	  else
        	  {
         		  NS_LOG_UNCOND("Incorrect type of Bloom Filter for the Fib. Please choose between 'simple' and 'stable'!\t");
    			  exit(1);
        	  }
     	  }
    	  else if (bfScope.compare("cache") == 0)
    	  {
        	  if(bfTypeCache.compare("simple") == 0)
        	  {
        		  ndnHelper.SetBfCache("ns3::ndn::BloomFilterSimple", "Scope", SCOPE, "InitMethod", INITCACHE,
        				  "ContentCatalog", CATALOGCACHE, "DesiredPfp", PFPCACHE,
        				  "CellWidth", WIDTHCACHE, "FilterCustomLength", CUSTOMLENGTHCACHE,
        				  "FilterCustomHashes", CUSTOMHASHESCACHE);
        	  }
        	  else if(bfTypeCache.compare("stable") == 0)
        	  {
        		  ndnHelper.SetBfCache("ns3::ndn::BloomFilterStable", "Scope", SCOPE, "InitMethod", INITCACHE,
        				  "ContentCatalog", CATALOGCACHE, "DesiredPfp", PFPCACHE,
        				  "CellWidth", WIDTHCACHE, "FilterCustomLength", CUSTOMLENGTHCACHE,
        				  "FilterCustomHashes", CUSTOMHASHESCACHE);
        	  }
        	  else
        	  {
         		  NS_LOG_UNCOND("Incorrect type of Bloom Filter for the Cache. Please choose between 'simple' and 'stable'!\t");
    			  exit(1);
        	  }
    	  }
    	  else if (bfScope.compare("both") == 0)
    	  {
        	  if(bfTypeFib.compare("simple") == 0)
        	  {
        		  ndnHelper.SetBfFib("ns3::ndn::BloomFilterSimple", "Scope", SCOPE, "InitMethod", INIT,
        				  "ContentCatalog", CATALOG, "DesiredPfp", PFP,
        				  "CellWidth", WIDTH, "FilterCustomLength", CUSTOMLENGTH,
        				  "FilterCustomHashes", CUSTOMHASHES);
        	  }
        	  else if(bfTypeFib.compare("stable") == 0)
        	  {
        		  ndnHelper.SetBfFib("ns3::ndn::BloomFilterStable", "Scope", SCOPE, "InitMethod", INIT,
        				  "ContentCatalog", CATALOG, "DesiredPfp", PFP,
        				  "CellWidth", WIDTH, "FilterCustomLength", CUSTOMLENGTH,
        				  "FilterCustomHashes", CUSTOMHASHES);
        	  }
        	  else
        	  {
         		  NS_LOG_UNCOND("Incorrect type of Bloom Filter for the Fib. Please choose between 'simple' and 'stable'!\t");
    			  exit(1);
        	  }
        	  if(bfTypeCache.compare("simple") == 0)
        	  {
        		  ndnHelper.SetBfCache("ns3::ndn::BloomFilterSimple", "Scope", SCOPE, "InitMethod", INITCACHE,
        				  "ContentCatalog", CATALOGCACHE, "DesiredPfp", PFPCACHE,
        				  "CellWidth", WIDTHCACHE, "FilterCustomLength", CUSTOMLENGTHCACHE,
        				  "FilterCustomHashes", CUSTOMHASHESCACHE);
        	  }
        	  else if(bfTypeCache.compare("stable") == 0)
        	  {
        		  ndnHelper.SetBfCache("ns3::ndn::BloomFilterStable", "Scope", SCOPE, "InitMethod", INITCACHE,
        				  "ContentCatalog", CATALOGCACHE, "DesiredPfp", PFPCACHE,
        				  "CellWidth", WIDTHCACHE, "FilterCustomLength", CUSTOMLENGTHCACHE,
        				  "FilterCustomHashes", CUSTOMHASHESCACHE);
        	  }
        	  else
        	  {
         		  NS_LOG_UNCOND("Incorrect type of Bloom Filter for the Cache. Please choose between 'simple' and 'stable'!\t");
    			  exit(1);
        	  }
    	  }
    	  else
    	  {
     		  NS_LOG_UNCOND("Incorrect scope for the Bloom Filter. Please choose between 'fib', 'cache', or 'both'!\t");
			  exit(1);
    	  }
      }

      ndnHelper.Install((*node));
  }


  if (simType == 3)
  {
	  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
	  {
		  if (bfScope.compare("fib") == 0)
		  {
			  (*node)->GetObject<ForwardingStrategy>()->GetBfFib()->MakeBfInitialization();
		  }
		  else if (bfScope.compare("cache") == 0)
		  {
			  (*node)->GetObject<ForwardingStrategy>()->GetBfCache()->MakeBfInitialization();
		  }
		  else
		  {
			  (*node)->GetObject<ForwardingStrategy>()->GetBfFib()->MakeBfInitialization();
			  (*node)->GetObject<ForwardingStrategy>()->GetBfCache()->MakeBfInitialization();
		  }
	  }
  }

  NS_LOG_UNCOND("The number of consumer is:\t" << consumerNodes.GetN());




  // ** [MT] ** Initialize Repos with SEED copies. Content names are read from files.

  std::string line;
  Ptr<Node> nd;
  const char *PATH_COMPL;
  std::ifstream fin;

  //PATH_REPO= "/media/DATI/tortelli/COPIE_SEED/repository2_";         // Ci sono due file (2_1 e 2_2) con ugual numero di contenuti
  PATH_REPO= "/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/SEED_COPIES/NEW/repository2_";         // Ci sono due file (2_1 e 2_2) con ugual numero di contenuti

  // I repository sono più di uno
  for(uint32_t i = 0; i < repositoriesID->size(); i++)
  {
     nd = NodeList::GetNode(repositoriesID->operator[](i));
     ss << PATH_REPO << (i+1) << EXT;
     PATH_COMPL = ss.str().c_str();
     ss.str("");

     fin.open(PATH_COMPL);
  	 if(!fin) {
  	   	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n" << PATH_COMPL << "\t" << repositoriesID->size() << "\n";
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
  	    Ptr<Packet> packet = Create<Packet> (512);
  	    packet->AddHeader (*header);
  	    packet->AddTrailer (tail);

  	    nd->GetObject<Repo> ()->Add (header, packet);
  	 }
  	 fin.close();
  }

//nd->Unref();
//nd = 0;

  // ** [MT] ** Install Ndn applications
  NS_LOG_INFO ("Installing Applications");
  std::string prefix = "/prefix";

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");                    // ***** CARATTERIZZAZIONE DELL'APPLICAZIONE CONSUMER
  consumerHelper.SetPrefix (prefix);
  consumerHelper.SetAttribute ("Frequency", StringValue ("1"));               // ***** FREQUENZA DI GENERAZIONE (/s) DEGLI INTEREST
  //consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.000000)));
  ApplicationContainer consumers = consumerHelper.Install (consumerNodes);
  consumers.Start (Seconds(2));
  //consumers.Stop(finishTime-Seconds(1));


  // ** Settaggio della tipologia di nodo.

  for (NodeContainer::Iterator node_app = consumerNodes.Begin(); node_app != consumerNodes.End(); ++node_app)
  {
	  (*node_app)->GetObject<ForwardingStrategy>()->SetNodeType("client");
  }

  for (NodeContainer::Iterator node_prod = producerNodes.Begin(); node_prod != producerNodes.End(); ++node_prod)
  {
	  (*node_prod)->GetObject<ForwardingStrategy>()->SetNodeType("producer");
  }


  // ** [MT] ** NUOVO SISTEMA DI TRACING ***********************************************************************

  //**** INTEREST (Inviati, Ricevuti, Aggregati)
  std::string fnInterest;

  //**** DATA (Inviati e Ricevuti)
  std::string fnData;

  //**** DATA RICEVUTI APP (Tracing a livello Applicativo - Data Ricevuti, compresi quelli provenienti dalla cache locale)
  std::string fnDataApp;

  //**** INTEREST APP (Tracing a livello Applicativo - Interest Ritrasmessi, Eliminati (Max Rtx), Eliminati File)
  std::string fnInterestApp;

  //**** DOWNLOAD TIME (Tracing a livello Applicativo - Download Time, sia First Chunk che Complete File)
  std::string fnDwnTime;

  
  switch (simType)
  {
  case(1):
  case(2):

  	  ss << "RISULTATI_NEW/" <<  simulationType << "/INTEREST/Interest_Nodo_";
      fnInterest = ss.str();
      ss.str("");

  	  ss << "RISULTATI_NEW/" <<  simulationType << "/DATA/Data_Nodo_";
      fnData = ss.str();
      ss.str("");

  	  ss << "RISULTATI_NEW/" <<  simulationType << "/DATA/APP/DataRICEVUTI_APP_Nodo_";
      fnDataApp = ss.str();
      ss.str("");

      ss << "RISULTATI_NEW/" <<  simulationType << "/INTEREST/APP/Interest_APP_Nodo_";
      fnInterestApp = ss.str();
      ss.str("");

      ss << "RISULTATI_NEW/" <<  simulationType << "/DOWNLOAD/APP/DownloadTime_Nodo_";
      fnDwnTime = ss.str();
      ss.str("");

      break;

  case(3):

  	  ss << "RISULTATI_NEW/" <<  simulationType << "/" << bfScope << "/" << bfTypeFib << "/INTEREST/Interest_Nodo_";
      fnInterest = ss.str();
      ss.str("");

   	  ss << "RISULTATI_NEW/" <<  simulationType << "/" << bfScope << "/" << bfTypeFib << "/DATA/Data_Nodo_";
      fnData = ss.str();
      ss.str("");

   	  ss << "RISULTATI_NEW/" <<  simulationType << "/" << bfScope << "/" << bfTypeFib << "/DATA/APP/DataRICEVUTI_APP_Nodo_";
      fnDataApp = ss.str();
      ss.str("");

      ss << "RISULTATI_NEW/" <<  simulationType << "/" << bfScope << "/" << bfTypeFib << "/INTEREST/APP/Interest_APP_Nodo_";
      fnInterestApp = ss.str();
      ss.str("");

      ss << "RISULTATI_NEW/" <<  simulationType << "/" << bfScope << "/" << bfTypeFib << "/DOWNLOAD/APP/DownloadTime_Nodo_";
      fnDwnTime = ss.str();
      ss.str("");

      break;
      
  default:
	 NS_LOG_UNCOND ("Inserire una FORWARDING STRATEGY VALIDA!!");
	 exit(1);
	 break;
  }

  //NS_LOG_UNCOND(fnInterest << "\n" << fnData << "\n" << fnDataApp << "\n" << fnInterestApp << "\n" << fnDwnTime << "\n");


  std::stringstream fname;
  AsciiTraceHelper asciiTraceHelper;
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
	  // ************ NOMI FILE ******************

	  // **** INTEREST
	  fname << fnInterest << ((*node)->GetId()+1) << "." << stringScenario;
	  std::string nome_file_interest = fname.str();
	  const char *filename_interest = nome_file_interest.c_str();
	  fname.str("");

	  // **** DATA
	  fname << fnData << ((*node)->GetId()+1) << "." << stringScenario;
	  std::string nome_file_data = fname.str();
	  const char *filename_data = nome_file_data.c_str();
	  fname.str("");

	  // ******** STREAM DI OUTPUT ***************
	  Ptr<OutputStreamWrapper> streamInterest = asciiTraceHelper.CreateFileStream(filename_interest);
	  Ptr<OutputStreamWrapper> streamData = asciiTraceHelper.CreateFileStream(filename_data);

      // ** Scrittura del tipo di nodo all'inizio di ciascun file.
 	  *streamData->GetStream() << (*node)->GetObject<ForwardingStrategy>()->GetNodeType() << std::endl;
 	  *streamInterest->GetStream() << (*node)->GetObject<ForwardingStrategy>()->GetNodeType() << std::endl;

	   // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutInterests", MakeBoundCallback(&InterestTrace, streamInterest));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InInterests", MakeBoundCallback(&InterestTrace, streamInterest));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("AggregateInterests", MakeBoundCallback(&InterestTrace, streamInterest));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutData", MakeBoundCallback(&DataTrace, streamData));
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataTrace, streamData));
  }


  // ** [MT] ** App-Level Tracing -

  for (NodeContainer::Iterator node_app = consumerNodes.Begin(); node_app != consumerNodes.End(); ++node_app)
  {
	  // **** DATA APP
	  fname << fnDataApp << ((*node_app)->GetId()+1) << "." << stringScenario;
	  std::string nome_file_data_app = fname.str();
	  const char *filename_data_app = nome_file_data_app.c_str();
	  fname.str("");

	  // **** INTEREST APP
	  fname << fnInterestApp << ((*node_app)->GetId()+1) << "." << stringScenario;
      std::string nome_file_interest_app = fname.str();
      const char *filename_interest_app = nome_file_interest_app.c_str();
      fname.str("");

      // **** DOWNLOAD TIME
      fname << fnDwnTime << ((*node_app)->GetId()+1) << "." << stringScenario;
      std::string nome_file_download_time = fname.str();
      const char *filename_download_time = nome_file_download_time.c_str();
      fname.str("");

	  // ******** STREAM DI OUTPUT APP ***************
      Ptr<OutputStreamWrapper> streamDataApp = asciiTraceHelper.CreateFileStream(filename_data_app);
	  Ptr<OutputStreamWrapper> streamInterestApp = asciiTraceHelper.CreateFileStream(filename_interest_app);
      Ptr<OutputStreamWrapper> streamDownloadTime = asciiTraceHelper.CreateFileStream(filename_download_time);

      // ** Scrittura del tipo di nodo all'inizio di ciascun file.
 	  *streamDataApp->GetStream() << (*node_app)->GetObject<ForwardingStrategy>()->GetNodeType() << std::endl;
 	  *streamInterestApp->GetStream() << (*node_app)->GetObject<ForwardingStrategy>()->GetNodeType() << std::endl;
 	  *streamDownloadTime->GetStream() << (*node_app)->GetObject<ForwardingStrategy>()->GetNodeType() << std::endl;

      // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
      (*node_app)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("DataInCacheApp", MakeBoundCallback(&DataAppTrace, streamDataApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("InterestApp",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("TimeOutTrace",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("NumMaxRtx",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("UncompleteFile",MakeBoundCallback(&InterestAppTrace, streamInterestApp));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTime",MakeBoundCallback(&DownloadTimeTrace, streamDownloadTime));
      (*node_app)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("DownloadTimeFile",MakeBoundCallback(&DownloadTimeTrace, streamDownloadTime));
  }

  // **************************************************************************************************************************


  delete repositoriesID;



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

// ******* FUNZIONE PER LEGGERE IL FILE CHE DESCRIVE LA MATRICE DELLE ADIACENZE
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

// ******* FUNZIONE PER LEGGERE IL FILE CHE DESCRIVE LE COORDINATE DEI NODI
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
	 *stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  header->GetName() << "\t" << face->GetId() << std::endl;
	 //NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<  header->GetName() << face->GetId());
}

void DataInviatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, bool local, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  header->GetName() << "\t" << local << "\t" <<  face->GetId() << std::endl;
}

void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  header->GetName() << "\t" <<  face->GetId() << std::endl;
}


void
EntryExpTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  header->GetName() << std::endl;
}

/*void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  *header << "\t" << time_sent << "\t" << time << "\t" << dist << std::endl;
}*/

void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  *header << "\t" << time_sent << "\t" << time << "\t" << dist << std::endl;
}


//void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header)
//{
//	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
//}

void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  *header << "\t" << time_sent << "\t" << std::endl;
}

void DataScartatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, uint32_t dropUnsol)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  header->GetName() << "\t" << dropUnsol << std::endl;
}

void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  *header << "\t" << time_sent << std::endl;
}

void InterestInviatiAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* interest)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  *interest << std::endl;
}

void DataInCaheAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  header->GetName() << std::endl;
}


// *************************+

void InterestTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Interest> header, Ptr<const Face> face, std::string eventType)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << "--\t" << face->GetId() << std::endl;
}

void DataTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, bool local, Ptr<const Face> face, std::string eventType)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << "--\t" << local << "\t" <<  face->GetId() << std::endl;
}

void DataAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObject> header, std::string eventType)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << "--\t" << std::endl;
}

void InterestAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, std::string eventType)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << "--\t" << time_sent << std::endl;
}

void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t downloadTime, uint32_t dist, std::string eventType)
{
	*stream->GetStream() << Simulator::Now().GetMicroSeconds() << "\t" <<  eventType << "\t" << "--\t" << time_sent << "\t" << downloadTime << "\t" << dist << std::endl;
}

