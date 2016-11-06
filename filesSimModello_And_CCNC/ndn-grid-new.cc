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
#include "../src/ndnSIM/model/Bloom_Filter/bloom_filter.h"
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
std::vector<double>*  Crea_CumSum (uint32_t m_N, double m_q, double m_s);

void PrintCacheTrace(Ptr<OutputStreamWrapper> stream, uint32_t node_id);
void PrintSentId(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header);

//void InterestScartatiFailureTrace(Ptr<OutputStreamWrapper> stream);
//void InterestInviatiRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header, Ptr<const Face> face);
//void DataInviatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, bool local, Ptr<const Face> face);
void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, Ptr<const Face> face);
//void DataInCaheAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header);
//void UpdateD0D1InviatiTrace(Ptr<OutputStreamWrapper> stream, uint32_t locOrForw, uint32_t numInterf);
//void UpdateD0D1InviatiTrace_Old(Ptr<OutputStreamWrapper> stream);
//void UpdateD0D1RicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Face> face);
//void PercD0D1Trace(Ptr<OutputStreamWrapper> stream, const std::vector<double>* componenti);
//void EntryExpTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header);
//
////void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, int64_t time, uint32_t dist);
////void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, int64_t time);
//void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist);
//void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent, int64_t time, uint32_t dist);
//
////void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header);
//void UncompleteFileTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent);
//
////void DataScartatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, uint32_t dropUnsol);
//
//void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent);
//
//void InterestInviatiAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* interest);

/**
 * This scenario simulates a grid topology (using PointToPointGrid module)
 *
 * (consumer) -- ( ) ----- ( )
 *     |          |         |
 *    ( ) ------ ( ) ----- ( )
 *     |          |         |
 *    ( ) ------ ( ) -- (producer)
 *
 * Grid size could be specified using --nGrid parameter (default 3)
 *
 * All links are 1Mbps with propagation 10ms delay.
 *
 * FIB is populated using NdnGlobalRoutingHelper.
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * Simulation time is 20 seconds, unless --finish parameter is specified
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=NdnSimple:NdnConsumer ./waf --run=ndn-grid
 */


