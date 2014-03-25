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
main (int argc, char *argv[8])
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

  static ns3::GlobalValue g_numeroSIM ("g_numeroSIM",
                                   "The number of the current simulation in the same scenario",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());

  static ns3::GlobalValue g_simType ("g_simType",
                                   "The choosen Forwarding Strategy",
                                   ns3::UintegerValue(1),
                                   ns3::MakeUintegerChecker<uint32_t> ());


  //uint32_t nGrid = 3;
  Time finishTime = Seconds (115000.0);
  uint32_t contentCatalog = 0;          // Numero totale di contenuti
  double cacheToCatalog = 0;            // Rapporto tra cache size e content catalog
  uint32_t simType = 1;                 // Parametro passato da riga di comando per definire il tipo di simulazione.
  	  	  	  	  	  	  	  	  	    // 1 = FLOODING  (Default)
  	  	  	  	  	  	  	  	  	  	// 2 = BEST ROUTE
  	  	  	  	  	  	  	  	  	    // 3 = BF

  uint32_t numeroComponenti = 1;
  uint32_t soglia = 30;
  uint32_t numeroSIM = 1;              // Per ogni scenario verranno effettuati 3 run.             

  CommandLine cmd;
  //cmd.AddValue ("nGrid", "Number of grid nodes", nGrid);
  //cmd.AddValue ("finish", "Finish time", finishTime);
  cmd.AddValue ("contentCatalog", "Total number of contents", contentCatalog);
  cmd.AddValue ("cacheToCatalog", "Cache to Catalog Ratio", cacheToCatalog);
  cmd.AddValue ("simType", "Chosen Forwarding Strategy", simType);
  cmd.AddValue ("numeroComponenti", "Maximum Number of Components for the Small BF", numeroComponenti);
  cmd.AddValue ("soglia", "Update Threshold", soglia);
  cmd.AddValue ("numeroSIM", "Current Simulation Run", numeroSIM);

  cmd.Parse (argc, argv);

  Config::SetGlobal ("g_numeroSIM", UintegerValue(numeroSIM));
  Config::SetGlobal ("g_simType", UintegerValue(simType));
  
  std::string EXT = ".txt";
  std::stringstream scenario;
  std::string extScenario;  		// Estensione per distinguere i vari scenari simulati

  // *** Ad ogni RUN, la collocazione della edge network che farà da repository sarà casuale. Quindi devo estrarre un numero casuale da 0 a 22 (numero di Core router)
  // *** Inoltre, anche il numero di repository all'interno di quella rete varierà casualmente, da 1 a 3 (numero minimo di nodi in una edge network)
  // *** distribuendosi rispettivamente i contenuti
  Ptr<UniformRandomVariable> myRand = CreateObject<UniformRandomVariable> ();
  
//NS_LOG_UNCOND("Numero Casuale estratto 1:\t" << myRand->GetValue() << "\n");
  uint32_t repAttach = (uint32_t)( (int)( myRand->GetValue()*pow(10,5) ) % 22);
  
