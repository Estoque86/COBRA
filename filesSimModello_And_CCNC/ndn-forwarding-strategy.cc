/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
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
 * Author:  Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *          Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-forwarding-strategy.h"

#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"
#include "ns3/ndn-interest-header.h"
#include "ns3/ndn-content-object-header.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-fib.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-face.h"

#include "ns3/assert.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/string.h"


#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tuple/tuple.hpp>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>


namespace ll = boost::lambda;

NS_LOG_COMPONENT_DEFINE ("ndn.ForwardingStrategy");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ForwardingStrategy);

TypeId ForwardingStrategy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ForwardingStrategy")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutInterests",  "OutInterests",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outInterests))
    .AddTraceSource ("InInterests",   "InInterests",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inInterests))
    .AddTraceSource ("DropInterests", "DropInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterests))
    .AddTraceSource ("DropInterestFailure",  "DropInterestFailure",  MakeTraceSourceAccessor (&ForwardingStrategy::m_dropInterestFailure))
    .AddTraceSource ("AggregateInterests", "AggregateInterests", MakeTraceSourceAccessor (&ForwardingStrategy::m_aggregateInterests))
    
    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////

    .AddTraceSource ("OutData",  "OutData",  MakeTraceSourceAccessor (&ForwardingStrategy::m_outData))
    .AddTraceSource ("InData",   "InData",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inData))
    .AddTraceSource ("DataInCacheApp",   "DataInCacheApp",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inDataCacheApp))
    .AddTraceSource ("DropData", "DropData", MakeTraceSourceAccessor (&ForwardingStrategy::m_dropData))
    .AddTraceSource ("InUpdateD0","InUpdateD0",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inUpdateD0))
    .AddTraceSource ("InUpdateD1","InUpdateD1",   MakeTraceSourceAccessor (&ForwardingStrategy::m_inUpdateD1))
    .AddTraceSource ("OutUpdateD0","OutUpdateD0",   MakeTraceSourceAccessor (&ForwardingStrategy::m_outUpdateD0))
    .AddTraceSource ("OutUpdateD1","OutUpdateD1",   MakeTraceSourceAccessor (&ForwardingStrategy::m_outUpdateD1))

    ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////
    .AddTraceSource ("ExpEntry","ExpEntry",   MakeTraceSourceAccessor (&ForwardingStrategy::m_expiredEntry))

    .AddAttribute ("CacheUnsolicitedData", "Cache overheard data that have not been requested",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ForwardingStrategy::m_cacheUnsolicitedData),
                   MakeBooleanChecker ())

    .AddAttribute ("DetectRetransmissions", "If non-duplicate interest is received on the same face more than once, "
                                            "it is considered a retransmission",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ForwardingStrategy::m_detectRetransmissions),
                   MakeBooleanChecker ())

    .AddAttribute ("UpdateThreshold",
                   "Valore di soglia fissato (da 0 a 100)",
                   StringValue ("30"),
                   MakeUintegerAccessor (&ForwardingStrategy::threshold),
                   MakeUintegerChecker<uint32_t> ())

    ;
  return tid;
}

ForwardingStrategy::ForwardingStrategy ()
{
}

ForwardingStrategy::~ForwardingStrategy ()
{
}

void
ForwardingStrategy::NotifyNewAggregate ()
{
  if (m_pit == 0)
    {
      m_pit = GetObject<Pit> ();
    }
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_contentStore == 0)
    {
      m_contentStore = GetObject<ContentStore> ();
    }
  if (m_bloomFilter == 0)
      {
        m_bloomFilter = GetObject<bloom_filter> ();
      }
  if (m_repository == 0)
     {
        m_repository = GetObject<Repo> ();
     }


  Object::NotifyNewAggregate ();
  //setThreshold();
}

void
ForwardingStrategy::DoDispose ()
{
  m_pit = 0;
  m_contentStore = 0;
  m_fib = 0;
  m_bloomFilter = 0;
  m_repository = 0;

  Object::DoDispose ();
}

//  *********  FUNZIONE ONINTEREST ORIGINARIA  *********

void
ForwardingStrategy::OnInterest (const Ptr<Face> &incomingFace,
                                    Ptr<InterestHeader> &header,
                                    const Ptr<const Packet> &packet)
{
  TypeId faceType = incomingFace->GetInstanceTypeId();
  std::string faceType_name = faceType.GetName();

  if(faceType_name.compare("ns3::ndn::AppFace") != 0)   // Se l'Interest non proviene dal livllo applicativo, lo traccio come ricevuto
	  m_inInterests (header, incomingFace);

  //NS_LOG_INFO ("RICEVUTO INTEREST DA FACE:\t" << *incomingFace << "ID:\t" << incomingFace->GetId() << "\t" << incomingFace->GetInstanceTypeId().GetName() << "\n");
  //NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t Interest Ricevuto L3:\t" << header->GetName() << "\t" << Simulator::Now().GetNanoSeconds());

  // Metodo per recuperare le variabili globali settate con cmd line
  UintegerValue st;
  std::string sit = "g_simType";
  ns3::GlobalValue::GetValueByNameFailSafe(sit, st);
  uint64_t sim = st.Get();


  // **************    VERIFICO SE LO STESSO INTEREST È GIÀ STATO INOLTRATO    *******************

  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);


  if (pitEntry == 0)										  // Non è presente una entry nella PIT per il contenuto richiesto.
  {
      // ******  HO BISOGNO DI PASSARE IL TIPO DI INTERFACCIA DA CUI HO RICEVUTO L'INTEREST IN MODO DA PASSARLO ALLA FUNZIONE DI LOOKUP
	  // ******  NEI BF. Se è una "AppFace", il lookup va fatto su tutte le interfacce del nodo, altrimenti, se si tratta di una
	  // ******  "NetDeviceFace", questo andrà fatto escludendo l'interfaccia d'arrivo.

      if (sim == 3 && (incomingFace->GetNode()->GetNDevices() > 1))     // Il lookup nei BF verrà fatto solo nella rispettiva simulazione e solo se non si tratta
    	                                                              // di un nodo client (i quali hanno una sola interfaccia di rete verso cui instradare gli Interest)
      {
    	  pitEntry = m_pit->CreateType (header, incomingFace);					  // Creazione della entry PIT;
    	  //NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t PIT ENTRY CREATA:\t" << pitEntry->GetPrefix());
      }
      else
      {
    	  pitEntry = m_pit->Create (header);					       // Creazione della entry PIT;
    	  //NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t PIT ENTRY CREATA:\t" << pitEntry->GetPrefix());
      }

      if (pitEntry != 0)
        {
          DidCreatePitEntry (incomingFace, header, packet, pitEntry);
        }
      else
        {
          FailedToCreatePitEntry (incomingFace, header, packet);
          return;
        }
  }


  bool isDuplicated = true;
  if (!pitEntry->IsNonceSeen (header->GetNonce ()))         // Tramite il Nounce, verifica se l'Interest ricevuto è un duplicato.
  {
    pitEntry->AddSeenNonce (header->GetNonce ());			// Se nn lo è, aggiunge il nounce alla lista degli Interest ricevuti
    isDuplicated = false;
  }

  if (isDuplicated) 
  {
    DidReceiveDuplicateInterest (incomingFace, header, packet, pitEntry);    // Se l'Interest è un duplicato, esco dalla funzione senza processarlo.
    return;
  }

  // Verificata la validità dell'Interest e creata la entry PIT, procede alla verifica della presenza del contenuto richiesto nella cache.

  // ****************  VERIFICO SE IL CONTENUTO RICHIESTO È GIÀ IN CACHE  *************************

  Ptr<Packet> contentObject;
  Ptr<const ContentObjectHeader> contentObjectHeader; // used for tracing
  Ptr<const Packet> payload; // used for tracing
  Ptr<ContentObjectHeader> header_repo;

  bool repo = false;
  std::string line;
  std::stringstream ss;
  // Recupero il nome del contenuto richiesto
  ss << header -> GetName();
  std::string interest_ric = ss.str();
  ss.str("");

  uint32_t rp_size = m_repository->GetMaxSize();
  if(rp_size!=0)   // E' un nodo REPO, quindi prendo il nome dall'header, tolgo la parte del chunk, faccio il lookup.
  {
        repo = true;       
        //NS_LOG_UNCOND("REPOSITORY:\t" << incomingFace->GetNode()->GetId() << "\tRicevuto Interest" << "\t" << header->GetName() <<  "\t" << Simulator::Now().GetNanoSeconds());

        // Tolgo la componente del chunk
        Ptr<NameComponents> receivedContent = Create<NameComponents> (interest_ric);
        uint32_t numComponents = receivedContent->GetComponents().size();
  
        std::list<std::string> nome_int_ric (numComponents);
        nome_int_ric.assign(receivedContent->begin(), receivedContent->end());
        std::list<std::string>::iterator it = nome_int_ric.begin(); 		// L'iterator punta alla prima componente
        nome_int_ric.pop_back();
        --numComponents;
	
        for(uint32_t t = 0; t < numComponents; t++)
        {
	    ss << "/" << *it;
	    it++;
        }
        std::string interestNoChunk = ss.str();
        ss.str("");
        
        nome_int_ric.clear();

        Ptr<InterestHeader> header_new = Create<InterestHeader>();
	header_new->SetName(Create<NameComponents> (interestNoChunk));

       //NS_LOG_UNCOND("REPOSITORY:\t" << incomingFace->GetNode()->GetId() << "\tInterest senza chunk:\t" << interestNoChunk <<  "\t" << Simulator::Now().GetNanoSeconds());

        //  **** Lookup nel repository
        boost::tie (contentObject, contentObjectHeader, payload) = m_repository->Lookup (header_new);
       
        if(contentObject != 0)   // Il REPO ha il contenuto senza chunk, quindi devo il contenuto con il nome comprensivo di chunk
        {
              header_repo = Create<ContentObjectHeader> ();
              contentObject->RemoveHeader (*header_repo);
              header_repo->SetName(Create<NameComponents>(interest_ric));
              contentObject->AddHeader(*header_repo);

           
              /*Ptr<Packet> contentObject_R = Create<Packet> (1024);
              static ContentObjectTail tail;
  	      Ptr<ContentObjectHeader> contentObjectHeader_R = Create<ContentObjectHeader> ();
  	      contentObjectHeader_R->SetName (Create<NameComponents>(interest_ric));
  	      contentObjectHeader_R->GetSignedInfo ().SetTimestamp (Simulator::Now ());
  	      contentObjectHeader_R->GetSignature ().SetSignatureBits (0);

  	      contentObject_R->AddHeader (*contentObjectHeader_R);
  	      contentObject_R->AddTrailer (tail);

              boost::tie (contentObject, contentObjectHeader, payload) = boost::tuple<Ptr<Packet>, Ptr<ContentObjectHeader>, Ptr<Packet> > (contentObject_R, contentObjectHeader_R, 0);*/
      	   
       }       
  
  }
  else    // NON È UN NODO REPO, QUINDI DEVO RICERCARE IL CONTENUTO CON IL SUO NOME INTERO
  {
        boost::tie (contentObject, contentObjectHeader, payload) = m_contentStore->Lookup (header);
  }