int
main (int argc, char *argv[7])
{
  struct rlimit newlimit;
  const struct rlimit * newlimitP;
  newlimitP = NULL;
  newlimit.rlim_cur = 5000;
  newlimit.rlim_max = 5000;
  newlimitP = &newlimit;
  //pid_t pid = 0;
  //prlimit(pid,RLIMIT_NOFILE,newlimitP,&oldlimit);
  setrlimit(RLIMIT_NOFILE,newlimitP);

  static ns3::GlobalValue g_numeroSIM ("g_numeroSIM",
                                   "The number of the current simulation in the same scenario",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());

  static ns3::GlobalValue g_simType ("g_simType",
                                   "The choosen Forwarding Strategy",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());
                                   
  //static ns3::GlobalValue cum_sum ("g_cumsum",
	//	  	  	  	  	  	  	   "Zipf Cumulative Distribution Function",
	//	  	  	  	  	  	  	   ns3::PointerValue(NULL),
	//	  	  	  	  	  	  	   ns3::MakePointerChecker<std::vector<double> >());
  
  //uint32_t nGrid = 3;
  Time finishTime = Seconds (5000.0);
  uint32_t contentCatalog = 0;          // Numero totale di contenuti
  double cacheToCatalog = 0;            // Rapporto tra cache size e content catalog
  uint32_t simType = 1;                 // Parametro passato da riga di comando per definire il tipo di simulazione.
  	  	  	  	  	  	  	  	  	    // 1 = FLOODING  (Default)
  	  	  	  	  	  	  	  	  	  	// 2 = BEST ROUTE
  	  	  	  	  	  	  	  	  	    // 3 = BF
  	  	  	  	  	  	  	  	  	    
  double req_freq = 0;

  uint32_t numeroSIM = 1;              // Per ogni scenario verranno effettuati 3 run.             

  CommandLine cmd;
  //cmd.AddValue ("nGrid", "Number of grid nodes", nGrid);
  //cmd.AddValue ("finish", "Finish time", finishTime);
  cmd.AddValue ("contentCatalog", "Total number of contents", contentCatalog);
  cmd.AddValue ("cacheToCatalog", "Cache to Catalog Ratio", cacheToCatalog);
  cmd.AddValue ("simType", "Chosen Forwarding Strategy", simType);
  cmd.AddValue ("numeroSIM", "Current Simulation Run", numeroSIM);
  cmd.AddValue ("reqFreq", "Client request frequency", req_freq);

  cmd.Parse (argc, argv);

  Config::SetGlobal ("g_numeroSIM", UintegerValue(numeroSIM));
  Config::SetGlobal ("g_simType", UintegerValue(simType));
  
  //static std::vector<double>* cs_p = Crea_CumSum(contentCatalog, 0, 1.2);    // Alpha = 1.2
  static std::vector<double>* cs_p = Crea_CumSum(contentCatalog, 0, 1);    // Alpha = 1
  //Config::SetGlobal ("g_cumsum", PointerValue(cs_p));
  
 
  
  std::string EXT = ".txt";
  std::stringstream scenario;
  std::string extScenario;  		// Estensione per distinguere i vari scenari simulati

  // *** Ad ogni RUN, la collocazione della edge network che farà da repository sarà casuale. Quindi devo estrarre un numero casuale da 0 a 22 (numero di Core router)
  // *** Inoltre, anche il numero di repository all'interno di quella rete varierà casualmente, da 1 a 3 (numero minimo di nodi in una edge network)
  // *** distribuendosi rispettivamente i contenuti
  Ptr<UniformRandomVariable> myRand = CreateObject<UniformRandomVariable> ();
  
//NS_LOG_UNCOND("Numero Casuale estratto 1:\t" << myRand->GetValue() << "\n");
//  uint32_t repAttach = (uint32_t)( (int)( myRand->GetValue()*pow(10,5) ) % 22);
  
//NS_LOG_UNCOND("Numero Casuale estratto 2:\t" << myRand->GetValue() << "\n"); 
//  uint32_t numRepo = (uint32_t) ( ( (int)( myRand->GetValue()*pow(10,5) ) %3)+1);
  
  //uint32_t repAttach = (uint32_t)((22 * rand()) / (RAND_MAX + 1.0));
  //uint32_t numRepo = (uint32_t)((3 * rand()) / (RAND_MAX + 1.0));

  std::string simulationType;


  switch (simType)
  {
  case(1):
	 simulationType = "Flooding";
  	 scenario << simulationType << "_" << numeroSIM << "_" << cacheToCatalog << "_" << req_freq << "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
  	 extScenario = scenario.str();
  	 scenario.str("");
     break;
  case(2):
	 simulationType = "BestRoute";
  	 scenario << simulationType << "_"  << numeroSIM << "_" << cacheToCatalog << "_" << req_freq << "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
     extScenario = scenario.str();
     scenario.str("");
     break;
  default:
	 NS_LOG_UNCOND ("Inserire una FORWARDING STRATEGY VALIDA!!");
	 exit(1);
	 break;
  }

  // Estrazione del RUN utilizzato per il generatore di numeri casuali.
  uint64_t run = SeedManager::GetRun();
  std::stringstream ssInit;
  std::string runNumber;
  ssInit << run;
  runNumber = ssInit.str();
  ssInit.str("");

// *******  CREAZIONE DELLA TOPOLOGIA TRAMITE LETTURA FILE ********
//  AnnotatedTopologyReader topologyReader ("", 10);
//  topologyReader.SetFileName ("/home/tortelli/Simulazioni_Finali/ndnSIM_Failure_Copy_ExactMatch_1/Geant_Topo.txt");
//  topologyReader.Read ();

//  // ****************  CREAZIONE TOPOLOGIA RANDOM  ***************************
//
NS_LOG_UNCOND("*****  CREAZIONE TOPOLOGIA RANDOM  *****"); 

  std::string adj_mat_file_name ("/media/DATI/tortelli/Simulazioni_Modello/ndnSIM_Modello/Adj_Matrix_prova_flooding_triu.txt");
//  std::string coord_file_name ("/home/michele/ndn-SIM/node_coord.txt");
//
  std::vector<std::vector<bool> > Adj_Matrix;
  Adj_Matrix = readAdjMat (adj_mat_file_name);
//
//  std::vector<std::vector<double> > coord_array;
//  coord_array = readCordinatesFile (coord_file_name);
//
//  int n_nodes = coord_array.size();
  uint32_t n_nodes = (uint32_t)Adj_Matrix.size();
//
//  if (matrixDimension != n_nodes)
//    {
//      NS_FATAL_ERROR ("The number of lines in coordinates file (" << n_nodes << ") is not equal to the number of nodes in adjacency matrix size " << matrixDimension);
//    }
//
  NS_LOG_INFO ("Create Nodes.");
//
  NodeContainer nodes;   // Declare nodes objects
  nodes.Create (n_nodes);
//
  NS_LOG_INFO ("Create Links Between Nodes.");
//
  PointToPointHelper p2p;
  uint32_t linkCount = 0;
//
  for (size_t i = 0; i < Adj_Matrix.size (); i++)
    {
      for (size_t j = 0; j < Adj_Matrix[i].size (); j++)
        {
           if (Adj_Matrix[i][j] == 1)
            {
              NodeContainer n_links = NodeContainer (nodes.Get (i), nodes.Get (j));
              p2p.SetDeviceAttribute ("DataRate", StringValue ("50Mbps"));
              p2p.SetChannelAttribute ("Delay", StringValue ("0ms"));
              NetDeviceContainer n_devs = p2p.Install (n_links);
//              //ipv4_n.Assign (n_devs);
//              //ipv4_n.NewNetwork ();
              linkCount++;
//              //NS_LOG_INFO ("matrix element [" << i << "][" << j << "] is 1");
            }
          else
            {
              //NS_LOG_INFO ("matrix element [" << i << "][" << j << "] is 0");
            }
        }
    }
  NS_LOG_INFO ("Number of links in the adjacency matrix is: " << linkCount);
  NS_LOG_UNCOND ("Number of all nodes is: " << nodes.GetN ());

  NS_LOG_INFO ("Allocate Positions to Nodes.");
  
  NS_LOG_UNCOND("*****  FINE CREAZIONE TOPOLOGIA RANDOM  *****");

//  MobilityHelper mobility_n;
//  Ptr<ListPositionAllocator> positionAlloc_n = CreateObject<ListPositionAllocator> ();
//
//  for (size_t m = 0; m < coord_array.size (); m++)
//    {
//      positionAlloc_n->Add (Vector (coord_array[m][0], coord_array[m][1], 0));
//      Ptr<Node> n0 = nodes.Get (m);
//      Ptr<ConstantPositionMobilityModel> nLoc =  n0->GetObject<ConstantPositionMobilityModel> ();
//      if (nLoc == 0)
//        {
//          nLoc = CreateObject<ConstantPositionMobilityModel> ();
//          n0->AggregateObject (nLoc);
//        }
//      // y-coordinates are negated for correct display in NetAnim
//      // NetAnim's (0,0) reference coordinates are located on upper left corner
//      // by negating the y coordinates, we declare the reference (0,0) coordinate
//      // to the bottom left corner
//      Vector nVec (coord_array[m][0], -coord_array[m][1], 0);
//      nLoc->SetPosition (nVec);
//    }
//  mobility_n.SetPositionAllocator (positionAlloc_n);
//  mobility_n.Install (nodes);
//
//
//  //  ****************  FINE CREAZIONE TOPOLOGIA RANDOM  ********************************

//  uint32_t numeroNodi = topologyReader.GetNodes().GetN();
//  NS_LOG_UNCOND("Il numero di nodi è:\t" << numeroNodi);

// **** CREAZIONE DELL'ARRAY CONTENENTE GLI ID DEI CONTENUTI

  //std::vector<uint32_t> id_vector;
  //id_vector.resize(contentCatalog);
  //for(uint32_t i = 0; i<contentCatalog; i++)
  //{
	 //id_vector[i] = (i+1);
  //}

  // **** CALCOLO DELLA DIMENSIONE DELLA CACHE E DELLA PERCENTUALE PER LE COPIE SEED

  uint32_t CacheSizeTot = round((double)contentCatalog * cacheToCatalog);
  uint32_t RepoSize = round((double)contentCatalog / n_nodes);
  uint32_t CacheSize = CacheSizeTot - RepoSize;                               // Alla dimensione totale della cache, tolgo la porzione destinata alle copie seed

  ssInit << CacheSize;
  std::string CACHE_SIZE = ssInit.str();
  ssInit.str("");

  ssInit << RepoSize;
  std::string REPO_SIZE = ssInit.str();
  ssInit.str("");

  // *** VERIFICA PARAMETRI IMMESSI E CALCOLATI ****
  NS_LOG_UNCOND("CONTENT CATALOG:\t" << contentCatalog << "\n" << "CACHE/CATALOG:\t" << cacheToCatalog << "\n" << "SIM TYPE:\t" << simType << "\n"
    << "RNG:\t" << runNumber << "\n" << "CACHE SIZE:\t" << CacheSize << "\n" << "REPO SIZE:\t" << REPO_SIZE << "\n" << "NUMERO SIM:\t" << numeroSIM << "\n");




  // *****************  INSTALLAZIONE DELLO STACK NDN A SECONDA DELLA TIPOLOGIA DI NODO ********************

  NS_LOG_INFO ("Installing Ndn stack on all nodes");
  ndn::StackHelper ndnHelper;
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;                           // ****** ROUTING HELPER

  // To FLOOD Interest we need to set the Forwarding strategy as well as the default routes for every node
if(simType == 1)
{
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::Flooding");                       // *** FORWARDING STRATEGY
  ndnHelper.SetDefaultRoutes(true);
}
else
{
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::Flooding");                       // *** FORWARDING STRATEGY
  //ndnHelper.SetDefaultRoutes(true);
}

// Settaggio del ContentStore e del Repository
  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", CACHE_SIZE);
  ndnHelper.SetRepository("ns3::ndn::repo::Persistent", "MaxSize", REPO_SIZE);

// Installazione dello StackHelper su tutti i nodi
  ndnHelper.InstallAll();


  if(simType == 2)                                // Se è lo scenario BestRoute, ho bisogno di installare il Routing Helper
  {
	ndnGlobalRoutingHelper.InstallAll ();
  }


  /*****************************************************************************
   *  Inizializzazione dei repository dei vari nodi con copie seed random, sempre rispettando il vincolo di una sola copia SEED per contenuto.
   *  Viene distribuito un ugual numero di copie seed tra i vari nodi.
   */

NS_LOG_UNCOND("*****  INIZIALIZZAZIONE REPOSITORY  *****");

  std::stringstream fname_repo;
  std::string contenuto;
  std::string line;
  
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {

	  // ************ NOMI FILE ******************

	  // **** INTEREST
	  fname_repo << "Copie_SEED/Seed_500_nodi/seed_" << ((*node)->GetId()+1) << "_" << runNumber << "." << "txt";
	  std::string nome_file_repo = fname_repo.str();
	  const char *filename_repo = nome_file_repo.c_str();
	  fname_repo.str("");
	  std::ifstream fin_repo;
	  
	  fin_repo.open(filename_repo);
	  if(!fin_repo) {
	    std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n";
	    exit(0);
	  }

	  
	  while(std::getline(fin_repo, line))
	  {
		  fname_repo << "/" << line;
		  contenuto = fname_repo.str();
          fname_repo.str("");

		  static ContentObjectTail tail;
  	      Ptr<ContentObjectHeader> header = Create<ContentObjectHeader> ();
  	      header->SetName (Create<NameComponents>(contenuto));
   	      header->GetSignedInfo ().SetTimestamp (Simulator::Now ());
   	      header->GetSignature ().SetSignatureBits (0);

   	      // ***** I CONTENUTI HANNO TUTTI UN PAYLOAD VIRTUALE DI 1024 Byte *****
  	      Ptr<Packet> packet = Create<Packet> (1024);
  	      packet->AddHeader (*header);
  	      packet->AddTrailer (tail);

  	      // ********  AGGIUNTA DEL CONTENUTO NEL REPOSITORY ***************
  	      (*node)->GetObject<Repo> ()->Add (header, packet);
  	      
  	      if (simType == 2)
  	      {
  	         ndnGlobalRoutingHelper.AddOrigin (contenuto, (*node));
  	      }
	  }
	  fin_repo.close();
  }
  

  
  NS_LOG_UNCOND("*****  FINE INIZIALIZZAZIONE REPOSITORY  *****");



  // ***** DEFINIZIONE DEI NODI CONSUMER (SEMPRE A LIVELLO APPLICATIVO) RESPONSABILI DELLA GENERAZIONE DEGLI INTEREST. SOLO NODI EDGE

  NodeContainer consumerNodes, tn;   // Nella nuova sim sono tutti i nodi consumer
  consumerNodes = tn.GetGlobal();
  
  NS_LOG_UNCOND("Il numero di consumer è:\t" << consumerNodes.GetN());

//  consumerNodes.Add(Names::Find <Node> ("Edge0_1"));
//  uint32_t id_c1 = Names::Find <Node> ("Edge0_1")->GetId();


  // Install Ndn applications
  NS_LOG_UNCOND ("*** INSTALLAZIONE LIVELLO APPLICATIVO ***");
  //std::string prefix = "/prefix";
  
  
  std::string freq;
  ssInit << req_freq;
  freq = ssInit.str();
  ssInit.str("");

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  //consumerHelper.SetPrefix (prefix);
  //consumerHelper.SetAttribute ("Frequency", StringValue ("0.02")); // 100 interests a second
  consumerHelper.SetAttribute ("Frequency", StringValue (freq)); // La frequenza delle richieste viene passata da riga di comando.
  consumerHelper.SetAttribute ("NumberOfContents", StringValue ("90000")); // 10 different contents
  consumerHelper.SetAttribute ("q", StringValue ("0")); // Con q = 0 si ha una Zipf law pura
  //consumerHelper.SetAttribute ("s", StringValue ("1.2")); // 's' è l'esponente della zipf; alpha = 1.2
  consumerHelper.SetAttribute ("s", StringValue ("1")); // 's' è l'esponente della zipf; alpha = 1
  consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.000000)));
  //consumerHelper.SetAttribute ("Randomize", StringValue ("uniform")); // 100 interests a second
  ApplicationContainer consumers = consumerHelper.Install (consumerNodes);
  
  NS_LOG_UNCOND ("*** FINE INSTALLAZIONE LIVELLO APPLICATIVO ***");

  for (NodeContainer::Iterator node_app = consumerNodes.Begin(); node_app != consumerNodes.End(); ++node_app)
  {
	  (*node_app)->GetApplication(0)->GetObject<App>()->SetCumSum(cs_p);
	  NS_LOG_UNCOND ("Settaggio Puntatore CumSum Nodo:\t" << (*node_app)->GetId());
	  
  }
  
    // Creazione dei trace e dei file di output per printare i relativi contenuti delle cache

  std::string fnCache = "RISULTATI/PROVA/alpha_1/CACHE/Cache_Nodo_";
  std::string fnId = "RISULTATI/PROVA/alpha_1/ID/Sent_Nodo_";
  std::string fnRcv = "RISULTATI/PROVA/alpha_1/RICEVUTI/Received_Nodo_";
  std::string fnRcvApp = "RISULTATI/PROVA/alpha_1/RICEVUTI_APP/Received_App_Nodo_";
  
  std::stringstream fname;
  AsciiTraceHelper asciiTraceHelper;
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
  {
	  // ************ NOMI FILE ******************

	  // **** CACHE
	  fname << fnCache << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_cache = fname.str();
	  const char *filename_cache = nome_file_cache.c_str();
	  fname.str("");
	  
	  fname << fnId << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_id = fname.str();
	  const char *filename_id = nome_file_id.c_str();
	  fname.str("");
	  
	  fname << fnRcv << ((*node)->GetId()+1) << "." << extScenario;
	  std::string nome_file_rcv = fname.str();
	  const char *filename_rcv = nome_file_rcv.c_str();
	  fname.str("");
	  
	  fname << fnRcvApp << ((*node)->GetId()+1) << "." << extScenario;
      std::string nome_file_rcv_app = fname.str();
      const char *filename_rcv_app = nome_file_rcv_app.c_str();
      fname.str("");

	  // ******** STREAM DI OUTPUT ***************
	  Ptr<OutputStreamWrapper> streamCache = asciiTraceHelper.CreateFileStream(filename_cache);
	 
	  Ptr<OutputStreamWrapper> streamId = asciiTraceHelper.CreateFileStream(filename_id);
	  
	  Ptr<OutputStreamWrapper> streamRcv = asciiTraceHelper.CreateFileStream(filename_rcv); 
	  
	  Ptr<OutputStreamWrapper> streamRcvApp = asciiTraceHelper.CreateFileStream(filename_rcv_app);

	  // ***** ASSOCIAZIONE ALLE FUNZIONI CHE TRATTANO I VARI EVENTI DI TRACE *****
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("Cache", MakeBoundCallback(&PrintCacheTrace, streamCache));
	  
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("CID", MakeBoundCallback(&PrintSentId, streamId));
	  
	  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InData", MakeBoundCallback(&DataRicevutiTrace, streamRcv));
	  
	  (*node)->GetApplication(0)->GetObject<App>()->TraceConnectWithoutContext("ReceivedContentObjects",MakeBoundCallback(&DataRicevutiTrace, streamRcvApp));


  }


  //ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  //producerHelper.SetPrefix (prefix);
  //producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));           // ***** DEFINIZIONE DELLA DIMENSIONE DEL PAYLOAD VIRTUALE DEI CONTENUTI