//NS_LOG_UNCOND("Numero Casuale estratto 2:\t" << myRand->GetValue() << "\n"); 
  uint32_t numRepo = (uint32_t) ( ( (int)( myRand->GetValue()*pow(10,5) ) %3)+1);
  
  //uint32_t repAttach = (uint32_t)((22 * rand()) / (RAND_MAX + 1.0));
  //uint32_t numRepo = (uint32_t)((3 * rand()) / (RAND_MAX + 1.0));

  std::string simulationType;


  switch (simType)
  {
  case(1):
	 simulationType = "Flooding";
  	 scenario << simulationType << "_" << numeroSIM << "_" << cacheToCatalog << "_" << repAttach << "_" << numRepo << "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
  	 extScenario = scenario.str();
  	 scenario.str("");
     break;
  case(2):
	 simulationType = "BestRoute";
  	 scenario << simulationType << "_"  << numeroSIM << "_" << cacheToCatalog << "_" << repAttach << "_" << numRepo << "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
     extScenario = scenario.str();
     scenario.str("");
     break;
  case(3):
	 simulationType = "BloomFilter";
  	 scenario << simulationType << "_" << numeroSIM << "_" << soglia << "_" << numeroComponenti << "_" << cacheToCatalog << "_" << repAttach << "_" << numRepo <<  "_" << SeedManager::GetSeed() << "_" << SeedManager::GetRun();
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

  // Calcolo della dimensione della cache
  uint32_t CacheSize = round((double)contentCatalog * cacheToCatalog);
  ssInit << CacheSize;
  std::string CACHE_SIZE = ssInit.str();
  ssInit.str("");


  // *******  CREAZIONE DELLA TOPOLOGIA TRAMITE LETTURA FILE ********
  AnnotatedTopologyReader topologyReader ("", 10);
  topologyReader.SetFileName ("/media/DATI/tortelli/Geant_Topo.txt");
  topologyReader.Read ();

  //Simulator::Schedule (Seconds (6),&Ipv4::SetDown,ipv46, ipv4ifIndex6);




  uint32_t numeroNodi = topologyReader.GetNodes().GetN();
  NS_LOG_UNCOND("Il numero di nodi è:\t" << numeroNodi);



  std::string REPO_SIZE;
  std::string PATH_REPO;
  uint32_t RepoSize;
  // *** SETTING DEL REPO SIZE
  switch(numRepo)
  {
  case(1):
		//RepoSize = contentCatalog;
        RepoSize = 104452;
        ssInit << RepoSize;
		REPO_SIZE = ssInit.str();
		ssInit.str("");
		PATH_REPO= "/media/DATI/tortelli/COPIE_SEED/repository1_1.txt";       // Vi è un solo repository che contiene tutti i files
	    break;
  case(2):
		//RepoSize = round(contentCatalog/2) + 1;
        RepoSize = (104452/2) + 1;
		ssInit << RepoSize;
		REPO_SIZE = ssInit.str();
		ssInit.str("");
		PATH_REPO= "/media/DATI/tortelli/COPIE_SEED/repository2_";         // Ci sono due file (2_1 e 2_2) con ugual numero di contenuti
		break;
  case(3):
		//RepoSize = round(contentCatalog/3) + 3;
        RepoSize = round(104452/3) + 3;		
		ssInit << RepoSize;
		REPO_SIZE = ssInit.str();
		ssInit.str("");
		PATH_REPO= "/media/DATI/tortelli/COPIE_SEED/repository3_";         // Ci sono tre file (3_1, 3_2 e 3_3) con ugual numero di contenuti
		break;
  default:
	  NS_LOG_UNCOND("NUMERO DI REPOSITORY NN CONSENTITO!\t" << numRepo);
	  exit(1);
	  break;
  }

  // *** VERIFICA PARAMETRI IMMESSI E CALCOLATI ****
  NS_LOG_UNCOND("CONTENT CATALOG:\t" << contentCatalog << "\n" << "CACHE/CATALOG:\t" << cacheToCatalog << "\n" << "SIM TYPE:\t" << simType << "\n"
		  << "RNG:\t" << runNumber << "\n" << "CACHE SIZE:\t" << CacheSize << "\n" << "REPO ATTACHMENT:\t" << repAttach << "\n" << "NUM_REPO:\t" << numRepo << "\n"
		  << "REPO SIZE" << REPO_SIZE << "\n" << "NUMERO SIM \t" << numeroSIM << "\n");

//  // ****************  CREAZIONE TOPOLOGIA RANDOM  ***************************
//
//  std::string adj_mat_file_name ("/home/michele/ndn-SIM/adj_matrix.txt");
//  std::string coord_file_name ("/home/michele/ndn-SIM/node_coord.txt");
//
//  std::vector<std::vector<bool> > Adj_Matrix;
//  Adj_Matrix = readAdjMat (adj_mat_file_name);
//
//  std::vector<std::vector<double> > coord_array;
//  coord_array = readCordinatesFile (coord_file_name);
//
//  int n_nodes = coord_array.size();
//  int matrixDimension = Adj_Matrix.size();
//
//  if (matrixDimension != n_nodes)
//    {
//      NS_FATAL_ERROR ("The number of lines in coordinates file (" << n_nodes << ") is not equal to the number of nodes in adjacency matrix size " << matrixDimension);
//    }
//
//  NS_LOG_INFO ("Create Nodes.");
//
//  NodeContainer nodes;   // Declare nodes objects
//  nodes.Create (n_nodes);
//
//  NS_LOG_INFO ("Create Links Between Nodes.");
//
//  PointToPointHelper p2p;
//  uint32_t linkCount = 0;
//
//  for (size_t i = 0; i < Adj_Matrix.size (); i++)
//    {
//      for (size_t j = 0; j < Adj_Matrix[i].size (); j++)
//        {
//           if (Adj_Matrix[i][j] == 1)
//            {
//              NodeContainer n_links = NodeContainer (nodes.Get (i), nodes.Get (j));
//              NetDeviceContainer n_devs = p2p.Install (n_links);
//              //ipv4_n.Assign (n_devs);
//              //ipv4_n.NewNetwork ();
//              linkCount++;
//              //NS_LOG_INFO ("matrix element [" << i << "][" << j << "] is 1");
//            }
//          else
//            {
//              //NS_LOG_INFO ("matrix element [" << i << "][" << j << "] is 0");
//            }
//        }
//    }
//  NS_LOG_INFO ("Number of links in the adjacency matrix is: " << linkCount);
//  NS_LOG_INFO ("Number of all nodes is: " << nodes.GetN ());
//
//  NS_LOG_INFO ("Allocate Positions to Nodes.");
//
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


  // *****************  INSTALLAZIONE DELLO STACK NDN A SECONDA DELLA TIPOLOGIA DI NODO ********************

  NS_LOG_INFO ("Installing Ndn stack on all nodes");
  ndn::StackHelper ndnHelper;
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;                           // ****** ROUTING HELPER

  // To FLOOD Interest we need to set the Forwarding strategy as well as the default routes for every node
if(simType == 3)
{  
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::SelectiveFlooding");                       // *** FORWARDING STRATEGY
  ndnHelper.SetDefaultRoutes(true);
}
else if(simType == 1)
{
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::Flooding");                       // *** FORWARDING STRATEGY
  ndnHelper.SetDefaultRoutes(true);
}
else
{
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::Flooding");                       // *** FORWARDING STRATEGY
  //ndnHelper.SetDefaultRoutes(true);
}



  // *** IDENTIFICAZIONE DEI NOMI DEI NODI CHE CONTERRANNO I REPOSITORY
  // ** Stringhe per identificare i nodi **
  uint32_t numCoreRouter = 22;
  uint32_t maxEdge = 6;            // Numero massimo di nodi per una edge network
  std::string core = "Core";
  std::string edge = "Edge";
  std::stringstream ssEdge;
  std::vector<uint32_t> repositoriesID (numRepo);
  std::string rp;

  NodeContainer producerNodes;
    //producerNodes.Add(Names::Find <Node> ("Edge13_1"));  // ** Identifico gli ID associati ai repository
  for(uint32_t i = 0; i < numRepo; i++)
  {
	  ssEdge << edge << (repAttach) << "_" << i;
	  rp = ssEdge.str();
	  repositoriesID[i] =  Names::Find<Node>(rp)->GetId();
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
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", "0");
		  ndnHelper.SetRepository("ns3::ndn::repo::Persistent", "MaxSize", REPO_SIZE);
	  }
	  else
	  {
		  ndnHelper.SetContentStore("ns3::ndn::cs::Lru", "MaxSize", CACHE_SIZE);
		  ndnHelper.SetRepository("ns3::ndn::repo::Persistent", "MaxSize", "0");
	  }

	  ndnHelper.Install((*node));

	  // *********  ACQUISIZIONE DEI PARAMETRI NECESSARI ALL'INIZIALIZZAZIONE DEI BF  ********************

      if (simType == 3)
      {
    	    Ptr<bloom_filter> bf_p = (*node)->GetObject<bloom_filter>();
	        uint32_t interf = bf_p->num_interfaces_;                                        // Ottengo il numero di interfacce del nodo.
	        uint32_t numero_comp = bf_p->numComp_;                                          // Numero di Componenti per Nome.
	        double   fpBfStable = bf_p->desired_fpBfStable_;                                // Probabilità falso positivo SBF.
	        double   fpBfComp   = bf_p->desired_fpBfComp_;                                  // Probabilità falso positivo BF piccoli.
	        unsigned long long int   numElem_BfStable = bf_p->proj_numElem_BfStable_;       // Numero contenuti previsti SBF.
	        unsigned long long int   numElem_BfComp   = bf_p->proj_numElem_BfComp_;         // Numero contenuti previsti per componenti BF piccoli.
	        unsigned int   c_Width = bf_p->cell_Width_;                                     // Numero di Bit per cella (stable and counting)

	        // *********  FUNZIONE DI INIZIALIZZAZIONE DEI BF DEL NODO ********************

	        (*node)->GetObject<bloom_filter>()->initBf(interf, numero_comp, fpBfStable, fpBfComp, numElem_BfStable, numElem_BfComp, c_Width);


////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////


//	        //  *********  CREAZIONE DELLE ENTRY PIT PERMANENTI PER POTER INVIARE I MESSAGGI DI UPDATE  *********
//
//	        Ptr<NameComponents> desiredContent_0 = Create<NameComponents> ("/update_0");
//	        Ptr<NameComponents> desiredContent_1 = Create<NameComponents> ("/update_1");
//	        InterestHeader interestHeader_0, interestHeader_1;
//
//	        interestHeader_0.SetName				(desiredContent_0);
//	        interestHeader_1.SetName				(desiredContent_1);
//
//	        Ptr<pit::Entry> pitEntry_0 = (*node)->GetObject<Pit>()->Create(&interestHeader_0);
//	        Ptr<pit::Entry> pitEntry_1 = (*node)->GetObject<Pit>()->Create(&interestHeader_1);
//
//	        pitEntry_0->SetExpireTime(finishTime);
//	        pitEntry_1->SetExpireTime(finishTime);                       // **** PER RENDERLE PERMANENTI, L'EXIRE TIME VIENE POSTO PARI ALLA DURATA DELLA SIM
//
//	        // AGGIUNGO ALLA PIT ENTRY TUTTE LE INTERFACCE DEL NODO IN MODO DA INVIARE IN BROADCAST I MESSAGGI DI UPDATE
//	        Ptr<Face> facePoint;
//	        uint32_t num_interfacce = (*node)->GetNDevices();
//	        for(uint32_t i=0; i < num_interfacce; i++)
//	        {
//	            facePoint = (*node)->GetObject<L3Protocol>()->GetFaceByNetDevice((*node)->GetDevice(i));
//	            pitEntry_0->AddIncoming(facePoint);
//	            pitEntry_1->AddIncoming(facePoint);
//	        }
//
//	        // ***** INIZIALIZZAZIONE DELLA SOGLIA PER L'INVIO DEI MESSAGGI DI UPDATE
//	        (*node)->GetObject<ForwardingStrategy>()->setThreshold((*node)->GetId());

	  }

  }


  //ndnHelper.SetContentStore ("ns3::ndn::cs::Lru", "MaxSize", CACHE_SIZE);                 // ****** CONTENT STORE
  //ndnHelper.SetRepository("ns3::ndn::repo::Persistent", "MaxSize", REPO_SIZE);            // ****** REPOSITORY
  //ndnHelper.InstallAll ();

  if(simType == 2)                                                             // Se è lo scenario BestRoute, ho bisogno di installare il Routing Helper
  {
	ndnGlobalRoutingHelper.InstallAll ();
  }


  /*****************************************************************************
     * MICHELE -- INIZIALIZZAZIONE DEI/DEL REPOSITORY CON COPIE SEED   (OK)
     */

  std::string line;
  Ptr<Node> nd;
  const char *PATH_COMPL;
  std::ifstream fin;
  //std::istringstream iss;
  //uint32_t sizeGlobal;
  std::stringstream ss;
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("\t");
  bool primo = true;

  switch (numRepo)
  {
  case(1):

		// Il repository è unico
		nd = NodeList::GetNode(repositoriesID[0]);
  	  	PATH_COMPL = PATH_REPO.c_str();
                // SETTO IL FILE CHE PUNTA AL REPOSITORY
                //nd->GetObject<ForwardingStrategy>()->setRepository(PATH_COMPL);



  	    fin.open(PATH_COMPL);
  	    if(!fin) {
  	    	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n";
  	      	exit(0);
  	    }

  	    // *******  LETTURA DELLE COPIE SEED E INSERIMENTO NEL REPOSITORY

  	    //uint32_t v;
  	    while(std::getline(fin, line))
  	    {
  	      //iss >> line >> v;
// *** ADESSO IL REPOSITORY INSERISCE I CONTENUTI SENZA CHUNK, QUINDI NON HO PIÙ BISOGNO DEL TOKENIZER              
              //tokenizer tokens_space(line, sep);
              //tokenizer::iterator it_space=tokens_space.begin();
              //std::string contenutoChunk = *it_space;
              //++it_space;
              //std::string expChunk_string = *it_space;

              //line = contenutoChunk;

  	      static ContentObjectTail tail;
  	      Ptr<ContentObjectHeader> header = Create<ContentObjectHeader> ();
  	      header->SetName (Create<NameComponents>(line));
  	      header->GetSignedInfo ().SetTimestamp (Simulator::Now ());
  	      header->GetSignature ().SetSignatureBits (0);

  	      //NS_LOG_INFO ("node("<< ((*node)->GetId()+1) <<") uploading ContentObject:\n" << boost::cref(*header) << std::endl);

  	      // ***** I CONTENUTI HANNO TUTTI UN PAYLOAD VIRTUALE DI 1024 Byte *****
  	      Ptr<Packet> packet = Create<Packet> (1024);
  	      packet->AddHeader (*header);
  	      packet->AddTrailer (tail);

  	      // ********  AGGIUNTA DEL CONTENUTO NEL REPOSITORY ***************

  	      nd->GetObject<Repo> ()->Add (header, packet);

  	      if (simType == 2 && primo == true)
		  {
			std::string root = "/";
			ndnGlobalRoutingHelper.AddOrigin (root, nd);
			Ptr<NetDevice> device = nd->GetDevice (0);
			Ptr<NetDeviceFace> face = CreateObject<NetDeviceFace> (nd, device);
			ndnHelper.AddRoute (nd, "/", face, 1);
			primo = false;
			NS_LOG_UNCOND("ROTTA AGGIUNTA!");
		  }


/*              {
                //++it_space;
                //std::string expChunk_string = *it_space;
        	const char* expChunk = expChunk_string.c_str();
        	uint32_t numChunk = std::atoi(expChunk);
        	if(numChunk != 0)     // Si tratta del primo chunk del contenuto, quindi devo aggiornare download_time_file
                {
        		Ptr<NameComponents> requestedContent = Create<NameComponents> (line);
        		uint32_t numComponents = requestedContent->GetComponents().size();
                
                        std::list<std::string> nome_co (numComponents);
			nome_co.assign(requestedContent->begin(), requestedContent->end());
			std::list<std::string>::iterator it;
			it = nome_co.begin();           			// L'iterator punta alla prima componente
 
        		nome_co.pop_back();
        		--numComponents;
        		
         		for(uint32_t t = 0; t < numComponents; t++)
        		{
        			ss << "/" << *it;
        			it++;
        		}
        		std::string contenutoNoChunk = ss.str();
//NS_LOG_UNCOND("Stesso contenuto senza il chunk:\t" << contenutoNoChunk);
        		ss.str("");
  	    	
                        ndnGlobalRoutingHelper.AddOrigin (contenutoNoChunk, nd);  // Aggiungo solo il nome del contenuto senza chunk
                }
              }
*/

////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////

//  	      if (simType == 3)
//  	      {
//
//  	      	  // (NB) Verifico il numero di componenti del nome da inserire e lo paragono al numero max di componenti che si è deciso di considerare
//  	      	  size_t size;
//  	      	  if(header->GetNamePtr()->GetComponents().size() >= nd->GetObject<bloom_filter>()->numComp_)
//  	      	  {
//  	      		  size = nd->GetObject<bloom_filter>()->numComp_;
//
//  	      	  }
//  	      	  else
//  	      	  {
//  	      		  size = header->GetNamePtr()->GetComponents().size();
//  	      	  }
//
//
//  	      	  // ****** CREAZIONE DELLE COMPONENTE DEL NOME DEL CONTENUTO
//  	      	  std::list<std::string> nome_interest(header->GetNamePtr()->GetComponents().size());
//  	      	  nome_interest.assign(header->GetNamePtr()->GetComponents().begin(), header->GetNamePtr()->GetComponents().end());
//  	      	  std::list<std::string>::iterator it;
//  	      	  it = nome_interest.begin();
//
//  	      	  // ****** INSERIMENTO DELLE VARIE COMPONENTI NEI MICRO BF DEL BF_REPO
//  	      	  for(std::size_t i=0; i < size; i++)
//  	      	  {
//  	            /**
//  	             *   La funzione "insert_comp" ha come parametri:
//  	             *   string contenuto = nome del contenuto da inserire;
//  	             *   size_t comp = numero della componente del nome da inserire
//  	             *   boolean counting = serve per distinguere tra inserimento nel counting BF di una interfaccia (true) e inserimento nel BF del CS o del REPO (false)
//  	             *   boolean firstTime = serve per distinguere tra inserimento nel BF_REPO (true) e inserimento nel BF_CS
//  	             *   size_t interface = eventuale interfaccia del relativo BF_COUNTING
//  	             */
//  	      		nd->GetObject<bloom_filter>()->insert_comp((*it),i,false,true,0);
//  	      	  	++it;
//  	      	  }
//
//  	      	  // ****** NELLA FASE DI STARTUP DI UN NODO, LE INFO SULLE COPIE SEED POSSEDUTE VENGONO INVIATE COME UPDATE UPDATE DIRTY ZERO, IN MODO TALE DA
//  	      	  // ****** INCREMENTARE IL COUNTING BF DELL'INTERFACCIA CHE RICEVE QUESTO UPDATE, IN CORRISPONDENZA DELLE POSIZIONI INDICATE
//
//  	      	    // nd->GetObject<bloom_filter>()->initUpdDirtyZero(size);
//
//  	      }
  	    }
  	    fin.close();
////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////
//  	    sizeGlobal = nd->GetObject<bloom_filter>()->numComp_;
//  	    if (simType == 3)
//  	    	nd->GetObject<bloom_filter>()->initUpdDirtyZero(sizeGlobal);
  	    break;

  case(2):
  case(3):

  	    // Questa volta il repository è più di uno
  	   for(uint32_t i = 0; i < repositoriesID.size(); i++)
  	   {
  		   bool more_repo = true;
  		   nd = NodeList::GetNode(repositoriesID[i]);
  		   ssInit << PATH_REPO << (i+1) << EXT;
  		   PATH_COMPL = ssInit.str().c_str();
                   ssInit.str("");
                   // SETTO IL FILE CHE PUNTA AL REPOSITORY
                   //nd->GetObject<ForwardingStrategy>()->setRepository(PATH_COMPL);
           

// ***** COMMENTO INSERITO
  		   fin.open(PATH_COMPL);
  		   if(!fin) {
  		       	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n" << PATH_COMPL << "\t" << repositoriesID.size() << "\n";
  		       	exit(0);
  		   }

  		   // *******  LETTURA DELLE COPIE SEED E INSERIMENTO NEL REPOSITORY
  		   while(std::getline(fin, line))
  		   {
                      //tokenizer tokens_space(line, sep);
                      //tokenizer::iterator it_space=tokens_space.begin();
                      //std::string contenutoChunk = *it_space;
                      //++it_space;
                      //std::string expChunk_string = *it_space;

                      //line = contenutoChunk;
  		      
                      static ContentObjectTail tail;
  		      Ptr<ContentObjectHeader> header = Create<ContentObjectHeader> ();
  		      header->SetName (Create<NameComponents>(line));
  		      header->GetSignedInfo ().SetTimestamp (Simulator::Now ());
  		      header->GetSignature ().SetSignatureBits (0);

  		      //NS_LOG_INFO ("node("<< ((*node)->GetId()+1) <<") uploading ContentObject:\n" << boost::cref(*header) << std::endl);

  		      // ***** I CONTENUTI HANNO TUTTI UN PAYLOAD VIRTUALE DI 1024 Byte *****
  		   	  Ptr<Packet> packet = Create<Packet> (1024);
  		   	  packet->AddHeader (*header);
  		   	  packet->AddTrailer (tail);

  		   	  // ********  AGGIUNTA DEL CONTENUTO NEL REPOSITORY ***************

  		   	  nd->GetObject<Repo> ()->Add (header, packet);

  	      if (simType == 2 && more_repo == true)
		  {
			std::string root = "/";
			ndnGlobalRoutingHelper.AddOrigin (root, nd);
			more_repo = false;
		  }

/*              {
				//NS_LOG_UNCOND("Iterator:\t" << *it_space);
                //++it_space;
                //std::string expChunk_string = *it_space;
                //NS_LOG_UNCOND("Contenuto:\t" << contenutoChunk << "\t Chunk:\t" << expChunk_string);
				const char* expChunk = expChunk_string.c_str();
				uint32_t numChunk = std::atoi(expChunk);
				if(numChunk != 0)     // Si tratta del primo chunk del contenuto, quindi devo aggiornare download_time_file
                {
        		Ptr<NameComponents> requestedContent = Create<NameComponents> (line);
        		uint32_t numComponents = requestedContent->GetComponents().size();
                
                        std::list<std::string> nome_co (numComponents);
			nome_co.assign(requestedContent->begin(), requestedContent->end());
			std::list<std::string>::iterator it;
			it = nome_co.begin();           			// L'iterator punta alla prima componente
 
        		nome_co.pop_back();
        		--numComponents;
        		
         		for(uint32_t t = 0; t < numComponents; t++)
        		{
        			ss << "/" << *it;
        			it++;
        		}
        		std::string contenutoNoChunk = ss.str();
//NS_LOG_UNCOND("Stesso contenuto senza il chunk:\t" << contenutoNoChunk);
        		ss.str("");
  	    	
                        ndnGlobalRoutingHelper.AddOrigin (contenutoNoChunk, nd);  // Aggiungo solo il nome del contenuto senza chunk
                }
              }
*/
///  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////
//  		   	  if (simType == 3)
//  		   	  {
//
//  		   	   	  // (NB) Verifico il numero di componenti del nome da inserire e lo paragono al numero max di componenti che si è deciso di considerare
//  		   	   	  size_t size;
//  		   	   	  if(header->GetNamePtr()->GetComponents().size() >= nd->GetObject<bloom_filter>()->numComp_)
//  		   	   	  {
//  		   	   		  size = nd->GetObject<bloom_filter>()->numComp_;
//  		   	   	  }
//  		   	   	  else
//  		   	   	  {
//  		   	   		  size = header->GetNamePtr()->GetComponents().size();
//  		   	   	  }
//
//  		   	   	  // ****** CREAZIONE DELLE COMPONENTE DEL NOME DEL CONTENUTO
//  		   	   	  std::list<std::string> nome_interest(header->GetNamePtr()->GetComponents().size());
//  		   	   	  nome_interest.assign(header->GetNamePtr()->GetComponents().begin(), header->GetNamePtr()->GetComponents().end());
//  		   	   	  std::list<std::string>::iterator it;
//  		   	   	  it = nome_interest.begin();
//
//  		   	   	  // ****** INSERIMENTO DELLE VARIE COMPONENTI NEI MICRO BF DEL BF_REPO
//  		   	   	  for(std::size_t i=0; i < size; i++)
//  		   	   	  {
//  		   	           /**
//  		   	           *   La funzione "insert_comp" ha come parametri:
//  		   	           *   string contenuto = nome del contenuto da inserire;
//  		   	           *   size_t comp = numero della componente del nome da inserire
//  		   	           *   boolean counting = serve per distinguere tra inserimento nel counting BF di una interfaccia (true) e inserimento nel BF del CS o del REPO (false)
//  		   	           *   boolean firstTime = serve per distinguere tra inserimento nel BF_REPO (true) e inserimento nel BF_CS
//  		   	           *   size_t interface = eventuale interfaccia del relativo BF_COUNTING
//  		   	           */
//  		   	     		nd->GetObject<bloom_filter>()->insert_comp((*it),i,false,true,0);
//  		   	       	  	++it;
//  		   	   	  }
//
//  		   	   	  // ****** NELLA FASE DI STARTUP DI UN NODO, LE INFO SULLE COPIE SEED POSSEDUTE VENGONO INVIATE COME UPDATE UPDATE DIRTY ZERO, IN MODO TALE DA
//  		   	   	  // ****** INCREMENTARE IL COUNTING BF DELL'INTERFACCIA CHE RICEVE QUESTO UPDATE, IN CORRISPONDENZA DELLE POSIZIONI INDICATE
//
//  		   	      // nd->GetObject<bloom_filter>()->initUpdDirtyZero(size);
//
//  		   	  }

  		   }
  		   fin.close();
////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////
//  		 sizeGlobal = nd->GetObject<bloom_filter>()->numComp_;
//   		 if (simType == 3)
//  		   	    	nd->GetObject<bloom_filter>()->initUpdDirtyZero(sizeGlobal);

  	   }
  	   break;
  }



  		   // *** NBB : PER ADESSO IPOTIZZIAMO CHE I REPOSITORY NON INVIINO DEGLI UPDATE ALTRIMENTI SAREBBERO TUTTI SETTATI AD 1. GLI UPDATE, INVECE, VERRANNO
  	       // *** INVIATI AL RAGGIUNGIMENTO DELLA SOGLIA PREFISSATA ( LA SUCCESSIVA PARTE DI CODICE COMMENTATA ERA NEI WHILE PRECEDENTI.