/*
  uint32_t cs_size = m_contentStore->GetSize();       // I nodi con CACHE, hanno un REPO = 0; i nodi con REPO, hanno una CACHE = 0;
  if(cs_size!=0)
  {
	  boost::tie (contentObject, contentObjectHeader, payload) = m_contentStore->Lookup (header);
  }
  else
  {
	  repo = true;
	  boost::tie (contentObject, contentObjectHeader, payload) = m_repository->Lookup (header);
  }
*/
  if (contentObject != 0)                                       //  IL CONTENUTO RICHIESTO È PRESENTE NELLA CACHE LOCALE
    {
      NS_ASSERT (contentObjectHeader != 0);

      // L'INTEREST VIENE SODDISFATTO LOCALMENTE
      //contentObjectHeader->GetSignedInfo().SetHopCount(0);       // Il nodo che genera il data, inizializza l'hop count ad 0. Il nodo che lo riceve,
      	  	  	  	  	  	  	  	  	  	  	  	  	  	  	 // incrementerà l'hop count di 1.
      	  	  	  	  	  	  	  	  	  	  	  	  	  	  	 
      //NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t CO TROVATO IN CACHE O REPO:\t" << contentObjectHeader->GetName());

      Ptr<ContentObjectHeader> header_cont = Create<ContentObjectHeader> ();
      contentObject->RemoveHeader (*header_cont);
      header_cont->GetSignedInfo().SetHopCount(0);
      contentObject->AddHeader(*header_cont);

      //      uint32_t type = 2;
//      contentObjectHeader->GetSignedInfo().SetUpdType(type);
      //pitEntry->AddIncoming (incomingFace, Seconds (1.0));    // Aggiungo l'interfaccia d'arrivo dell'Interest all'entry PIT creata precedentemente.
      pitEntry->AddIncoming (incomingFace);

      // Do data plane performance measurements
      //WillSatisfyPendingInterest (0, pitEntry);                 // l'Incoming face è '0' perchè l'interest verrà soddisfatto dalla cache locale.

      // Actually satisfy pending interest
      if(!repo) {
    	  NS_LOG_FUNCTION("Contenuto in CACHE; soddisfo l'Interest:" << "\t" << header->GetName());
		  SatisfyPendingInterest (0, contentObjectHeader, payload, contentObject, pitEntry);   // Soddisfo l'Interest ricevuto ed esco dalla funzione.
		  }
      else {
    	  //NS_LOG_UNCOND("Contenuto creato dal REPOSITORY:" << "\t" << contentObjectHeader->GetName());
          SatisfyPendingInterest (0, header_repo, payload, contentObject, pitEntry);   // Soddisfo l'Interest ricevuto ed esco dalla funzione.
	      }
      return;
    }

  // Se il contenuto richiesto nn è presente nè nella cache nè nel repository, verifico se si tratta di un Interest nuovo o ritrasmesso (la verifica sul duplicato è stata fatta in
  // precedenza).

  if (ShouldSuppressIncomingInterest (incomingFace, pitEntry))
    {
      //pitEntry->AddIncoming (incomingFace, header->GetInterestLifetime ());
      pitEntry->AddIncoming (incomingFace);

      // update PIT entry lifetime
      pitEntry->UpdateLifetime (header->GetInterestLifetime ());

      // Suppress this interest if we're still expecting data from some other face
      NS_LOG_DEBUG ("Suppress interests");
      m_aggregateInterests (header, incomingFace);                // *** VERIFICARE MEGLIO!!
      return;
    }

  // Se nn è un duplicato, non ho il contenuto in cache, o comunque nn ho eliminato l'Interest, lo inoltro secondo la forwarding strategy scelta.

  //header->GetName().Add(seq_num);
  if (sim == 3 && (incomingFace->GetNode()->GetNDevices() > 1))
	  PropagateInterest_NEW (incomingFace, header, packet, pitEntry);
  else
      PropagateInterest (incomingFace, header, packet, pitEntry);
}