//if(simType == 3)
//ApplicationContainer producers = producerHelper.Install (producerNodes);

  // Add /prefix origins to ndn::GlobalRouter
  //ndnGlobalRoutingHelper.AddOrigins (prefix, producer);
  //ndnGlobalRoutingHelper.AddOrigins(prefix, producerNodes);    // Install to all the nodes in the NodeContainer

  // Calculate and install FIBs
if(simType == 2)
{  
        ndnGlobalRoutingHelper.CalculateRoutes ();
}


  Simulator::Stop (finishTime);


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();

//   ******  STAMPA FINALE DELLA CACHE DI CIASCUN NODO

/*  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
    {
      std::cout << "Node #" << ((*node)->GetId ()+1) << std::endl;
      (*node)->GetObject<ndn::ContentStore> ()->Print (std::cout);
      std::cout << std::endl;
    }
*/

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
PrintCacheTrace(Ptr<OutputStreamWrapper> stream, uint32_t node_id)
{
	*stream->GetStream() << "*****\tSIM TIME = " << Simulator::Now().GetMilliSeconds() << "\t*****" << std::endl;
	Ptr<Node> pt = NodeList::GetNode(node_id);
	pt->GetObject<ndn::Repo>() ->Print(*stream->GetStream());
	pt->GetObject<ndn::ContentStore>() ->Print(*stream->GetStream());
	*stream->GetStream() << std::endl;
}