//  	          // ********  AGGIUNTA DELLE INFO NEL BF_REPO  **********
//
//  	      	  // (NB) Verifico il numero di componenti del nome da inserire e lo paragono al numero max di componenti che si è deciso di considerare
//  	      	  size_t size;
//  	      	  if(header->GetNamePtr()->GetComponents().size() >= (*node)->GetObject<bloom_filter>()->numComp_)
//  	      	  {
//  	      		  size = (*node)->GetObject<bloom_filter>()->numComp_;
//  	      	  }
//  	      	  else
//  	      	  {
//  	      		  size = header->GetNamePtr()->GetComponents().size();
//  	      	  }
//
//  	      	  // ****** CREAZIONE DELLE COMPONENTE DEL NOME DEL CONTENUTO
//  	      	  std::list<std::string> nome_interest(header->GetNamePtr()->GetComponents().size());
//  	      	  nome_interest.assign(header->GetNamePtr()->GetComponents().begin(), header->GetNamePtr()->GetComponents().end());
//  	      	  std::list<std::string>::iterator it;
//  	      	  it = nome_interest.begin();
//
//  	      	  // ****** INSERIMENTO DELLE VARIE COMPONENTI NEI MICRO BF DEL BF_REPO
//  	      	  for(std::size_t i=0; i < size; i++)
//  	      	  {
//  	            /**
//  	             *   La funzione "insert_comp" ha come parametri:
//  	             *   string contenuto = nome del contenuto da inserire;
//  	             *   size_t comp = numero della componente del nome da inserire
//  	             *   boolean counting = serve per distinguere tra inserimento nel counting BF di una interfaccia (true) e inserimento nel BF del CS o del REPO (false)
//  	             *   boolean firstTime = serve per distinguere tra inserimento nel BF_REPO (true) e inserimento nel BF_CS
//  	             *   size_t interface = eventuale interfaccia del relativo BF_COUNTING
//  	             */
//  	      		(*node)->GetObject<bloom_filter>()->insert_comp((*it),i,false,true,0);
//  	      	  	++it;
//  	      	  }
//
//  	      	  // ****** NELLA FASE DI STARTUP DI UN NODO, LE INFO SULLE COPIE SEED POSSEDUTE VENGONO INVIATE COME UPDATE UPDATE DIRTY ZERO, IN MODO TALE DA
//  	      	  // ****** INCREMENTARE IL COUNTING BF DELL'INTERFACCIA CHE RICEVE QUESTO UPDATE, IN CORRISPONDENZA DELLE POSIZIONI INDICATE
//
//  	      	     (*node)->GetObject<bloom_filter>()->initUpdDirtyZero(size);