void
ForwardingStrategy::OnData (const Ptr<Face> &incomingFace,
                                Ptr<ContentObjectHeader> &header,
                                Ptr<Packet> &payload,
                                const Ptr<const Packet> &packet)
{
  NS_LOG_FUNCTION (incomingFace << header->GetName () << payload << packet);
  //m_inData (header, incomingFace);
  
  //NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t DATA RICEVUTO L3:\t" << header->GetName() << "\t" << Simulator::Now().GetNanoSeconds());
  
  // Metodo per recuperare le variabili globali settate con cmd line
  UintegerValue st;
  std::string sit = "g_simType";
  ns3::GlobalValue::GetValueByNameFailSafe(sit, st);
  uint64_t sim = st.Get();

  UintegerValue rn;
  std::string r_n = "g_numeroSIM";    // Vado ad identificare il run.
  ns3::GlobalValue::GetValueByNameFailSafe(r_n, rn);
  uint64_t run_num = rn.Get();


  // Lookup PIT entry
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
  if (pitEntry == 0)
    {
      DidReceiveUnsolicitedData (incomingFace, header, payload);
      return;
    }
  else
    {
        m_inData (header, incomingFace);
      // ***** IMPLEMENTAZIONE DEL LCD (Leave Copy Down)
/// ***** DISABILITO IL CACHING *****
//	  uint32_t receivedHC = header->GetSignedInfo().GetHopCount();

	  // **** Solo se l'hop count è pari ad 1, il contenuto verrà memorizzato in cache.

	 // if (receivedHC == 1)
		m_contentStore->Add (header, payload);     // Add or update entry in the content store

	//NS_LOG_UNCOND ("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\t PIT ENTRY TROVATA ALLA RICEZIONE DEL CO\t" << "\" " << header->GetName() << "\" :\t" << pitEntry->GetPrefix());
//      Incremento l'hop count di 1
//       uint32_t receivedHC = header->GetSignedInfo().GetHopCount();
//       uint32_t newHC = receivedHC + 1;
//       Ptr<ContentObjectHeader> newHeader = Create<ContentObjectHeader> ();
//       packet->RemoveHeader (*newHeader);
//       newHeader->GetSignedInfo().SetHopCount(newHC);
//       packet->AddHeader(*newHeader);
      while (pitEntry != 0)
      {
          // Do data plane performance measurements
          WillSatisfyPendingInterest (incomingFace, pitEntry);

          // Actually satisfy pending interest
          SatisfyPendingInterest (incomingFace, header, payload, packet, pitEntry);

          // Lookup another PIT entry
          pitEntry = m_pit->Lookup (*header);
      }

// *** Nelle interfacce che collegano il nodo corrente con il repository, evito la procedure di inserimento delle info per non 
// *** compromettere le info inserite all'inizio.
bool evita = false;
uint32_t nodo_ID = incomingFace->GetNode()->GetId();
uint32_t face_ID = incomingFace->GetId();
if(sim==3)
{
  if(run_num == 2)
  {
     if(nodo_ID == 0)
     {
        if(face_ID == 2 || face_ID == 3 || face_ID == 4)
           evita = true;
     }
  }
  else if(run_num == 3)
  {
     if(nodo_ID == 13)
     {
        if(face_ID == 2 || face_ID == 3)
           evita = true;
     }
  }
  else if(run_num == 5)
  {
     if(nodo_ID == 14)
     {
        if(face_ID == 6 || face_ID == 7 || face_ID == 8)
           evita = true;
     }
  }
  else if(run_num == 6)
  {
     if(nodo_ID == 15)
     {
        if(face_ID == 5 || face_ID == 6)
           evita = true;
     }
  }      
      
}

 	  // ******  MODIFICHE PER AGGIUNGERE LE INFO DEL CONTENUTO RICEVUTO NEL CORRISPONDENTE SBF DELL'INTERFACCIA
 	  if(sim == 3 && !evita)
 	  {
 		  uint32_t num_comp_nome = header->GetNamePtr()->GetComponents().size();
 		  bool prima = true;
 		  std::list<std::string> nome_co (num_comp_nome);
 		  nome_co.assign(header->GetNamePtr()->GetComponents().begin(), header->GetNamePtr()->GetComponents().end());
 		  std::list<std::string>::iterator it;
 		  it = nome_co.begin();           			// L'iterator punta alla prima componente
 		  std::stringstream ss;                 	// Stringstream per comporre volte per volta il nome
 		  std::string nome_composto;
 		  
 		  uint32_t num_inc_face = incomingFace->GetId();
 		  for(uint32_t t = num_comp_nome; t > 0 ; t--)
 		  {
 		     	if(!prima)
 		    		nome_co.pop_back();

 		    	for(uint32_t d=1; d <=t; d++ )
 		    	{
 		    		ss << "/" << (*it);
 		    		it++;
 		    	}
 		    	nome_composto = ss.str();
 		    	//NS_LOG_UNCOND ("NODONODO " << incomingFace->GetNode()->GetId() << ":\t Nome DATA INSERITO:\t" << nome_composto);
 		    	prima = false;
 		    	it = nome_co.begin();
 		    	ss.str("");
 		    	m_bloomFilter-> insert_stable(nome_composto,num_inc_face);
 		  }
	  }


////  **********   ADESSO GLI UPDATE SONO STATI DISATTIVATI   **********   //////

// 	  if(sim == 3)
//      {
//    	  uint32_t sender = incomingFace->GetNode()->GetId();      // SENDER di un eventuale update generato.
//
//    	  // **********  Aggiornamento del contatore per l'update e verifica di superamento della soglia
//
//    	  thr_counter = thr_counter + 1;
//    	  uint32_t intCounter = 0;   // Contatore interfacce inoltro update
//    	  NS_LOG_DEBUG ("CONTATORE:\t" << thr_counter << "\t SOGLIA:\t:" << thr_check);
//    	  //if (thr_counter >= thr_check)
//    	  if (thr_counter >= thr_check)
//    	  {
//    		  NS_LOG_DEBUG ("**** GENERAZIONE UPDATE NODO:\t" << sender);
//
//    		  // ********* Tracing Updates Generati ******
//    		 // m_outUpdateD0();
//    		 // m_outUpdateD1();
//
//    		  // *********  Calcolo Updates
//    		  m_bloomFilter->makeUpdate();
//
//
//    		  std::vector<double> *percComp_D0 = new std::vector<double> (m_bloomFilter->bf_cs_.size(), 0);
//    		  std::vector<double> *percComp_D1 = new std::vector<double> (m_bloomFilter->bf_cs_.size(), 0);
//
//    		  for (std::size_t i=0; i < bf_cs_.size(); i++)
//    		  {
//    		  	percComp_D0->operator [](i) = (double)(m_bloomFilter->table_size_comp_ - (unsigned long long int)m_bloomFilter->bf_upd_dirty_zeros_[i].count()) / (double)(m_bloomFilter->table_size_comp_);
//    		   	percComp_D1->operator [](i) = (double)(m_bloomFilter->table_size_comp_ - (unsigned long long int)m_bloomFilter->bf_upd_dirty_ones_[i].count()) / (double)(m_bloomFilter->table_size_comp_);
//    		  }
//    		  m_percD0(percComp_D0);
//    		  m_percD1(percComp_D1);
//
//
//
//    		  // *********  Creazione ed Invio dei messaggi di Update
//    		  static ContentObjectTail tail_0;
//    		  static ContentObjectTail tail_1;
//    		  Ptr<ContentObjectHeader> header_0 = Create<ContentObjectHeader> ();
//    		  Ptr<ContentObjectHeader> header_1 = Create<ContentObjectHeader> ();
//
//    		  Ptr<NameComponents> dirtyZero = Create<NameComponents>("/update_0");
//    		  Ptr<NameComponents> dirtyOnes = Create<NameComponents>("/update_1");
//
//    		  header_0->SetName (dirtyZero);
//    		  header_1->SetName (dirtyOnes);
//
//    		  header_0->GetSignedInfo ().SetTimestamp (Simulator::Now ());
//    		  header_1->GetSignedInfo ().SetTimestamp (Simulator::Now ());
//
//    		  header_0->GetSignature ().SetSignatureBits (0);
//    		  header_1->GetSignature ().SetSignatureBits (0);
//
//    		  uint32_t type_0 = 10;                              // "10" è il tipo che viene usato per inviare un update di tipo "Dirty Zero".
//    		  uint32_t type_1 = 20;
//
//    		  header_0->GetSignedInfo().SetUpdType(type_0);
//    		  header_1->GetSignedInfo().SetUpdType(type_1);
//
//    		  //NS_LOG_INFO ("Tipo di Update: \t" << header->GetSignedInfo().GetUpdType());
//    		  header_0->GetSignedInfo().SetSenderID(sender);
//    		  header_1->GetSignedInfo().SetSenderID(sender);
//    		  //NS_LOG_INFO ("Nodo che genera l'update: \t" << sender);
//    		  //  NS_LOG_INFO ("node("<< GetNode()->GetId() <<") responding with ContentObject:\n" << boost::cref(*header));
//
//    	  	  //  **** Per adesso inseriamo una dimensione fittizia del pacchetto, in modo da rientrare nei 50 kB consentiti. In realtà, considerando un dimensionamento
//    	  	  //       con 26000 item e una Pfp = 5 %, ogni micro BF da inviare dovrebbe essere di 19.8 kB.
 	  	  	  //  Considerando la compressione, però, unita alla percentuale misurata di zeri negli update, ogni singolo microBF avrebbe una dimensione di 1,6kB.
 	          //  Quindi ogno update sarebbe 5 * 1,6 = 8kB
//    		  // Ptr<Packet> packet_0 = Create<Packet> ((m_bloomFilter->raw_table_size_comp_)*(m_bloomFilter->numComp_));
//    		  // Ptr<Packet> packet_0 = Create<Packet> (45000);
 	          // Ptr<Packet> packet_0 = Create<Packet> (8000);
//    		  packet_0->AddHeader (*header_0);
//    		  packet_0->AddTrailer (tail_0);
//
//    		  // Ptr<Packet> packet_1 = Create<Packet> ((m_bloomFilter->raw_table_size_comp_)*(m_bloomFilter->numComp_));
//    		  Ptr<Packet> packet_1 = Create<Packet> (8000);
//    		  packet_1->AddHeader (*header_1);
//    		  packet_1->AddTrailer (tail_1);
//
//    		  Ptr<pit::Entry> pitEntry_0 = m_pit->Lookup (*header_0);
//    		  if (pitEntry_0 == 0)
//    		  {
//    			  DidReceiveUnsolicitedData (incomingFace, header_0, packet_0);
//    			  NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
//    			  return;
//    		  }
//    		  else
//    		  {
//    			  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry_0->GetIncoming ())
//    			  {
//    				  // Invio dei DIRTY ZERO
//    				  bool ok_0 = incoming.m_face->Send (packet_0->Copy ());
//    				  if (ok_0)
//    				  {
//    					  //m_outUpdD0 (header_0, packet_0, incoming.m_face);
//    					  //DidSendOutData (incoming.m_face, header_0, packet_0, packet_0);
//    					  intCounter++;
//    					  NS_LOG_DEBUG ("DIRTY ZERO UPDATE FORWARDED TO:\t " << *incoming.m_face);
//    				  }
//    				  else
//    				  {
//    					  //m_dropData (header_0, packet_0, incoming.m_face);
//    					  NS_LOG_DEBUG ("CANNOT FORWARD THE DIRTY ZERO UPDATE TO:\t" << *incoming.m_face);
//    				  }
//
//    				  // Invio dei DIRTY ONES
//    				  bool ok_1 = incoming.m_face->Send (packet_1->Copy ());
//    				  if (ok_1)
//    				  {
//    					  //m_outUpdD1 (header_1, packet_1, incoming.m_face);
//    					  //DidSendOutData (incoming.m_face, header_1, packet_1, packet_1);
//
//    					  NS_LOG_DEBUG ("DIRTY ONE UPDATE FORWARDED TO:\t " << *incoming.m_face);
//    				  }
//    				  else
//    				  {
//    					  //m_dropData (header_1, packet_1, incoming.m_face);
//    					  NS_LOG_DEBUG ("CANNOT FORWARD THE DIRTY ONE UPDATE TO:\t" << *incoming.m_face);
//    				  }
//
//    			  }
//    	    	  m_outUpdateD0((uint32_t)1, intCounter);
//    	    	  m_outUpdateD1((uint32_t)1, intCounter);
//    		  }
//
//    		  // Resetto il Contatore per la soglia.
//    		  thr_counter = 0;
//    	 }
//
//      }
    }

}