void
PrintSentId(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
}

void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" <<  face->GetId() << std::endl;
}


/*std::vector<double> *
Crea_CumSum (uint32_t m_N, double m_q, double m_s)
{
  NS_LOG_UNCOND("CREAZIONE CATALOGO!!");
  //m_N = numOfContents;

  NS_LOG_DEBUG (m_q << " and " << m_s << " and " << m_N);

  std::vector<double> m_Pcum (m_N + 1);

  m_Pcum[0] = 0.0;
  for (uint32_t i=1; i<=m_N; i++)
    {
      m_Pcum[i] = m_Pcum[i-1] + 1.0 / std::pow(i+m_q, m_s);
    }

  for (uint32_t i=1; i<=m_N; i++)
    {
      m_Pcum[i] = m_Pcum[i] / m_Pcum[m_N];
      NS_LOG_LOGIC ("Cumulative probability [" << i << "]=" << m_Pcum[i]);
  }
  //std::vector<double>* cs_pointer = &m_Pcum;
  //return cs_pointer;
  return &m_Pcum;
}*/


std::vector<double> *
Crea_CumSum (uint32_t m_N, double m_q, double m_s)
{
  NS_LOG_UNCOND("CREAZIONE CATALOGO COMUNE!!");
  //m_N = numOfContents;

  NS_LOG_DEBUG (m_q << " and " << m_s << " and " << m_N);

  std::vector<double>* m_Pcum = new std::vector<double> ();
  m_Pcum->resize(m_N + 1);

  m_Pcum->operator[](0) = 0.0;
  for (uint32_t i=1; i<=m_N; i++)
    {
      m_Pcum->operator[](i) = m_Pcum->operator[](i-1) + 1.0 / std::pow(i+m_q, m_s);
    }

  for (uint32_t i=1; i<=m_N; i++)
    {
      m_Pcum->operator[](i) = m_Pcum->operator[](i) / m_Pcum->operator[](m_N);
      //NS_LOG_LOGIC ("Cumulative probability [" << i << "]=" << m_Pcum[i]);
  }
  //std::vector<double>* cs_pointer = &m_Pcum;
  //return cs_pointer;
  return m_Pcum;
}