//
//
//  	        //	  ndnGlobalRoutingHelper.AddOrigin (line, *node);          // **** CREAZIONE DELLA FIB PER QUESTO CONTENUTO TRAMITE IL GLOBAL ROUTING HELPER.
//  	      	                                                               // **** LE FIB INSTALLATE SUI VARI NODI VERRANNO CALCOLATE IN BASE AD UN ALGORITMO
//  	      	                                                               // **** CHE GARANTISCHE LO SHORTEST PATH PER QUEL CONTENUTO
//  	      	}
//  	      	fin.close();
//
//  }

  // Stringhe per i vari parametri tracciati

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

  //**** UPDATE (Inviati e Ricevuti)
////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////
//  std::string fnUpdateD0Inviati = "RISULTATI/UPDATE/UpdateD0_INVIATI/UpdateD0INVIATI_Nodo_";
//  std::string fnUpdateD1Inviati = "RISULTATI/UPDATE/UpdateD1_INVIATI/UpdateD1INVIATI_Nodo_";
//  std::string fnUpdateD0Ricevuti = "RISULTATI/UPDATE/UpdateD0_RICEVUTI/UpdateD0RICEVUTI_Nodo_";
//  std::string fnUpdateD1Ricevuti = "RISULTATI/UPDATE/UpdateD1_RICEVUTI/UpdateD1RICEVUTI_Nodo_";
//  std::string fnPercD0 = "RISULTATI/UPDATE/PERCENTUALI/D0/UpdateD0_Perc_Nodo_";
//  std::string fnPercD1 = "RISULTATI/UPDATE/PERCENTUALI/D1/UpdateD1_Perc_Nodo_";

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