// ***** FUNZIONE PER IL TRATTAMENTO DI DI UN DIRTY ZERO UPDATE
void
ForwardingStrategy::OnUpdDirtyZero (const Ptr<Face> &incomingFace,
                                Ptr<ContentObjectHeader> &header,
                                Ptr<Packet> &payload,
                                const Ptr<const Packet> &packet)
{
  NS_LOG_FUNCTION (incomingFace << header->GetName () << payload << packet);

  bool already_received = false;

  // Verifico la tipologia dell'interfaccia di arrivo dell'update (App o NetDevice); se è una AppFace si tratta di un update generato localmente,
  // quindi va trasmesso verso tutte le interfacce

  TypeId faceType = incomingFace->GetInstanceTypeId();
  std::string faceType_name = faceType.GetName();

  // ***********  L'UPDATE MI ARRIVA DALLA AppFACE !! LO DEVO INOLTRARE VERSO TUTTI I NETDEVICE!! ****************

  if(faceType_name.compare("ns3::ndn::AppFace") == 0)                // l'UPDATE è giunto dal livello applicativo quindi va inoltrato a tutte le interfacce
  {
	  // L'aggiornamento dalla AppFace arriva solo allo startup dei nodi;

	  //m_outUpdateD0();
	  uint32_t intCounter = 0;
	  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
	  if (pitEntry == 0)
	  {
	    DidReceiveUnsolicitedData (incomingFace, header, payload);
	    NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
	    return;
	  }
	  else
	  {
		  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
		  {
			  bool ok = incoming.m_face->Send (packet->Copy ());
			  if (ok)
			  {
				  intCounter++;
			     //m_outData (header, payload, incomingFace == 0, incoming.m_face);
			     DidSendOutData (incoming.m_face, header, payload, packet);

			  	 NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
			  }
			  else
			  {
			  	  //m_dropData (header, payload, incoming.m_face);
			      NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
			  }

		  }
		  m_outUpdateD0((uint32_t)1, intCounter);
	  }

  }

  // *********** L'UPDATE MI ARRIVA DA UN NETDEVICE !! DEVO CONTROLLARE SE SI TRATTA DI UN DUPLICATO ********

  else
  {

  // **** CHECK ULTERIORE PER EVITARE CHE L'UPDATE VENGA PROCESSATO DALLO STESSO NODO CHE LO HA GENERATO !! ******
  if(header->GetSignedInfo().GetSenderID() != incomingFace->GetNode()->GetId())   // Verifico che l'update non ritorni al nodo che l'ha generato
  {
	  m_inUpdateD0 (incomingFace);

	  // Lookup PIT entry; Ci sarà una PIT entry di default in tutti i nodi per i messaggi di update (tipo /update_0 e /update_1)
	  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
	  if (pitEntry == 0)
	  {
		  DidReceiveUnsolicitedData (incomingFace, header, payload);
		  NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
		  return;
	  }

	  else
	  {
		  uint32_t num_inc_face = incomingFace->GetId();
		  Time updTimestamp = header->GetSignedInfo().GetTimestamp();
		  uint32_t senderID = header->GetSignedInfo().GetSenderID();

		  std::map<uint32_t, Time>::iterator it = m_bloomFilter->tab_upd_dirty_zero_[num_inc_face]->find(senderID);

		  // **** CASO: NON HO MAI RICEVUTO QUESTO UPDATE SULL'INTERFACCIA CONSIDERATA *****

		  if(it == m_bloomFilter->tab_upd_dirty_zero_[num_inc_face]->end())
		  {
			  // Non ho trovato nessun update già ricevuto da quel nodo e da quella interfaccia.

			  // **** INSERISCO LE INFO NELLA TABELLA RELATIVA ********
			  m_bloomFilter->tab_upd_dirty_zero_[num_inc_face]->insert(std::pair<uint32_t, Time>(senderID,updTimestamp));

			  // Funzione per aggiornare il counting BF dell'interfaccia di arrivo dell'update. Passo tale interfaccia e l'id del nodo che ha inviato l'update
			  // in maniera tale da recuperarlo (ricordiamo che il payload è virtuale)

			  // **** AGGIORNO IL COUNTING BF DELL'INTERFACCIA CONSIDERATA ******
			  m_bloomFilter->update_dirtyZero(num_inc_face, senderID);


			  // ***** ADESSO DEVO CONTROLLARE CHE L'UPDATE NON SIA STATO RICEVUTO DA NESSUNA DELLE INTERFACCE, IN QUANTO, SE COSÌ NON FOSSE, NON AVREBBE
			  // ***** SENSO INVIARLO NUOVAMENTE.

			  //bool already_received = false;
			  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
			  {
				  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
				  {
					  continue;                                       // Le operazioni sUll'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
			                   //already_received = true;
				  }
				  else
				  {

				    it = m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->find(senderID);
     		       	if(it!=m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->end())
			  		{

     		       		// Ho trovato un'interfaccia che ha ricevuto un update da quel nodo. Adesso, tramite il timestamp, devo verificare se si tratta
     		       		// di un duplicato o di un effettivo aggiornamento
     		       		if(it->second >= updTimestamp)
     		       		{
     		       			// Vuol dire che la traccia nella tabella si riferisce allo stesso update.
     		       			already_received = true;
     		       			break;
     		       		}
			  		}
				  }
			  }


			  if(!already_received)
			  {
				  uint32_t intCounter = 0;
				  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
				  {
					  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
					  {
					 	  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
					                      //already_received = true;
					  }

					  bool ok = incoming.m_face->Send (packet->Copy ());
				      if (ok)
				      {
				         //m_outData (header, payload, incomingFace == 0, incoming.m_face);
				    	  intCounter++;
				         DidSendOutData (incoming.m_face, header, payload, packet);
    	  			     NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
				      }
				  	  else
				  	  {
				  	     //m_dropData (header, payload, incoming.m_face);
				  	     NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
				  	  }

				  }
				  m_outUpdateD0((uint32_t)0, intCounter);
			  }
			  else
			  {
				  NS_LOG_DEBUG ("UPDATE ALREADY RECEIVED!");
			  }
		  }


     	  else  // Nell'interfaccia d'arrivo ho già un update di quel nodo
		  {
			  if(it->second < updTimestamp)
			  {
				  // Vuol dire che la traccia nella tabella si riferisce ad un update vecchio. Quindi aggiorno BF dell'interfaccia d'arrivo e inoltro
				  // l'update verso le interfaccie che non lo hanno ancora ricevuto

				  // Funzione per aggiornare il counting BF dell'interfaccia di arrivo dell'update. Passo tale interfaccia e l'id del nodo che ha inviato l'update
			  	  // in maniera tale da recuperarlo (ricordiamo che il payload è virtuale)
			  	  // **** AGGIORNO IL COUNTING BF DELL'INTERFACCIA CONSIDERATA ******
			  	  m_bloomFilter->update_dirtyZero(num_inc_face, senderID);

				  it->second = updTimestamp;

				  //bool already_received = false;
				  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
				  {
				     if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
				     {
				    	 continue;                                       // Le operazioni sUll'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
				  			                   //already_received = true;
				     }
				     else
				     {
				    	 it = m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->find(senderID);
				    	 if(it!=m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->end())
				    	 {

				  			// Ho trovato un'interfaccia che ha ricevuto un update da quel nodo. Adesso, tramite il timestamp, devo verificare se si tratta
				  	 		// di un duplicato o di un effettivo aggiornamento
				    		 if(it->second >= updTimestamp)
				    		 {
				    			 // Vuol dire che la traccia nella tabella si riferisce allo stesso update.
				    			 already_received = true;
				    			 break;
				    		 }
				    	 }
				     }
				  }



			  	  if(!already_received)
				  {

				  	  // ***** ADESSO DEVO CONTROLLARE CHE L'UPDATE NON SIA STATO RICEVUTO DA NESSUNA DELLE INTERFACCE, IN QUANTO, SE COSÌ NON FOSSE, NON AVREBBE
				  	  // ***** SENSO INVIARLO NUOVAMENTE.

			  		  uint32_t intCounter = 0;
				  	  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
				  	  {
				  		  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
				  		  {
				  		 	  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
				  				                      //already_received = true;
				  		  }

				  		  bool ok = incoming.m_face->Send (packet->Copy ());
				  	      if (ok)
				  	      {
				  	         //m_outData (header, payload, incomingFace == 0, incoming.m_face);
				  	         intCounter++;
				  	    	  DidSendOutData (incoming.m_face, header, payload, packet);
				      	     NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
				  		  }
				  		  else
				  		  {
				  		     //m_dropData (header, payload, incoming.m_face);
				  			 NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
				  		  }

				  	   }
				  	  m_outUpdateD0((uint32_t)0, intCounter);
				  }
				  else
				  {
				  	  NS_LOG_DEBUG ("UPDATE ALREADY RECEIVED!");
				  }
			  }
			  else
			  {
				  NS_LOG_DEBUG("UPDATE ALREADY RECEIVED ON INTERFACE:\t" << incomingFace->GetId());
			  }
		  }
	  }
  }

  else   //UPDATE RICEVUTO DALLO STESSO NODO CHE LO HA GENERATO.
  {
	  NS_LOG_DEBUG("UPDATE RITORNATO ALL'ORIGINE!!");
	  return;
  }
 }
}