////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////
//	  if (simType == 3)
//	  {
//		  // **** UPDATE
//		  fname << fnUpdateD0Inviati << ((*node)->GetId()+1) << "." << extScenario;
//		  std::string nome_file_updateD0_inviati = fname.str();
//		  const char *filename_updateD0_inviati = nome_file_updateD0_inviati.c_str();
//		  fname.str("");
//
//		  fname << fnUpdateD1Inviati << ((*node)->GetId()+1) << "." << extScenario;
//		  std::string nome_file_updateD1_inviati = fname.str();
//		  const char *filename_updateD1_inviati = nome_file_updateD1_inviati.c_str();
//		  fname.str("");
//
//	      fname << fnUpdateD0Ricevuti << ((*node)->GetId()+1) << "." << extScenario;
//	      std::string nome_file_updateD0_ricevuti = fname.str();
//	      const char *filename_updateD0_ricevuti = nome_file_updateD0_ricevuti.c_str();
//	      fname.str("");
//
//	      fname << fnUpdateD1Ricevuti << ((*node)->GetId()+1) << "." << extScenario;
//	      std::string nome_file_updateD1_ricevuti = fname.str();
//	      const char *filename_updateD1_ricevuti = nome_file_updateD1_ricevuti.c_str();
//	      fname.str("");
//
//	      fname << fnPercD0 << ((*node)->GetId()+1) << "." << extScenario;
//	      std::string nome_file_updateD0_perc = fname.str();
//	      const char *filename_updateD0_perc = nome_file_updateD0_perc.c_str();
//	      fname.str("");
//
//	      fname << fnPercD1 << ((*node)->GetId()+1) << "." << extScenario;
//	      std::string nome_file_updateD1_perc = fname.str();
//	      const char *filename_updateD1_perc = nome_file_updateD1_perc.c_str();
//	      fname.str("");
//
//
//	      Ptr<OutputStreamWrapper> streamUpdateD0Inviati = asciiTraceHelper.CreateFileStream(filename_updateD0_inviati);
//	      Ptr<OutputStreamWrapper> streamUpdateD0Ricevuti = asciiTraceHelper.CreateFileStream(filename_updateD0_ricevuti);
//	      Ptr<OutputStreamWrapper> streamUpdateD1Inviati = asciiTraceHelper.CreateFileStream(filename_updateD1_inviati);
//	      Ptr<OutputStreamWrapper> streamUpdateD1Ricevuti = asciiTraceHelper.CreateFileStream(filename_updateD1_ricevuti);
//	      Ptr<OutputStreamWrapper> streamUpdateD0Perc = asciiTraceHelper.CreateFileStream(filename_updateD0_perc);
//	      Ptr<OutputStreamWrapper> streamUpdateD1Perc = asciiTraceHelper.CreateFileStream(filename_updateD1_perc);
//
//
//	      (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutUpdateD0", MakeBoundCallback(&UpdateD0D1InviatiTrace, streamUpdateD0Inviati));
//   		  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("OutUpdateD1", MakeBoundCallback(&UpdateD0D1InviatiTrace, streamUpdateD1Inviati));
//   		  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InUpdateD0", MakeBoundCallback(&UpdateD0D1RicevutiTrace, streamUpdateD0Ricevuti));
//   		  (*node)->GetObject<ForwardingStrategy>()->TraceConnectWithoutContext("InUpdateD1", MakeBoundCallback(&UpdateD0D1RicevutiTrace, streamUpdateD1Ricevuti));
//   		  (*node)->GetObject<bloom_filter>()->TraceConnectWithoutContext("D0_Perc", MakeBoundCallback(&PercD0D1Trace, streamUpdateD0Perc));
//   		  (*node)->GetObject<bloom_filter>()->TraceConnectWithoutContext("D1_Perc", MakeBoundCallback(&PercD0D1Trace, streamUpdateD1Perc));
//
//	  }

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


  // ***** POSSIBILITÀ DI DEFINIRE DEI NODI PRODUCER (A LIVELLO APPLICATIVO)

  //NodeContainer producerNodes;
  //producerNodes.Add(Names::Find <Node> ("Edge13_1"));

  // ***** DEFINIZIONE DEI NODI CONSUMER (SEMPRE A LIVELLO APPLICATIVO) RESPONSABILI DELLA GENERAZIONE DEGLI INTEREST. SOLO NODI EDGE

  NodeContainer consumerNodes;

  for(uint32_t i = 0; i < numCoreRouter; i++)
  {
     for(uint32_t j = 0; j < maxEdge; j++)
     {
    	 bool isRepo = false;
    	 ssEdge << edge << i << "_" << j;
    	 std::string cons_edge = ssEdge.str();
    	 ssEdge.str("");
         Ptr<Node> ce = Names::Find<Node>(cons_edge);
    	 if(ce != NULL) // Il nodo puntato esiste
    	 {
    		 // Devo verificare se se tratta di un repository
    		 for(uint32_t z = 0; z < repositoriesID.size(); z++)
    		 {
    			 if (ce->GetId() == repositoriesID[z])
    				 isRepo = true;
    		 }
    		 if(!isRepo)
    			 consumerNodes.Add(ce);
    	 }
    	 else	// Non ci sono più edge su questo core router
    		break;

     }
  }

  NS_LOG_UNCOND("Il numero di consumer è:\t" << consumerNodes.GetN());