//// ***** FUNZIONE PER IL TRATTAMENTO DI DI UN DIRTY ZERO UPDATE
//void
//ForwardingStrategy::OnUpdDirtyZero (const Ptr<Face> &incomingFace,
//                                Ptr<ContentObjectHeader> &header,
//                                Ptr<Packet> &payload,
//                                const Ptr<const Packet> &packet)
//{
//  NS_LOG_FUNCTION (incomingFace << header->GetName () << payload << packet);
//
//  // Verifico la tipologia dell'interfaccia di arrivo dell'update (App o NetDevice); se è una AppFace si tratta di un update generato localmente,
//  // quindi va trasmesso verso tutte le interfacce
//
//  TypeId faceType = incomingFace->GetInstanceTypeId();
//  std::string faceType_name = faceType.GetName();
//
//  // ***********  L'UPDATE MI ARRIVA DALLA AppFACE !! LO DEVO INOLTRARE VERSO TUTTI I NETDEVICE!! ****************
//
//  if(faceType_name.compare("ns3::ndn::AppFace") == 0)                // l'UPDATE è giunto dal livello applicativo quindi va inoltrato a tutte le interfacce
//  {
//	  // L'aggiornamento dalla AppFace arriva solo allo startup dei nodi;
//
//	  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
//	  if (pitEntry == 0)
//	  {
//	    DidReceiveUnsolicitedData (incomingFace, header, payload);
//	    NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
//	    return;
//	  }
//	  else
//	  {
//		  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
//		  {
//			  bool ok = incoming.m_face->Send (packet->Copy ());
//			  if (ok)
//			  {
//			     m_outData (header, payload, incomingFace == 0, incoming.m_face);
//			     DidSendOutData (incoming.m_face, header, payload, packet);
//
//			  	 NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
//			  }
//			  else
//			  {
//			  	  m_dropData (header, payload, incoming.m_face);
//			      NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
//			  }
//
//		  }
//	  }
//
//  }
//
//  // *********** L'UPDATE MI ARRIVA DA UN NETDEVICE !! DEVO CONTROLLARE SE SI TRATTA DI UN DUPLICATO ********
//
//  else
//  {
//
//  // **** CHECK ULTERIORE PER EVITARE CHE L'UPDATE VENGA PROCESSATO DALLO STESSO NODO CHE LO HA GENERATO !! ******
//  if(header->GetSignedInfo().GetSenderID() != incomingFace->GetNode()->GetId())   // Verifico che l'update non ritorni al nodo che l'ha generato
//  {
//	  m_inUpdD0 (header, payload, incomingFace);
//
//	  // Lookup PIT entry; Ci sarà una PIT entry di default in tutti i nodi per i messaggi di update (tipo /update_0 e /update_1)
//	  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
//	  if (pitEntry == 0)
//	  {
//		  DidReceiveUnsolicitedData (incomingFace, header, payload);
//		  NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
//		  return;
//	  }
//
//	  else
//	  {
//		  uint32_t num_inc_face = incomingFace->GetId();
//		  Time updTimestamp = header->GetSignedInfo().GetTimestamp();
//		  uint32_t senderID = header->GetSignedInfo().GetSenderID();
//
//		  std::map<uint32_t, Time>::iterator it = m_bloomFilter->tab_upd_dirty_zero_[num_inc_face]->find(senderID);
//
//		  // **** CASO: NON HO MAI RICEVUTO QUESTO UPDATE SULL'INTERFACCIA CONSIDERATA *****
//
//		  if(it == m_bloomFilter->tab_upd_dirty_zero_[num_inc_face]->end())
//		  {
//			  // Non ho trovato nessun update già ricevuto da quel nodo e da quella interfaccia. Quindi faccio le operazioni necessarie
//			  // di aggiornamento del BF, aggiungo l'update nella tabella e poi inoltro l'update.
//
//			  // **** INSERISCO LE INFO NELLA TABELLA RELATIVA ********
//			  m_bloomFilter->tab_upd_dirty_zero_[num_inc_face]->insert(std::pair<uint32_t, Time>(senderID,updTimestamp));
//
//			  // Funzione per aggiornare il counting BF dell'interfaccia di arrivo dell'update. Passo tale interfaccia e l'id del nodo che ha inviato l'update
//			  // in maniera tale da recuperarlo (ricordiamo che il payload è virtuale)
//
//			  // **** AGGIORNO IL COUNTING BF DELL'INTERFACCIA CONSIDERATA ******
//			  m_bloomFilter->update_dirtyZero(num_inc_face, senderID);
//
//			  // ***** ADESSO DEVO CONTROLLARE CHE L'UPDATE NON SIA STATO RICEVUTO DA NESSUNA DELLE INTERFACCE, IN QUANTO, SE COSÌ NON FOSSE, NON AVREBBE
//			  // ***** SENSO INVIARLO NUOVAMENTE.
//
//
//			  bool already_received = false;
//			  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
//			  {
//				  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
//				  {
//					  continue;                                       // Le operazioni sUll'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
//                      //already_received = true;
//				  }
//				  else
//				  {
//
//					  it = m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->find(senderID);
//
//					  if(it!=m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->end())
//					  {
//
//						  // Ho trovato un'interfaccia che ha ricevuto un update da quel nodo. Adesso, tramite il timestamp, devo verificare se si tratta
//						  // di un duplicato o di un effettivo aggiornamento
//						  if(it->second >= updTimestamp)
//						  {
//							  // Vuol dire che la traccia nella tabella si riferisce allo stesso update.
//							  already_received = true;
//						  }
//					  }
//				  }
//			  }
//
//			  if(!already_received)
//			  {
//				  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
//				  {
//					  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
//					  {
//					 	  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
//					                      //already_received = true;
//					  }
//
//					  bool ok = incoming.m_face->Send (packet->Copy ());
//				      if (ok)
//				      {
//				         m_outData (header, payload, incomingFace == 0, incoming.m_face);
//				         DidSendOutData (incoming.m_face, header, payload, packet);
//    	  			     NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
//				      }
//				  	  else
//				  	  {
//				  	     m_dropData (header, payload, incoming.m_face);
//				  	     NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
//				  	  }
//
//				  }
//			  }
//			  else
//			  {
//				  NS_LOG_DEBUG ("UPDATE ALREADY RECEIVED!");
//			  }
//		  }
//
//
//     	  else  // Nell'interfaccia d'arrivo ho già un update di quel nodo
//		  {
//			  if(it->second < updTimestamp)
//			  {
//				  // Vuol dire che la traccia nella tabella si riferisce ad un update vecchio. Quindi aggiorno BF dell'interfaccia d'arrivo e inoltro
//				  // l'update verso le interfaccie che non lo hanno ancora ricevuto
//
//				  m_bloomFilter->update_dirtyZero(num_inc_face, senderID);
//				  it->second = updTimestamp;
//
//				  bool already_received = false;
//				  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
//				  {
//					  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
//					  {
//						  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
//	                      //already_received = true;
//					  }
//					  else
//					  {
//
//						  it = m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->find(senderID);
//
//						  if(it!=m_bloomFilter->tab_upd_dirty_zero_[incoming.m_face->GetId()]->end())
//						  {
//
//							  // Ho trovato un'interfaccia che ha ricevuto un update da quel nodo. Adesso, tramite il timestamp, devo verificare se si tratta
//							  // di un duplicato o di un effettivo aggiornamento
//							  if(it->second >= updTimestamp)
//							  {
//								  // Vuol dire che la traccia nella tabella si riferisce allo stesso update.
//								  already_received = true;
//							  }
//						  }
//					  }
//				  }
//
//				  if(!already_received)
//				  {
//					  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
//					  {
//						  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
//						  {
//						 	  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
//						                      //already_received = true;
//						  }
//
//						  bool ok = incoming.m_face->Send (packet->Copy ());
//					      if (ok)
//					      {
//					         m_outData (header, payload, incomingFace == 0, incoming.m_face);
//					         DidSendOutData (incoming.m_face, header, payload, packet);
//	    	  			     NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
//					      }
//					  	  else
//					  	  {
//					  	     m_dropData (header, payload, incoming.m_face);
//					  	     NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
//					  	  }
//
//					  }
//				  }
//				  else
//				  {
//					  NS_LOG_DEBUG ("UPDATE ALREADY RECEIVED!");
//				  }
//			  }
//			  else
//			  {
//				  NS_LOG_DEBUG("UPDATE ALREADY RECEIVED ON INTERFACE:\t" << incomingFace->GetId());
//			  }
//		  }
//	  }
//  }
//
//  else   //UPDATE RICEVUTO DALLO STESSO NODO CHE LO HA GENERATO.
//  {
//	  NS_LOG_DEBUG("UPDATE RITORNATO ALL'ORIGINE!!");
//	  return;
//  }
// }
//}