//  consumerNodes.Add(Names::Find <Node> ("Edge0_1"));
//  consumerNodes.Add(Names::Find <Node> ("Edge19_1"));
//  uint32_t id_c1 = Names::Find <Node> ("Edge0_1")->GetId();
//  uint32_t id_c2 = Names::Find <Node> ("Edge19_1")->GetId();


  // Install Ndn applications
  NS_LOG_INFO ("Installing Applications");
  std::string prefix = "/prefix";

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");                    // ***** CARATTERIZZAZIONE DELL'APPLICAZIONE CONSUMER
  consumerHelper.SetPrefix (prefix);
  consumerHelper.SetAttribute ("Frequency", StringValue ("1"));               // ***** FREQUENZA DI GENERAZIONE (/s) DEGLI INTEREST
  consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.000000)));
  ApplicationContainer consumers = consumerHelper.Install (consumerNodes);


  // ***** TRACE A LIVELLO APPLICATIVO PER GLI INTEREST RITRASMESSI ****
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

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  //producerHelper.SetPrefix (prefix);
  //producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));           // ***** DEFINIZIONE DELLA DIMENSIONE DEL PAYLOAD VIRTUALE DEI CONTENUTI

////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////
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


  // ******   IMPLEMENTAZIONE DEL LINK FAILURE   ******