// ***** FUNZIONE PER IL TRATTAMENTO DI DI UN DIRTY ONE UPDATE
void
ForwardingStrategy::OnUpdDirtyOne (const Ptr<Face> &incomingFace,
                                Ptr<ContentObjectHeader> &header,
                                Ptr<Packet> &payload,
                                const Ptr<const Packet> &packet)
{
  NS_LOG_FUNCTION (incomingFace << header->GetName () << payload << packet);

  bool already_received = false;

  // Verifico la tipologia dell'interfaccia di arrivo dell'update (App o NetDevice); se è una AppFace si tratta di un update generato localmente,
  // quindi va trasmesso verso tutte le interfacce

  TypeId faceType = incomingFace->GetInstanceTypeId();
  std::string faceType_name = faceType.GetName();

  // ***********  L'UPDATE MI ARRIVA DALLA AppFACE !! LO DEVO INOLTRARE VERSO TUTTI I NETDEVICE!! ****************

  if(faceType_name.compare("ns3::ndn::AppFace") == 0)                // L'Interest è giunto dal livello applicativo quindi va inoltrato a tutte le interfacce
  {

	  //m_outUpdateD1();

	  uint32_t intCounter = 0;
	  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
	  if (pitEntry == 0)
	  {
	    DidReceiveUnsolicitedData (incomingFace, header, payload);
	    NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
	    return;
	  }
	  else
	  {
		  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
		  {
			  bool ok = incoming.m_face->Send (packet->Copy ());
			  if (ok)
			  {
			     intCounter++;
				  //m_outData (header, payload, incomingFace == 0, incoming.m_face);
			     DidSendOutData (incoming.m_face, header, payload, packet);

			  	 NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
			  }
			  else
			  {
			  	  //m_dropData (header, payload, incoming.m_face);
			      NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
			  }

		  }
		  m_outUpdateD1((uint32_t)1, intCounter);
	  }
  }

  // *********** L'UPDATE MI ARRIVA DA UN NETDEVICE !! DEVO CONTROLLARE SE SI TRATTA DI UN DUPLICATO********

  else
  {

  // **** CHECK ULTERIORE PER EVITARE CHE L'UPDATE VENGA PROCESSATO DALLO STESSO NODO CHE LO HA GENERATO !! ******
  if(header->GetSignedInfo().GetSenderID() != incomingFace->GetNode()->GetId())   // Verifico che l'update non ritorni al nodo che l'ha generato
  {
	  m_inUpdateD1 (incomingFace);

	  // Lookup PIT entry; Ci sarà una PIT entry di default in tutti i nodi per i messaggi di update (tipo /update_0 e /update_1)
	  uint32_t intCounter = 0;
	  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*header);
	  if (pitEntry == 0)
	  {
		  DidReceiveUnsolicitedData (incomingFace, header, payload);
		  NS_LOG_FUNCTION ("ATTENZIONE!! PIT ENTRY RELATIVA AGLI UPDATE NON TROVATA!!");
		  return;
	  }

	  else
	  {
		  uint32_t num_inc_face = incomingFace->GetId();
		  Time updTimestamp = header->GetSignedInfo().GetTimestamp();
		  uint32_t senderID = header->GetSignedInfo().GetSenderID();

		  std::map<uint32_t, Time>::iterator it = m_bloomFilter->tab_upd_dirty_ones_[num_inc_face]->find(senderID);

		  // **** CASO: NON HO MAI RICEVUTO QUESTO UPDATE SULL'INTERFACCIA CONSIDERATA *****

		  if(it == m_bloomFilter->tab_upd_dirty_ones_[num_inc_face]->end())
		  {
			  // Non ho trovato nessun update già ricevuto da quel nodo e da quella interfaccia. Quindi faccio le operazioni necessarie
			  // di aggiornamento del BF, aggiungo l'update nella tabella e poi inoltro l'update.
			  // **** INSERISCO LE INFO NELLA TABELLA RELATIVA ********
			  m_bloomFilter->tab_upd_dirty_ones_[num_inc_face]->insert(std::pair<uint32_t, Time>(senderID,updTimestamp));

			  // Funzione per aggiornare il counting BF dell'interfaccia di arrivo dell'update. Passo tale interfaccia e l'id del nodo che ha inviato l'update
			  // in maniera tale da recuperarlo (ricordiamo che il payload è virtuale)
			  // **** AGGIORNO IL COUNTING BF DELL'INTERFACCIA CONSIDERATA ******
			  m_bloomFilter->update_dirtyOnes(num_inc_face, senderID);

			  // ***** ADESSO DEVO CONTROLLARE CHE L'UPDATE NON SIA STATO RICEVUTO DA NESSUNA DELLE INTERFACCE, IN QUANTO, SE COSÌ NON FOSSE, NON AVREBBE
			  // ***** SENSO INVIARLO NUOVAMENTE.


			  //bool already_received = false;
			  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
			  {
				  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
				  {
					  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
                      //already_received = true;
				  }
				  else
				  {

					  it = m_bloomFilter->tab_upd_dirty_ones_[incoming.m_face->GetId()]->find(senderID);

					  if(it!=m_bloomFilter->tab_upd_dirty_ones_[incoming.m_face->GetId()]->end())
					  {

						  // Ho trovato un'interfaccia che ha ricevuto un update da quel nodo. Adesso, tramite il timestamp, devo verificare se si tratta
						  // di un duplicato o di un effettivo aggiornamento
						  if(it->second >= updTimestamp)
						  {
							  // Vuol dire che la traccia nella tabella si riferisce allo stesso update.
							  already_received = true;
							  break;
						  }
					  }
				  }
			  }

			  if(!already_received)
			  {
				  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
				  {
					  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
					  {
					 	  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
					                      //already_received = true;
					  }

					  bool ok = incoming.m_face->Send (packet->Copy ());
				      if (ok)
				      {
				         //m_outData (header, payload, incomingFace == 0, incoming.m_face);
				         intCounter++;
				    	 DidSendOutData (incoming.m_face, header, payload, packet);
    	  			     NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
				      }
				  	  else
				  	  {
				  	     //m_dropData (header, payload, incoming.m_face);
				  	     NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
				  	  }

				  }
				  m_outUpdateD1((uint32_t)0, intCounter);
			  }
			  else
			  {
				  NS_LOG_DEBUG ("UPDATE ALREADY RECEIVED!");
			  }
		  }


     	  else  // Nell'interfaccia d'arrivo ho già un update di quel nodo
		  {
			  if(it->second < updTimestamp)
			  {
				  // Vuol dire che la traccia nella tabella si riferisce ad un update vecchio. Quindi aggiorno BF dell'interfaccia d'arrivo e inoltro
				  // l'update verso le interfaccie che non lo hanno ancora ricevuto

				  m_bloomFilter->update_dirtyOnes(num_inc_face, senderID);
				  it->second = updTimestamp;

				  //bool already_received = false;
				  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
				  {
					  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
					  {
						  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
	                      //already_received = true;
					  }
					  else
					  {

						  it = m_bloomFilter->tab_upd_dirty_ones_[incoming.m_face->GetId()]->find(senderID);

						  if(it!=m_bloomFilter->tab_upd_dirty_ones_[incoming.m_face->GetId()]->end())
						  {

							  // Ho trovato un'interfaccia che ha ricevuto un update da quel nodo. Adesso, tramite il timestamp, devo verificare se si tratta
							  // di un duplicato o di un effettivo aggiornamento
							  if(it->second >= updTimestamp)
							  {
								  // Vuol dire che la traccia nella tabella si riferisce allo stesso update.
								  already_received = true;
								  break;
							  }
						  }
					  }
				  }

				  if(!already_received)
				  {
					  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
					  {
						  if (incoming.m_face->GetId() == num_inc_face)       // Verifico se l'interfaccia selezionata è uguale a quella di arrivo dell'update.
						  {
						 	  continue;                                       // Le operazioni sell'interfaccia d'arrivo sono state già effettuate, quindi vado avanti
						                      //already_received = true;
						  }

						  bool ok = incoming.m_face->Send (packet->Copy ());
					      if (ok)
					      {
					         //m_outData (header, payload, incomingFace == 0, incoming.m_face);
					         intCounter++;
					    	 DidSendOutData (incoming.m_face, header, payload, packet);
	    	  			     NS_LOG_DEBUG ("UPDATE FORWARDED TO:\t " << *incoming.m_face);
					      }
					  	  else
					  	  {
					  	     //m_dropData (header, payload, incoming.m_face);
					  	     NS_LOG_DEBUG ("CANNOT FORWARD THE UPDATE TO:\t" << *incoming.m_face);
					  	  }

					  }
					  m_outUpdateD1((uint32_t)0, intCounter);
				  }
				  else
				  {
					  NS_LOG_DEBUG ("UPDATE ALREADY RECEIVED!");
				  }
			  }
			  else
			  {
				  NS_LOG_DEBUG("UPDATE ALREADY RECEIVED ON INTERFACE:\t" << incomingFace->GetId());
			  }
		  }
	  }
  }

  else   //UPDATE RICEVUTO DALLO STESSO NODO CHE LO HA GENERATO.
  {
	  NS_LOG_DEBUG("UPDATE RITORNATO ALL'ORIGINE!!");
	  return;
  }
 }
}

void ForwardingStrategy::setThreshold(uint32_t nodeID)
{
	thr_counter = 0;
	uint32_t cs_size = m_contentStore->GetMaxSize();
	//uint32_t sender = incomingFace->GetNode()->GetId();
	//NS_LOG_DEBUG("IL VALORE DEL CS DEL NODO:\t" << sender << "E':\t" << cs_size);
	uint32_t thr = threshold;
	thr_check = round((double)((double)thr/100)*(double)cs_size);
	//NS_LOG_DEBUG("IL VALORE DI SOGLIA E':\t" << thr_check);
}





void
ForwardingStrategy::DidReceiveDuplicateInterest (const Ptr<Face> &incomingFace,
                                                     Ptr<InterestHeader> &header,
                                                     const Ptr<const Packet> &packet,
                                                     Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this << boost::cref (*incomingFace));
  /////////////////////////////////////////////////////////////////////////////////////////
  //                                                                                     //
  // !!!! IMPORTANT CHANGE !!!! Duplicate interests will create incoming face entry !!!! //
  //                                                                                     //
  /////////////////////////////////////////////////////////////////////////////////////////
  //pitEntry->AddIncoming (incomingFace);
  //m_dropInterests (header, incomingFace);
}

void
ForwardingStrategy::DidExhaustForwardingOptions (const Ptr<Face> &incomingFace,
                                                     Ptr<InterestHeader> header,
                                                     const Ptr<const Packet> &packet,
                                                     Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this << boost::cref (*incomingFace));
  //m_dropInterests (header, incomingFace);
}

void
ForwardingStrategy::FailedToCreatePitEntry (const Ptr<Face> &incomingFace,
                                                Ptr<InterestHeader> header,
                                                const Ptr<const Packet> &packet)
{
  NS_LOG_FUNCTION (this);
  //m_dropInterests (header, incomingFace);
}
  
void
ForwardingStrategy::DidCreatePitEntry (const Ptr<Face> &incomingFace,
                                           Ptr<InterestHeader> header,
                                           const Ptr<const Packet> &packet,
                                           Ptr<pit::Entry> pitEntrypitEntry)
{
}

bool
ForwardingStrategy::DetectRetransmittedInterest (const Ptr<Face> &incomingFace,
                                                     Ptr<pit::Entry> pitEntry)
{
  pit::Entry::in_iterator inFace = pitEntry->GetIncoming ().find (incomingFace);

  bool isRetransmitted = false;
  
  if (inFace != pitEntry->GetIncoming ().end ())
    {
      // this is almost definitely a retransmission. But should we trust the user on that?
      isRetransmitted = true;
    }

  return isRetransmitted;
}

void
ForwardingStrategy::SatisfyPendingInterest (const Ptr<Face> &incomingFace,
                                                Ptr<const ContentObjectHeader> header,
                                                Ptr<const Packet> payload,
                                                const Ptr<const Packet> &packet,
                                                Ptr<pit::Entry> pitEntry)
{

  // Questa funzione viene chiamata sia per gli Interest che per i ContentObjects ricevuti. Se gli viene passato '0' come valore di Incoming Face
  // vuol dire che la chiamata è stata generata da un Interest che viene soddisfatto dalla cache locale; altrimenti, vuol dire che la chiamata è stata
  // generata da un ContentObject ricevuto, e quindi l'incoming face va rimossa dalla lista delle interfacce.

  if (incomingFace != 0)
    pitEntry->RemoveIncoming (incomingFace);

//  Ptr<ContentObjectHeader> coh = Create<ContentObjectHeader> ();
//  packet->RemoveHeader(*coh);
//  coh->GetSignedInfo().SetUpdType(2);
//  packet->AddHeader(*coh);
  //satisfy all pending incoming Interests
  BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
    {

	  bool ok = incoming.m_face->Send (packet->Copy ());
	  TypeId faceType = incoming.m_face->GetInstanceTypeId();
	  std::string faceType_name = faceType.GetName();
	  if (ok)
        {
          if(faceType_name.compare("ns3::ndn::AppFace") == 0)
        	  m_inDataCacheApp (header);

          else
        	  m_outData (header, incomingFace == 0, incoming.m_face);


          DidSendOutData (incoming.m_face, header, payload, packet);
          
          NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);
        }
      else
        {
     	  //uint32_t dropped = 0;
          //m_dropData (header, incoming.m_face, dropped);
          //m_dropData (header, dropped);
          NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
        }
          
      // successfull forwarded data trace
    }

  // All incoming interests are satisfied. Remove them
  pitEntry->ClearIncoming ();

  // Remove all outgoing faces
  pitEntry->ClearOutgoing ();
          
  // Set pruning timout on PIT entry (instead of deleting the record)
  m_pit->MarkErased (pitEntry);
}

void
ForwardingStrategy::DidReceiveUnsolicitedData (const Ptr<Face> &incomingFace,
                                                   Ptr<const ContentObjectHeader> header,
                                                   Ptr<const Packet> payload)
{
  if (m_cacheUnsolicitedData)
    {
      // Optimistically add or update entry in the content store
      m_contentStore->Add (header, payload);
    }
  else
    {
      // Drop data packet if PIT entry is not found
      // (unsolicited data packets should not "poison" content store)
      
      //drop dulicated or not requested data packet
      // uint32_t unsolicited = 1;
      //m_dropData (header, incomingFace, unsolicited);
      //m_dropData (header, unsolicited);
NS_LOG_FUNCTION("Unsolicited Data!");
    }
}