Ptr<Node> nodoOrig, nodoDest;
Ptr<L3Protocol> ndnOrig, ndnDest;
Ptr<NetDeviceFace> faceOrig, faceDest;
Ptr<PointToPointNetDevice> ndOrig, ndDest;
Ptr<GlobalRouter> grOrig, grDest;
Ptr<bloom_filter> bfOrig, bfDest;
uint32_t idFaceOrig, idFaceDest;

switch(repAttach)
{
  case(0):
		  nodoOrig = Names::Find<Node>("Core0");
  	  	  nodoDest = Names::Find<Node>("Core1");
  	  	  ndnOrig = nodoOrig->GetObject<L3Protocol>();
  	  	  ndnDest = nodoDest->GetObject<L3Protocol>();
  	  	  bfOrig = nodoOrig->GetObject<bloom_filter>();
  	  	  bfDest = nodoDest->GetObject<bloom_filter>();
  	  	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(0));
  	      if (faceOrig == 0)
  		  {
  		    NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core0!");
  		    exit(1);
  		  }
  	      faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
  	      if (faceDest == 0)
  		  {
  		    NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core1!");
  		    exit(1);
  		  }
  	      idFaceOrig = faceOrig->GetId();
  	      idFaceDest = faceDest->GetId();
  	      ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
  	      ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
   	      grOrig = nodoOrig->GetObject<GlobalRouter> ();
	      grDest = nodoDest->GetObject<GlobalRouter> ();

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
  	  	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (20005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

  	  	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (40005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

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
  case(11):
		  nodoOrig = Names::Find<Node>("Core11");
  	  	  nodoDest = Names::Find<Node>("Core10");
  	  	  ndnOrig = nodoOrig->GetObject<L3Protocol>();
  	  	  ndnDest = nodoDest->GetObject<L3Protocol>();
  	  	  bfOrig = nodoOrig->GetObject<bloom_filter>();
  	  	  bfDest = nodoDest->GetObject<bloom_filter>();
  	  	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(0));
  	      if (faceOrig == 0)
  		  {
  		    NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core11!");
  		    exit(1);
  		  }
  	      faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(1));
  	      if (faceDest == 0)
  		  {
  		    NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core10!");
  		    exit(1);
  		  }
  	      idFaceOrig = faceOrig->GetId();
  	      idFaceDest = faceDest->GetId();
  	      ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
  	      ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
   	      grOrig = nodoOrig->GetObject<GlobalRouter> ();
	      grDest = nodoDest->GetObject<GlobalRouter> ();

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
  	  	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (20005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

  	  	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (40005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

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
		  nodoOrig = Names::Find<Node>("Core13");
  	  	  nodoDest = Names::Find<Node>("Core14");
  	  	  ndnOrig = nodoOrig->GetObject<L3Protocol>();
  	  	  ndnDest = nodoDest->GetObject<L3Protocol>();
  	  	  bfOrig = nodoOrig->GetObject<bloom_filter>();
  	  	  bfDest = nodoDest->GetObject<bloom_filter>();
  	  	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(1));
  	      if (faceOrig == 0)
  		  {
  		    NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core13!");
  		    exit(1);
  		  }
  	      faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
  	      if (faceDest == 0)
  		  {
  		    NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core14!");
  		    exit(1);
  		  }
  	      idFaceOrig = faceOrig->GetId();
  	      idFaceDest = faceDest->GetId();
  	      ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
  	      ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
   	      grOrig = nodoOrig->GetObject<GlobalRouter> ();
	      grDest = nodoDest->GetObject<GlobalRouter> ();

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
  	  	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (20005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

  	  	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
  	  	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                      Simulator::Schedule (Seconds (40005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

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
  		  nodoOrig = Names::Find<Node>("Core10");
    	  nodoDest = Names::Find<Node>("Core9");
    	  ndnOrig = nodoOrig->GetObject<L3Protocol>();
    	  ndnDest = nodoDest->GetObject<L3Protocol>();
    	  bfOrig = nodoOrig->GetObject<bloom_filter>();
    	  bfDest = nodoDest->GetObject<bloom_filter>();
    	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(0));
    	  if (faceOrig == 0)
    	  {
    		 NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core10!");
    		 exit(1);
    	  }
    	  faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
    	  if (faceDest == 0)
    	  {
    		 NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core9!");
    		 exit(1);
    	  }
    	  idFaceOrig = faceOrig->GetId();
    	  idFaceDest = faceDest->GetId();
    	  ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
    	  ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
     	  grOrig = nodoOrig->GetObject<GlobalRouter> ();
  	      grDest = nodoDest->GetObject<GlobalRouter> ();

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
    	     Simulator::Schedule (Seconds (10005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
    	     Simulator::Schedule (Seconds (10005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

    	     	 Simulator::Schedule (Seconds (20005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

    	  	 Simulator::Schedule (Seconds (30005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
    	  	 Simulator::Schedule (Seconds (30005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                 Simulator::Schedule (Seconds (40005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

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
  		  nodoOrig = Names::Find<Node>("Core14");
    	  nodoDest = Names::Find<Node>("Core15");
    	  ndnOrig = nodoOrig->GetObject<L3Protocol>();
    	  ndnDest = nodoDest->GetObject<L3Protocol>();
    	  bfOrig = nodoOrig->GetObject<bloom_filter>();
    	  bfDest = nodoDest->GetObject<bloom_filter>();
    	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(2));
    	  if (faceOrig == 0)
    	  {
             NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core14!");
    		 exit(1);
    	  }
    	  faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(2));
    	  if (faceDest == 0)
    	  {
    		 NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core15!");
    		 exit(1);
    	  }
    	  idFaceOrig = faceOrig->GetId();
    	  idFaceDest = faceDest->GetId();
    	  ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
    	  ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
     	  grOrig = nodoOrig->GetObject<GlobalRouter> ();
  	      grDest = nodoDest->GetObject<GlobalRouter> ();

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
    	      Simulator::Schedule (Seconds (10005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
    	      Simulator::Schedule (Seconds (10005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                   Simulator::Schedule (Seconds (20005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

    	      Simulator::Schedule (Seconds (30005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
    	      Simulator::Schedule (Seconds (30005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                   Simulator::Schedule (Seconds (40005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

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
  		  nodoOrig = Names::Find<Node>("Core15");
    	  nodoDest = Names::Find<Node>("Core4");
    	  ndnOrig = nodoOrig->GetObject<L3Protocol>();
    	  ndnDest = nodoDest->GetObject<L3Protocol>();
    	  bfOrig = nodoOrig->GetObject<bloom_filter>();
    	  bfDest = nodoDest->GetObject<bloom_filter>();
    	  faceOrig = DynamicCast<NetDeviceFace> (ndnOrig->GetFace(3));
    	  if (faceOrig == 0)
    	  {
    		 NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core15!");
    		 exit(1);
    	  }
    	  faceDest = DynamicCast<NetDeviceFace> (ndnDest->GetFace(0));
    	  if (faceDest == 0)
    	  {
    	     NS_LOG_UNCOND ("Errore nel determinare l'interfaccia del Core4!");
    		 exit(1);
    	  }
    	  idFaceOrig = faceOrig->GetId();
    	  idFaceDest = faceDest->GetId();
    	  ndOrig = DynamicCast<PointToPointNetDevice> (faceOrig->GetNetDevice());
    	  ndDest = DynamicCast<PointToPointNetDevice> (faceDest->GetNetDevice());
     	  grOrig = nodoOrig->GetObject<GlobalRouter> ();
  	      grDest = nodoDest->GetObject<GlobalRouter> ();

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
    	     Simulator::Schedule (Seconds (10005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
    	     Simulator::Schedule (Seconds (10005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                  Simulator::Schedule (Seconds (20005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

    	     Simulator::Schedule (Seconds (30005.00), &ns3::ndn::bloom_filter::clear_stable, bfOrig, idFaceOrig);
    	     Simulator::Schedule (Seconds (30005.10), &ns3::ndn::bloom_filter::clear_stable, bfDest, idFaceDest);

                  Simulator::Schedule (Seconds (40005.00), &ns3::ndn::bloom_filter::set_stable, bfDest, idFaceDest);

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
  	  	  NS_LOG_UNCOND("Impossibile individuare il punto di attacco dei Repository!!");
  	  	  exit(1);
  	  	  break;

}


// ***** FINE IMPLEMENTAZIONE LINK FAILURE

  Simulator::Stop (finishTime);

//  AsciiTraceHelper ascii;
//  ForwardingStrategy.EnableAsciiAll(ascii.CreateFileStream("fw.tr"));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();

//   ******  STAMPA FINALE DELLA CACHE DI CIASCUN NODO
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
InterestInviatiRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header, Ptr<const Face> face)
{
	 *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << face->GetId() << std::endl;
	 //NS_LOG_UNCOND(Simulator::Now().GetSeconds() <<  header->GetName() << face->GetId());
}

void DataInviatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, bool local, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << local << "\t" <<  face->GetId() << std::endl;
}

void DataRicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" <<  face->GetId() << std::endl;
}

void
UpdateD0D1InviatiTrace(Ptr<OutputStreamWrapper> stream, uint32_t locOrForw, uint32_t numInterf)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" << locOrForw << "\t" << numInterf << std::endl;
}

void
UpdateD0D1InviatiTrace_Old(Ptr<OutputStreamWrapper> stream)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << std::endl;
}


void
UpdateD0D1RicevutiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const Face> face)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" << face->GetId() << std::endl;
}

void
EntryExpTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
}

//void DownloadTimeTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, int64_t time, uint32_t dist)
//{
//	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << time << "\t" << dist << std::endl;
//}

//void DownloadTimeFileTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, int64_t time)
//{
//	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << time << std::endl;
//}

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

void PercD0D1Trace(Ptr<OutputStreamWrapper> stream, const std::vector<double>* componenti)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" << componenti->operator [](0) << "\t" << componenti->operator [](1) << "\t" << componenti->operator [](2) << "\t" << componenti->operator [](3) << "\t" << componenti->operator [](4) << std::endl;
}

void DataScartatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header, uint32_t dropUnsol)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << dropUnsol << std::endl;
}

//void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, Ptr<const InterestHeader> header, int64_t time)
//{
//	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << "\t" << time << std::endl;
//}

void InterestEliminatiTrace(Ptr<OutputStreamWrapper> stream, const std::string* header, int64_t time_sent)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *header << "\t" << time_sent << std::endl;
}

void InterestInviatiAppTrace(Ptr<OutputStreamWrapper> stream, const std::string* interest)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  *interest << std::endl;
}

void DataInCaheAppTrace(Ptr<OutputStreamWrapper> stream, Ptr<const ContentObjectHeader> header)
{
	*stream->GetStream() << Simulator::Now().GetNanoSeconds() << "\t" <<  header->GetName() << std::endl;
}