void
ForwardingStrategy::WillSatisfyPendingInterest (const Ptr<Face> &incomingFace,
                                                    Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator out = pitEntry->GetOutgoing ().find (incomingFace);
  
  // If we have sent interest for this data via this face, then update stats. (aggiorno il round trip time)
  if (out != pitEntry->GetOutgoing ().end ())
    {
      pitEntry->GetFibEntry ()->UpdateFaceRtt (incomingFace, Simulator::Now () - out->m_sendTime);  // m_sendTime = tempo di invio del primo Interest
    } 
}

bool
ForwardingStrategy::ShouldSuppressIncomingInterest (const Ptr<Face> &incomingFace,
                                                        Ptr<pit::Entry> pitEntry)
{
  bool isNew = pitEntry->GetIncoming ().size () == 0 && pitEntry->GetOutgoing ().size () == 0;

  if (isNew) return false; // never suppress new interests
  
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (incomingFace, pitEntry);    // Se l'Interest era già stato ricevuto in precedenza su questa interfaccia, risulterà
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  // già tra le incoming face della PIT entry creata in precedenza per questo interest, il
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  // quale, quindi, verrà considerato una ritrasmissione. Inoltre, se la PIT è stata già
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  // creata in precedenza, vuol dire che lo è anche la FIB ad essa associata. Quindi le
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  // interfacce ordinate ci sono già.

  if (pitEntry->GetOutgoing ().find (incomingFace) != pitEntry->GetOutgoing ().end ())
    {
      NS_LOG_DEBUG ("Non duplicate interests from the face we have sent interest to. Don't suppress");
      // got a non-duplicate interest from the face we have sent interest to
      // Probably, there is no point in waiting data from that face... Not sure yet

      // If we're expecting data from the interface we got the interest from ("producer" asks us for "his own" data)
      // Mark interface YELLOW, but keep a small hope that data will come eventually.

      // ?? not sure if we need to do that ?? ...
      
      // pitEntry->GetFibEntry ()->UpdateStatus (incomingFace, fib::FaceMetric::NDN_FIB_YELLOW);
    }
  else
    if (!isNew && !isRetransmitted)
      {
        return true;
      }

  return false;
}

// ******** PROPAGETE INTEREST ORIGINARIO ********

void
ForwardingStrategy::PropagateInterest (const Ptr<Face> &incomingFace,
                                           Ptr<InterestHeader> header,
                                           const Ptr<const Packet> &packet,
                                           Ptr<pit::Entry> pitEntry)
{
  bool isRetransmitted = m_detectRetransmissions && // a small guard
                         DetectRetransmittedInterest (incomingFace, pitEntry);

  pitEntry->AddIncoming (incomingFace);
  /// @todo Make lifetime per incoming interface
  //pitEntry->UpdateLifetime (header->GetInterestLifetime ());
  pitEntry->UpdateLifetime (Seconds(20.0));


  //Ptr<Face> prevFace = (*pitEntry->GetOutgoing().rend()).m_face;

  bool propagated = DoPropagateInterest (incomingFace, header, packet, pitEntry);

  if (!propagated && isRetransmitted) //give another chance if retransmitted
    {
      // increase max number of allowed retransmissions
      pitEntry->IncreaseAllowedRetxCount ();

      // try again
      propagated = DoPropagateInterest (incomingFace, header, packet, pitEntry);
    }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it.
  if (!propagated && pitEntry->GetOutgoing ().size () == 0)     // Non si è riuscito a propagare l'Interest oppure la ForwardingStrategy, a monte, non
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	// è riuscita a trovare una rotta adatta.
    {
      DidExhaustForwardingOptions (incomingFace, header, packet, pitEntry);
        m_pit->MarkErased (pitEntry);
    }
}

// ********* PROPAGATE INTEREST MODIFICATO ***********

void
ForwardingStrategy::PropagateInterest_NEW (const Ptr<Face> &incomingFace,
                                           Ptr<InterestHeader> header,
                                           const Ptr<const Packet> &packet,
                                           Ptr<pit::Entry> pitEntry)
{
  //bool isRetransmitted = m_detectRetransmissions && // a small guard
//                      DetectRetransmittedInterest (incomingFace, pitEntry);
  
  pitEntry->AddIncoming (incomingFace);
  /// @todo Make lifetime per incoming interface
  //pitEntry->UpdateLifetime (header->GetInterestLifetime ());
  pitEntry->UpdateLifetime (Seconds(2.0));

  
  //Ptr<Face> prevFace = (*pitEntry->GetOutgoing().rend()).m_face;

  bool propagated = DoPropagateInterest (incomingFace, header, packet, pitEntry);

//  if (!propagated && isRetransmitted) //give another chance if retransmitted
//    {
//      // increase max number of allowed retransmissions
//      pitEntry->IncreaseAllowedRetxCount ();
//
//NS_LOG_UNCOND("NODONODO:\t" << incomingFace->GetNode()->GetId() << "\tNumero Ritrasmissione PIT:\t" << pitEntry->GetMaxRetxCount ());
//
//      // try again
//      propagated = DoPropagateInterest (incomingFace, header, packet, pitEntry);
//    }

  // ForwardingStrategy will try its best to forward packet to at least one interface.
  // If no interests was propagated, then there is not other option for forwarding or
  // ForwardingStrategy failed to find it. 
  if (!propagated) //&& pitEntry->GetOutgoing ().size () == 0)     // Non si è riuscito a propagare l'Interest oppure la ForwardingStrategy, a monte, non
	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	// è riuscita a trovare una rotta adatta.
    {
      DidExhaustForwardingOptions (incomingFace, header, packet, pitEntry);
    }
}



// *********  WILLSENDOUTINTEREST ORIGINARIO *********

bool
ForwardingStrategy::WillSendOutInterest (const Ptr<Face> &outgoingFace,
                                             Ptr<InterestHeader> header,
                                             Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outgoingFace);

  if (outgoing != pitEntry->GetOutgoing ().end () && outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
    {
      NS_LOG_ERROR (outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount ());
      return false; // already forwarded before during this retransmission cycle
    }


  bool ok = outgoingFace->IsBelowLimit ();
  if (!ok)
    return false;

  pitEntry->AddOutgoing (outgoingFace);
  return true;
}

// ************ WILLSENDOUTINTEREST MODIFICATO  ************

bool
ForwardingStrategy::WillSendOutInterest_NEW (const Ptr<Face> &outgoingFace,
                                             Ptr<InterestHeader> header,
                                             Ptr<pit::Entry> pitEntry)
{
  pit::Entry::out_iterator outgoing =
    pitEntry->GetOutgoing ().find (outgoingFace);
      
  if (outgoing != pitEntry->GetOutgoing ().end ()  && outgoing->m_retxCount >= pitEntry->GetMaxRetxCount ())
    {
      //NS_LOG_ERROR (outgoing->m_retxCount << " >= " << pitEntry->GetMaxRetxCount ());
//      NS_LOG_UNCOND ("NODONODO:\t" << outgoingFace->GetNode()->GetId() << "\tInterfaccia Usata Precedentemente!! Cambio Interfaccia (Se disponibile)");
	  return false; // already forwarded before during this retransmission cycle
    }

  
  bool ok = outgoingFace->IsBelowLimit ();
  if (!ok)
    return false;

  pitEntry->AddOutgoing (outgoingFace);
  return true;
}


void
ForwardingStrategy::DidSendOutInterest (const Ptr<Face> &outgoingFace,
                                            Ptr<InterestHeader> header,
                                            const Ptr<const Packet> &packet,
                                            Ptr<pit::Entry> pitEntry)
{
  m_outInterests (header, outgoingFace);
}

void
ForwardingStrategy::DidSendOutData (const Ptr<Face> &face,
                                        Ptr<const ContentObjectHeader> header,
                                        Ptr<const Packet> payload,
                                        const Ptr<const Packet> &packet)
{
}

void
ForwardingStrategy::WillErasePendingInterest (Ptr<pit::Entry> pitEntry)
{

  std::stringstream ss;
  pitEntry->GetPrefix().Print(ss);
  std::string name = ss.str();
  Ptr<NameComponents> nameCom = Create<NameComponents> (name);
  Ptr<InterestHeader> header = Create<InterestHeader> ();  // do nothing for now. may be need to do some logging
  header->SetName(nameCom);
  m_expiredEntry(header);
  ss.str("");
}


void
ForwardingStrategy::RemoveFace (Ptr<Face> face)
{
  // do nothing here
}

std::ifstream& ForwardingStrategy::GotoLine(std::ifstream& file, uint32_t num){
      file.seekg(std::ios::beg);
      for(uint32_t i=0; i < num - 1; ++i){
          file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
      }
      return file;
  }


} // namespace ndn
} // namespace ns3
