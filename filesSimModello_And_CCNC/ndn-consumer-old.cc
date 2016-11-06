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
 */

#include "ndn-consumer.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-grid.h"

#include "ns3/ndn-app-face.h"
#include "ns3/ndn-interest-header.h"
#include "ns3/ndn-content-object-header.h"
// #include "ns3/weights-path-stretch-tag.h"

#include <boost/ref.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/tokenizer.hpp>

#include "ns3/names.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstdlib>
#include <math.h>
#include <sstream>
#include <fstream>



namespace ll = boost::lambda;


NS_LOG_COMPONENT_DEFINE ("ndn.Consumer");

namespace ns3 {
namespace ndn {



NS_OBJECT_ENSURE_REGISTERED (Consumer);
    
TypeId
Consumer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::Consumer")
    .SetGroupName ("Ndn")
    .SetParent<App> ()
    .AddAttribute ("StartSeq", "Initial sequence number",
                   IntegerValue (0),
                   MakeIntegerAccessor(&Consumer::m_seq),
                   MakeIntegerChecker<int32_t>())

    .AddAttribute ("Prefix","Name of the Interest",
                   StringValue ("/"),
                   MakeNameComponentsAccessor (&Consumer::m_interestName),
                   MakeNameComponentsChecker ())
    .AddAttribute ("LifeTime", "LifeTime for interest packet",
                   StringValue ("2s"),
                   MakeTimeAccessor (&Consumer::m_interestLifeTime),
                   MakeTimeChecker ())
    .AddAttribute ("MinSuffixComponents", "MinSuffixComponents",
                   IntegerValue(-1),
                   MakeIntegerAccessor(&Consumer::m_minSuffixComponents),
                   MakeIntegerChecker<int32_t>())
    .AddAttribute ("MaxSuffixComponents", "MaxSuffixComponents",
                   IntegerValue(-1),
                   MakeIntegerAccessor(&Consumer::m_maxSuffixComponents),
                   MakeIntegerChecker<int32_t>())
    .AddAttribute ("ChildSelector", "ChildSelector",
                   BooleanValue(false),
                   MakeBooleanAccessor(&Consumer::m_childSelector),
                   MakeBooleanChecker())
    .AddAttribute ("Exclude", "only simple name matching is supported (use NameComponents)",
                   NameComponentsValue (),
                   MakeNameComponentsAccessor (&Consumer::m_exclude),
                   MakeNameComponentsChecker ())

    .AddAttribute ("RetxTimer",
                   "Timeout defining how frequent retransmission timeouts should be checked",
                   StringValue ("25ms"),
                   MakeTimeAccessor (&Consumer::GetRetxTimer, &Consumer::SetRetxTimer),
                   MakeTimeChecker ())

    .AddAttribute ("MaxNumRtx", "Maximum Number of Interest Retransmissions",
                   StringValue ("10"),
                   MakeUintegerAccessor(&Consumer::m_maxNumRtx),
                   MakeUintegerChecker<int32_t>())


    .AddTraceSource ("PathWeightsTrace", "PathWeightsTrace",
                    MakeTraceSourceAccessor (&Consumer::m_pathWeightsTrace))


    ;

  return tid;
}
    
Consumer::Consumer ()
  : m_rand (0, std::numeric_limits<uint32_t>::max ())
  , m_seq (0)
  , m_seqMax (111000) // don't request anything
{
  NS_LOG_FUNCTION_NOARGS ();

  m_rtt = CreateObject<RttMeanDeviation> ();
  TimeValue value;
  this->GetAttributeFailSafe("RetxTimer", value);
  //SetRetxTimer(value.Get());
  SetRetxTimer(Seconds(0.025));
  seq_contenuto = new std::map<std::string, uint32_t> ();
  num_rtx = new std::map<uint32_t, uint32_t> ();
  download_time = new std::map<std::string, DownloadEntry_Chunk> ();
  download_time_first = new std::map<std::string, DownloadEntry_Chunk> ();
  download_time_file = new std::map<std::string, DownloadEntry> ();
  ////  ******  NNNNBBBBB ******* Aumentato il numero di ritrasmissioni consentite al client
  m_maxNumRtx = 100;
}

void
Consumer::SetRetxTimer (Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning ())
    {
      // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
      Simulator::Remove (m_retxEvent); // slower, but better for memory
    }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &Consumer::CheckRetxTimeout, this); 
}

Time
Consumer::GetRetxTimer () const
{
  return m_retxTimer;
}

//  ***************  FUNZIONE CHECKRETXTIMEOUT ORIGINARIA  **************


void
Consumer::CheckRetxTimeout ()
{
  NS_LOG_FUNCTION(Application::GetNode()->GetId() << "\t con CheckRetxTimer:\t" << m_retxTimer << "\t e Interest LifeTime:\t" << m_interestLifeTime);

  Time now = Simulator::Now ();

  Time rto = m_rtt->RetransmitTimeout ();
  


  while (!m_seqTimeouts.empty ())
    {
      SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
        m_seqTimeouts.get<i_timestamp> ().begin ();
      if (entry->time + rto <= now) // timeout expired?
        {
          uint32_t seqNo = entry->seq;
          //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\t RTO SIMATO:\t" << rto.GetNanoSeconds() << "\al TEMPO:\t" << Simulator::Now().GetNanoSeconds() );
          //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\t Entry->time\t" << entry->time << "\trto\t" << rto << "\tseq\t" << seqNo);
          m_seqTimeouts.get<i_timestamp> ().erase (entry);
          OnTimeout (seqNo);
        }
      else
        break; // nothing else to do. All later packets need not be retransmitted
    }

  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &Consumer::CheckRetxTimeout, this); 
}

/*
// ****************  FUNZIONE CHECKRETXTIMEOUT MODIFICATA  ************

void
Consumer::CheckRetxTimeout ()
{
  Time now = Simulator::Now ();

  Time rto = m_rtt->RetransmitTimeout ();

  while (!m_seqTimeouts.empty ())
    {
      SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
        m_seqTimeouts.get<i_timestamp> ().begin ();
      if (entry->time + rto <= now) // timeout expired?
        {
          std::string cont;
          cont.assign(entry->contenuto);
          m_seqTimeouts.get<i_timestamp> ().erase (entry);
          OnTimeout (cont);
        }
      else
        break; // nothing else to do. All later packets need not be retransmitted
    }

  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &Consumer::CheckRetxTimeout, this);
}
*/

// Application Methods
void 
Consumer::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS ();

  // do base stuff
  App::StartApplication ();

  //  ******  AGGIUNTA MICHELE  ********
  // Inizializzazione del ifstream del nodo

  uint32_t ID_NODO;
  std::string PATH = "/media/DATI/tortelli/FILE_RICHIESTE/FILE_RICHIESTE_1/richieste";
  std::string EXT = ".txt";
  std::stringstream ss;
  Ptr<ns3::Node> node = Application::GetNode();

// Metodo per recuperare le variabili globali settate con cmd line
  UintegerValue st;
  std::string sit = "g_numeroSIM";
  ns3::GlobalValue::GetValueByNameFailSafe(sit, st);
  uint64_t sim = st.Get();

  
  ID_NODO = ((*node).GetId())%74;                                 // 73 is the maximum number of clients
  //ss << PATH << ((*node).GetId()+1) << EXT;
  ss << PATH << ID_NODO << "_" << sim << EXT;
  std::string PERCORSO = ss.str();
  const char *PATH_C = PERCORSO.c_str();
  ss.str("");
  //std::ifstream fin(PATH_COMPL);
  //if(!fin) {
  file_richieste.open(PATH_C, std::fstream::in);
  if(!file_richieste){
  	  	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n" << PATH_C << "\t" << sim << "\t" << st.Get();
 		exit(0);
  }

  // ******* (NB) IL CONSUMER NON INVIA PIÙ IL BF ALL'AVVIO, MA LO FARÀ SOLO AL RAGGIUNGIMENTO DELLA SOGLIA.


  //file_richieste = &fin;

  ScheduleNextPacket ();
}
    
void 
Consumer::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS ();

  // cancel periodic packet generation
  Simulator::Cancel (m_sendEvent);

  // cleanup base stuff
  App::StopApplication ();
  //file_richieste.close();
}
    
//     *********************   SendPacket  Originario  *****************************

/*void
Consumer::SendPacket ()
{
  if (!m_active) return;

  NS_LOG_FUNCTION_NOARGS ();

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid

  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s max -> " << m_seqMax << "\n";

  while (m_retxSeqs.size ())                        // LISTA ORDINATA dei numeri di sequenza degli interest da ritrasmettere
    {
      seq = *m_retxSeqs.begin ();
      m_retxSeqs.erase (m_retxSeqs.begin ());

      // NS_ASSERT (m_seqLifetimes.find (seq) != m_seqLifetimes.end ());
      // if (m_seqLifetimes.find (seq)->time <= Simulator::Now ())
      //   {

      //     NS_LOG_DEBUG ("Expire " << seq);
      //     m_seqLifetimes.erase (seq); // lifetime expired. Trying to find another unexpired sequence number
      //     continue;
      //   }
      break;
    }

  if (seq == std::numeric_limits<uint32_t>::max ())              // Dovrebbe entrare in questo 'if' se non c'è nessun Interest da ritrasmettere
    {
      if (m_seqMax != std::numeric_limits<uint32_t>::max ())     // Se è uguale, nn ci sono limiti al numero di Interest da inviare
        {
          if (m_seq >= m_seqMax)                                 // Numero massimo di Interest da inviare raggiunto
            {
              return; // we are totally done
            }
        }

      seq = m_seq++;                                             // Aumenta il numero di sequenza
    }

  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s -> " << seq << "\n";

  // Crea il nome dell'Interest e gli aggiunge in coda il numero di sequenza; 'm_interestName' viene inizializzato con la funzione SetPrefix di AppHelper.
  // Nella implementazione iniziale, il prefix è fisso e cambia solo il numero di sequenza.

  Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
  (*nameWithSequence) (seq);
  //

  InterestHeader interestHeader;
  interestHeader.SetNonce               (m_rand.GetValue ());
  interestHeader.SetName                (nameWithSequence);
  interestHeader.SetInterestLifetime    (m_interestLifeTime);

  // Il "ChildSelector" viene utilizzato per specificare quale regola applicare nel caso in cui un singolo Interest matchi più contenuti. Di default è "false"
  // cioè si selezione il contenuto con il nome che matcha rightmost (cioè con il matching più lungo); "true" è left most.
  interestHeader.SetChildSelector       (m_childSelector);
  if (m_exclude.size ()>0)
    {
      interestHeader.SetExclude (Create<NameComponents> (m_exclude));
    }
  interestHeader.SetMaxSuffixComponents (m_maxSuffixComponents);    // Se è negativo nn c'è limite al numero di componenti
  interestHeader.SetMinSuffixComponents (m_minSuffixComponents);

  // NS_LOG_INFO ("Requesting Interest: \n" << interestHeader);
  NS_LOG_INFO ("> Interest for " << seq);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (interestHeader);
  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());

  m_protocolHandler (packet);                                      // Il pacchetto creato viene passato al livello inferiore.

  NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");

  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  m_transmittedInterests (&interestHeader, this, m_face);

  m_rtt->SentSeq (SequenceNumber32 (seq), 1);
  ScheduleNextPacket ();
}*/

// ***********************************   SEND PACKET MODIFICATO MICHELE  ************************

//void
//Consumer::SendPacket ()
//{
//  if (!m_active) return;
//
//  NS_LOG_FUNCTION_NOARGS ();
//
//  bool check = true;            // Mi serve per verificare se si tratta di una ritrasmissione che ha raggiunto il limite massimo.
//  bool rtx = false;             // Mi serve per verificare se si tratta effettivamente di una ritrasmissione.
//  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid
//
//  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s max -> " << m_seqMax << "\n";
//
//  while (m_retxSeqs.size ())                        // LISTA ORDINATA dei numeri di sequenza degli interest da ritrasmettere
//  {
//	  //cont_da_ritr.assign(*m_retxSeqs.begin());
//	  seq = *m_retxSeqs.begin ();
//	  //uint32_t lung_coda_ritr = (uint32_t) m_retxSeqs.size();
//      //seq = m_seq-lung_coda_ritr;
//	  m_retxSeqs.erase (m_retxSeqs.begin ());
//	  m_seq = seq;
//	  rtx = true;
//
//      // NS_ASSERT (m_seqLifetimes.find (seq) != m_seqLifetimes.end ());
//      // if (m_seqLifetimes.find (seq)->time <= Simulator::Now ())
//      //   {
//
//      //     NS_LOG_DEBUG ("Expire " << seq);
//      //     m_seqLifetimes.erase (seq); // lifetime expired. Trying to find another unexpired sequence number
//      //     continue;
//      //   }
//      break;
//  }
//
//  // **** ENTRA IN QUESTO IF SE NON C'È NESSUN INTEREST DA RITRASMETTERE
//  if (seq == std::numeric_limits<uint32_t>::max ())
//  {
//    if (m_seqMax != std::numeric_limits<uint32_t>::max ())       // Se è uguale, nn ci sono limiti al numero di Interest da inviare
//    {
//       if (m_seq >= m_seqMax)                                    // Numero massimo di Interest da inviare raggiunto
//       {
//           return; // we are totally done
//        }
//    }
//
//    seq = m_seq++;                                             // Aumenta il numero di sequenza
//   }
//
//  // **** VERIFICO SE SI TRATTA DI UNA RITRASMISSIONE
//  if (rtx)
//  {
//	  // **** VERIFICO SE SI È RAGGIUNTO IL NUMERO MASSIMO DI RITRASMISSIONI
//	  if(num_rtx->find(seq)->second >= m_maxNumRtx)
//	  {
//		  // **** AGGIUNGERE IL TRACE DEL NUMERO MASSIMO DI RITRASMISSIONI RAGGIUNTE ******
//		  // Si è raggiunto il numero massimo di ritrasmissioni
//		  GotoLine(file_richieste, (seq+1));
//		  std::string line_rtr;
//		  file_richieste >> line_rtr;
//		  Ptr<InterestHeader> header = Create<InterestHeader>();
//		  header->SetName(Create<NameComponents> (line_rtr));
//		  App::OnNumMaxRtx(header);
//
//		  num_rtx->erase(seq);
//
//		  m_seqLifetimes.erase (seq);
//          m_seqTimeouts.erase (seq);
// 		  m_retxSeqs.erase (seq);
//
//		  seq = m_seq++;                  // Non chiedo più questo contenuto e passo al successivo
//		  check = true;
//
//	  }
//	  // **** CI SONO ANCORA DELLE RITRASMISSIONI DISPONIBILI
//	  else
//	  {
//		  num_rtx->find(seq)->second++;
//		  GotoLine(file_richieste, (seq+1));
//		  std::string line_rtr;
//		  file_richieste >> line_rtr;
//		  Ptr<InterestHeader> header = Create<InterestHeader>();
//		  header->SetName(Create<NameComponents> (line_rtr));
//		  App::OnRtoScaduto(header);   // Mi interessa capire quanti timeout scaduti genero e non il tempo esatto del timeout.
//		  check = false;               // Ho già aggiunto in precedenza questo interest nella mappa con il numero di ritrasmissioni
//	  }
//
//  }
//
//  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s -> " << seq << "\n";
//
//  // Crea il nome dell'Interest e gli aggiunge in coda il numero di sequenza; 'm_interestName' viene inizializzato con la funzione SetPrefix di AppHelper.
//  // Nella implementazione iniziale, il prefix è fisso e cambia solo il numero di sequenza.
//
//  //  ******* MODIFICA MICHELE  ******
//
//  // Accesso diretto alla riga specificata da (seq+1) del file delle richieste
//  GotoLine(file_richieste,(seq+1));
//  std::string line;
//  file_richieste >> line;
//
//  seq_contenuto->insert(std::pair<std::string,uint32_t> (line,seq));
//
//  download_time->insert(std::pair<std::string,int64_t> (line, Simulator::Now().GetNanoSeconds()));
//
//  if(check)  // Se si tratta della prima volta che invio questo interest, setto il contatore a uno
//  {
//	  num_rtx->insert(std::pair<uint32_t, uint32_t> (seq,1));
//  }
//
//
//  // ********* Display il contenuto di seq_contenuto (cioè l'associazione tra gli Interest inviati e il relativo numero di sequenza)
////  for(std::map<std::string, uint32_t>::iterator iter= seq_contenuto->begin(); iter != seq_contenuto->end(); iter++) {
////      std::cout << iter->first << " " << iter->second << std::endl;
////   }
//
//
//  //Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
//  //(*nameWithSequence) (seq);
//
//  //Ptr<NameComponents> nameWithSequence = Create<NameComponents> (line);
//  //(*nameWithSequence) (seq);
//
//  Ptr<NameComponents> desiredContent = Create<NameComponents> (line);
//
//  InterestHeader interestHeader;
//  interestHeader.SetNonce               (m_rand.GetValue ());
//  //interestHeader.SetName                (nameWithSequence);
//  interestHeader.SetName				(desiredContent);
//  interestHeader.SetInterestLifetime    (m_interestLifeTime);
//
//  // Il "ChildSelector" viene utilizzato per specificare quale regola applicare nel caso in cui un singolo Interest matchi più contenuti. Di default è "false"
//  // cioè si seleziona il contenuto con il nome che matcha rightmost (cioè con il matching più lungo); "true" è left most.
//  interestHeader.SetChildSelector       (m_childSelector);
//  if (m_exclude.size ()>0)
//    {
//      interestHeader.SetExclude (Create<NameComponents> (m_exclude));
//    }
//  interestHeader.SetMaxSuffixComponents (m_maxSuffixComponents);    // Se è negativo nn c'è limite al numero di componenti
//  interestHeader.SetMinSuffixComponents (m_minSuffixComponents);
//
//  // NS_LOG_INFO ("Requesting Interest: \n" << interestHeader);
//  NS_LOG_INFO ("> Interest for " << line << "\t" << "Sequence Number: " << seq);
//
//  //********  TOKENIZZAZIONE STRINGHE!! ******************************************
//
////  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
////  boost::char_separator<char> sep("/");
////  tokenizer tokens(line, sep);
////  //char f_index = '0';
////
////  int i=1;
////  for(tokenizer::iterator beg=tokens.begin(); beg!=tokens.end();++beg)
////  {
////
////      NS_LOG_INFO ("> Field " << i << ":\t" << *beg << "\t" << "Sequence Number: " << seq);
////      ++i;
////  }
//
//  std::list<std::string> nome_interest (interestHeader.GetNamePtr()->GetComponents().size());
//  	nome_interest.assign(interestHeader.GetNamePtr()->GetComponents().begin(), interestHeader.GetNamePtr()->GetComponents().end());
//
//  	std::stringstream ss;
//  	std::string nome_completo;
//  	std::list<std::string>::iterator it;
//  	it = nome_interest.begin();
//  	for(std::size_t i=0; i< nome_interest.size(); i++)
//  	{
//  		NS_LOG_INFO ("> Field " << i << ":\t" << (*it) << "\t" << "Sequence Number: " << seq);
//  		++it;
//  	}
//  	ss << interestHeader.GetName();
//  	nome_completo = ss.str();
//  	NS_LOG_INFO ("> Nome Completo:\t" << nome_completo);
//  	ss.str("");
//
//
//  //******************************************************************************
//
//  Ptr<Packet> packet = Create<Packet> ();
//  packet->AddHeader (interestHeader);
//  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());
//
//  // ***** m_seqTimeouts ci dice quanti Interest sono ancora in volo
//  NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");
//
//  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
//  m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
//  m_transmittedInterests (&interestHeader, this, m_face);
//
//  m_rtt->SentSeq (SequenceNumber32 (seq), 1);
//
//  m_protocolHandler (packet);                                      // Il pacchetto creato viene passato al livello inferiore.
//
//  //NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");
//
//  // m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
//  // m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
//  //m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
//  //m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
//  //m_transmittedInterests (&interestHeader, this, m_face);
//
//
//  //m_rtt->SentSeq (SequenceNumber32 (seq), 1);
//  ScheduleNextPacket ();
//}


void
Consumer::SendPacket ()
{
  if (!m_active) return;

  NS_LOG_FUNCTION_NOARGS ();

  bool retransmit = false;

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid

  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s max -> " << m_seqMax << "\n";
  
  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tLunghezza CODA RITRASMISSIONE:\t" << m_retxSeqs.size() << "\tal Tempo:\t" <<  Simulator::Now());

  //std::set::iterator iter;

  while (m_retxSeqs.size ())                        // LISTA ORDINATA dei numeri di sequenza degli interest da ritrasmettere
  {
	  //cont_da_ritr.assign(*m_retxSeqs.begin());
	  seq = *m_retxSeqs.begin ();

	  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tNUMSEQ RESTITUITO DA INIZIO LISTA:\t" << seq << "\t Time:\t" << Simulator::Now());

	  //uint32_t lung_coda_ritr = (uint32_t) m_retxSeqs.size();
      //seq = m_seq-lung_coda_ritr;
	  m_retxSeqs.erase (m_retxSeqs.begin ());
	  // (NB) **** COMMENTO INSERITO *****//m_seq = seq;

	  retransmit = true;

      // NS_ASSERT (m_seqLifetimes.find (seq) != m_seqLifetimes.end ());
      // if (m_seqLifetimes.find (seq)->time <= Simulator::Now ())
      //   {
          
      //     NS_LOG_DEBUG ("Expire " << seq);
      //     m_seqLifetimes.erase (seq); // lifetime expired. Trying to find another unexpired sequence number
      //     continue;
      //   }
      break;
  }

  if (seq == std::numeric_limits<uint32_t>::max ())              // Dovrebbe entrare in questo 'if' se non c'è nessun Interest da ritrasmettere
  {
    if (m_seqMax != std::numeric_limits<uint32_t>::max ())       // Se è uguale, nn ci sono limiti al numero di Interest da inviare
    {
       if (m_seq >= m_seqMax)                                    // Numero massimo di Interest da inviare raggiunto
       {
           return; // we are totally done
        }
    }
      
    seq = m_seq++;                                             // Aumenta il numero di sequenza
    // Inserisco il nuovo numero di sequenza nella mappa delle ritrasmissioni
    
  }
  
  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s -> " << seq << "\n";
  
  // Crea il nome dell'Interest e gli aggiunge in coda il numero di sequenza; 'm_interestName' viene inizializzato con la funzione SetPrefix di AppHelper.
  // Nella implementazione iniziale, il prefix è fisso e cambia solo il numero di sequenza.

  //  ******* MODIFICA MICHELE  ******

  // Accesso diretto alla riga specificata da (seq+1) del file delle richieste
  GotoLine(file_richieste,(seq+1));
  
  //if(file_richieste.eof())
	//return;
  std::string line_1;
  //file_richieste >> line;
  std::getline(file_richieste,line_1);
  
  if(line_1.empty())
    return;

  
  const std::string line = line_1;
  //if(line.empty())
    //return;


  // Adesso ogni stringa del file è composta dal nome del chunk da richiedere e dal numero di chunk attesi per quel file (in corrispondenza dei primi
  // chunk di ciascun contenuto, altrimenti c'è uno zero).
        	  
  std::stringstream ss;
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("\t");
  tokenizer tokens_space(line, sep);
  tokenizer::iterator it_space=tokens_space.begin();
  const std::string contenutoChunk = *it_space;
  it_space++;
  const std::string expChunk_string = *it_space;

  
  //NS_LOG_UNCOND ("Richiesta come letta da file:\t" << line);

  //line = contenutoChunk;
  
  if(!retransmit) {
	    std::string intApp = contenutoChunk;
	    App::OnInterestApp(&intApp);
        std::map<std::string, uint32_t>::iterator it;
        it = seq_contenuto->find(contenutoChunk);
        if(it!=seq_contenuto->end())
        {
             m_rtt->IncrNext(SequenceNumber32 (seq), 1);
             ScheduleNextPacket ();
             return;
        }
        else
        {
//NS_LOG_UNCOND("Contenuto da richiedere:\t" << contenutoChunk << "\t con numero chunk attesi:\t" << expChunk_string);
        	num_rtx->insert(std::pair<uint32_t, uint32_t> (seq,1));
        	seq_contenuto->insert(std::pair<std::string,uint32_t>(contenutoChunk,seq));

        	const char* expChunk = expChunk_string.c_str();
        	uint32_t numChunk = std::atoi(expChunk);
        	if(numChunk != 0)     // Si tratta del primo chunk del contenuto, quindi devo aggiornare download_time_file
        	{
        		//**** NB: Nella mappa "download_time_first" inserisco solo il primo chunk di ogni contenuto
        		//download_time_first->insert(std::pair<std::string,int64_t> (contenutoChunk, Simulator::Now().GetNanoSeconds()));
        		download_time_first->insert(std::pair<std::string,DownloadEntry_Chunk> (contenutoChunk, DownloadEntry_Chunk(Simulator::Now().GetNanoSeconds(), Simulator::Now().GetNanoSeconds(), 0)));
        		//download_time->insert(std::pair<std::string,int64_t> (contenutoChunk, Simulator::Now().GetNanoSeconds()));
        		download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (contenutoChunk, DownloadEntry_Chunk(Simulator::Now().GetNanoSeconds(), Simulator::Now().GetNanoSeconds(), 0)));
        		
        		Ptr<NameComponents> requestedContent = Create<NameComponents> (contenutoChunk);
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
        		const std::string contenutoNoChunk = ss.str();
//NS_LOG_UNCOND("Stesso contenuto senza il chunk:\t" << contenutoNoChunk);
        		ss.str("");

        		//**** NB: Nella mappa "download_time_file" inserisco solo il nome del contenuto senza il chunk
                int64_t time_now = Simulator::Now().GetNanoSeconds();
        		download_time_file->insert(std::pair<std::string,DownloadEntry> (contenutoNoChunk, DownloadEntry(time_now,0,numChunk,0,0)) );
        		      		
        	}
        	else
        	{
        		//download_time->insert(std::pair<std::string,int64_t> (contenutoChunk, Simulator::Now().GetNanoSeconds()));
        		download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (contenutoChunk, DownloadEntry_Chunk(Simulator::Now().GetNanoSeconds(), Simulator::Now().GetNanoSeconds(), 0)));
        	}
        	  
        	//download_time->insert(std::pair<std::string,int64_t> (contenutoChunk, Simulator::Now().GetNanoSeconds()));
        }
  }

  // *** Se si tratta di una ritrasmissione, dovrei aggiornare solo il nuovo tempo di invio.
  else
  {
  	const char* expChunk = expChunk_string.c_str();
  	uint32_t numChunk = std::atoi(expChunk);
  	if(numChunk != 0)     // Il chunk ritrasmesso è il primo del contenuto
  	{
  		if(download_time_first->find(contenutoChunk)!=download_time_first->end())
  			download_time_first->find(contenutoChunk)->second.sentTimeChunk_New = Simulator::Now().GetNanoSeconds();
  		else
  			NS_LOG_UNCOND("Impossibile trovare il contenuto ritrasmesso nella mappa!!");
  	}
  	else // Non si tratta del primo chunk
  	{
  		if(download_time->find(contenutoChunk)!=download_time->end())
  			download_time->find(contenutoChunk)->second.sentTimeChunk_New = Simulator::Now().GetNanoSeconds();
  		else
  			NS_LOG_UNCOND("Impossibile trovare il contenuto ritrasmesso nella mappa!!");

  	}

  }



//  NS_LOG_UNCOND ("NODONODO:\t" << Application::GetNode()->GetId() << "\t Interest Inviato CONSUMER:\t" << line << "\tNumero Sequenza:\t" << seq << "\t" << Simulator::Now().GetNanoSeconds());
  // ********* Display il contenuto di seq_contenuto (cioè l'associazione tra gli Interest inviati e il relativo numero di sequenza)
//  for(std::map<std::string, uint32_t>::iterator iter= seq_contenuto->begin(); iter != seq_contenuto->end(); iter++) {
//      std::cout << iter->first << " " << iter->second << std::endl;
//   }


  //Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_interestName);
  //(*nameWithSequence) (seq);

  //Ptr<NameComponents> nameWithSequence = Create<NameComponents> (line);
  //(*nameWithSequence) (seq);

  Ptr<InterestHeader> interestHeader = Create<InterestHeader>();
  interestHeader->SetName(Create<NameComponents> (contenutoChunk));
  interestHeader->SetNonce               (m_rand.GetValue ());
  interestHeader->SetInterestLifetime    (m_interestLifeTime);
  interestHeader->SetChildSelector       (m_childSelector);
  if (m_exclude.size ()>0)
    {
      interestHeader->SetExclude (Create<NameComponents> (m_exclude));
    }
  interestHeader->SetMaxSuffixComponents (m_maxSuffixComponents);    // Se è negativo nn c'è limite al numero di componenti
  interestHeader->SetMinSuffixComponents (m_minSuffixComponents);


//  Ptr<NameComponents> desiredContent = Create<NameComponents> (line);
//
//  InterestHeader interestHeader;
//  interestHeader.SetNonce               (m_rand.GetValue ());
//  //interestHeader.SetName                (nameWithSequence);
//  interestHeader.SetName				(desiredContent);
//  interestHeader.SetInterestLifetime    (m_interestLifeTime);
//
//  // Il "ChildSelector" viene utilizzato per specificare quale regola applicare nel caso in cui un singolo Interest matchi più contenuti. Di default è "false"
//  // cioè si seleziona il contenuto con il nome che matcha rightmost (cioè con il matching più lungo); "true" è left most.
//  interestHeader.SetChildSelector       (m_childSelector);
//  if (m_exclude.size ()>0)
//    {
//      interestHeader.SetExclude (Create<NameComponents> (m_exclude));
//    }
//  interestHeader.SetMaxSuffixComponents (m_maxSuffixComponents);    // Se è negativo nn c'è limite al numero di componenti
//  interestHeader.SetMinSuffixComponents (m_minSuffixComponents);
        
  // NS_LOG_INFO ("Requesting Interest: \n" << interestHeader);
  //NS_LOG_INFO ("> Interest for " << line << "\t" << "Sequence Number: " << seq);

  //********  TOKENIZZAZIONE STRINGHE!! ******************************************

//  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
//  boost::char_separator<char> sep("/");
//  tokenizer tokens(line, sep);
//  //char f_index = '0';
//
//  int i=1;
//  for(tokenizer::iterator beg=tokens.begin(); beg!=tokens.end();++beg)
//  {
//
//      NS_LOG_INFO ("> Field " << i << ":\t" << *beg << "\t" << "Sequence Number: " << seq);
//      ++i;
//  }

  //std::list<std::string> nome_interest (interestHeader.GetNamePtr()->GetComponents().size());
  	//nome_interest.assign(interestHeader.GetNamePtr()->GetComponents().begin(), interestHeader.GetNamePtr()->GetComponents().end());

  	//std::stringstream ss;
  	//std::string nome_completo;
  	//std::list<std::string>::iterator it;
  	//it = nome_interest.begin();
  	//for(std::size_t i=0; i< nome_interest.size(); i++)
  	//{
  		//NS_LOG_INFO ("> Field " << i << ":\t" << (*it) << "\t" << "Sequence Number: " << seq);
  		//++it;
  	//}
  	//ss << interestHeader.GetName();
  	//nome_completo = ss.str();
  	//NS_LOG_INFO ("> Nome Completo:\t" << nome_completo);
  	//ss.str("");


  //******************************************************************************

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (*interestHeader);
  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());

  // ***** m_seqTimeouts ci dice quanti Interest sono ancora in volo
  NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");

  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  m_transmittedInterests (interestHeader, this, m_face);

  m_rtt->SentSeq (SequenceNumber32 (seq), 1);

  if(retransmit)
	  App::OnRtoScaduto(interestHeader);

  m_protocolHandler (packet);                                      // Il pacchetto creato viene passato al livello inferiore.

  //NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");
  
  // m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  // m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  //m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  //m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  //m_transmittedInterests (&interestHeader, this, m_face);


  //m_rtt->SentSeq (SequenceNumber32 (seq), 1);

  //if(retransmit)
	 // m_seq++;
  ScheduleNextPacket ();
}

std::ifstream& Consumer::GotoLine(std::ifstream& file, uint32_t num){
      file.seekg(std::ios::beg);
      for(uint32_t i=0; i < num - 1; ++i){
          file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
      }
      return file;
  }

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////


void
Consumer::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                               Ptr<Packet> payload)
{
  if (!m_active) return;

  App::OnContentObject (contentObject, payload); // tracing inside
  
  NS_LOG_FUNCTION (this << contentObject << payload);

  NS_LOG_INFO ("Received content object: " << boost::cref(*contentObject));
  
  // **** ESTRAZIONE DEL HOP COUNT ASSOCIATO AL CONTENUTO RECUPERATO ****
  uint32_t dist = contentObject->GetSignedInfo().GetHopCount();

  //uint32_t seq = boost::lexical_cast<uint32_t> (contentObject->GetName ().GetComponents ().back ());

  std::stringstream ss;
  //contentObject->GetName().Print(ss);
  ss << contentObject -> GetName();
  const std::string cont_ric = ss.str();
  std::string cont_ric_app = ss.str();
  uint32_t seq_ric = seq_contenuto->find(cont_ric)->second;
  ss.str("");
  
  if((seq_ric % 100) == 0)   // Ogni 1000 chunk ricevuti, printa una riga

        NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\t DATA RICEVUTO APP:\t" << cont_ric << "\t" << Simulator::Now().GetNanoSeconds());

  
  //NS_LOG_INFO ("Numero di sequenza Data Ricevuto:" << "\t" << seq_ric);

  //NS_LOG_UNCOND ("NODONODO:\t" << Application::GetNode()->GetId() << "\t Data Ricevuto CONSUMER:\t" << cont_ric << "\tNumero Sequenza:\t" << seq_ric << "\t" << Simulator::Now().GetNanoSeconds());
  seq_contenuto->erase(cont_ric);

  // *** CALCOLO E TRACING DEL TEMPO DI DOWNLOAD
  // *** Il "seek time" va calcolato solo sul primo chunk del file
  if(download_time_first->find(cont_ric)!=download_time_first->end())
  {
	  //int64_t tempo_invio = download_time_first->find(cont_ric)->second;
	  int64_t tempo_invio = download_time_first->find(cont_ric)->second.sentTimeChunk_New;

      int64_t tempo_download = (Simulator::Now().GetNanoSeconds() - tempo_invio)+download_time_first->find(cont_ric)->second.incrementalTime;
	  //Ptr<ContentObjectHeader> header = Create<ContentObjectHeader>();
	  //header->SetName(Create<NameComponents> (cont_ric));

	  // LE INFO DEL TEMPO DI DOWNLOAD E DEL HOP COUNT VENGONO INSERITE IN UN UNICO TRACING FILE.
	  //App::OnDownloadTime(header, tempo_invio, tempo_download, dist);
	  App::OnDownloadTime(&cont_ric_app, tempo_invio, tempo_download, dist);
	  download_time_first->erase(cont_ric);
	  //download_time->erase(cont_ric);
  }
  
  // **** NON TRACCIO PIÙ TUTTI I CHUNK, MA SOLO IL SEEK TIME E IL FILE DOWNLOAD TIME
  /*else
  {
	  int64_t tempo_download = Simulator::Now().GetNanoSeconds() - download_time->find(cont_ric)->second;
	  Ptr<ContentObjectHeader> header = Create<ContentObjectHeader>();
	  header->SetName(Create<NameComponents> (cont_ric));

	  // LE INFO DEL TEMPO DI DOWNLOAD E DEL HOP COUNT VENGONO INSERITE IN UN UNICO TRACING FILE.
	  App::OnDownloadTimeAll(header, tempo_download, dist);
	  //download_time->erase(cont_ric);
  }*/


  // ANALISI DEL CHUNK CONTENUTO; AGGIORNAMENTO DELLA STRUTTURA Download_time_file ed eventuale calcolo
  
  Ptr<NameComponents> receivedContent = Create<NameComponents> (cont_ric);
  uint32_t numComponents = receivedContent->GetComponents().size();
  
  std::list<std::string> nome_co_ric (numComponents);
  nome_co_ric.assign(receivedContent->begin(), receivedContent->end());
  std::list<std::string>::iterator it = nome_co_ric.end();
 
  //it = nome_co_ric.back();             // L'iterator punta all'ultima componente.    
  --it;
  std::string stringa_chunk = *it;     // Ultima parte del nome contenente il chunk ricevuto

  it = nome_co_ric.begin();           			// L'iterator punta alla prima componente
  nome_co_ric.pop_back();
  --numComponents;
	
  for(uint32_t t = 0; t < numComponents; t++)
  {
	ss << "/" << *it;
	it++;
  }
  std::string contenutoNoChunk = ss.str();
  ss.str("");

  //  Potrebbe capitare che il penultimo chunk, o chunk precedenti, vengano ricevuti dopo l'ultimo chunk. Siccome in questo caso 
  //  si è già eliminato il nome del file dalla struttura download_time_file, forse va inserito un controllo prima del tipo 
  //  if(download_time_file->find(contenutoNoChunk)!=download_time_file->end())

  if(download_time_file->find(contenutoNoChunk)!=download_time_file->end())  // Vuol dire che non ho ancora cancellato il file 
  {

        uint32_t num_chunk_tot = download_time_file->find(contenutoNoChunk)->second.expNumChunk;
        std::string check_chunk = "s_";
        ss << check_chunk << (num_chunk_tot-1);
        check_chunk = ss.str();
        if (stringa_chunk.compare(check_chunk) != 0)  // Il chunk che ho ricevuto non è l'ultimo; quindi aumento solo il contatore dei chunk ricevuti 
	  	  	  	  	  	      // In realtà, se il chunk che perdo è proprio l'ultimo, i contenuti rispettivi rimarranno in download_time_file
        {
	        download_time_file->find(contenutoNoChunk)->second.rcvNumChunk++;
	        // ** Calcolo il tempo di download del singolo chunk e lo aggiungo alla somma dei download degli altri.
	        //int64_t increment = Simulator::Now().GetNanoSeconds() - download_time->find(cont_ric)->second;
                int64_t increment_temp = Simulator::Now().GetNanoSeconds() - download_time->find(cont_ric)->second.sentTimeChunk_New;
                int64_t increment;                
                if(increment_temp > 100000000) // vuol dire che la ritrasmissione schedulata non è stata ancora effettuata.
                     increment = 100000000 + download_time->find(cont_ric)->second.incrementalTime;
                else
                     increment = (Simulator::Now().GetNanoSeconds() - download_time->find(cont_ric)->second.sentTimeChunk_New)+download_time->find(cont_ric)->second.incrementalTime;

	        download_time_file->find(contenutoNoChunk)->second.sentTime+=increment;
                download_time_file->find(contenutoNoChunk)->second.distance+=dist;
	        download_time->erase(cont_ric);
        }
        else  // Il chunk che ho ricevuto è l'ultimo. Se chunk_ricevuti = chunk_attesi -> calcolo download; altrimenti contenuto perso.
        {
	        if(download_time_file->find(contenutoNoChunk)->second.rcvNumChunk == (num_chunk_tot - 1))
	        {
		        download_time_file->find(contenutoNoChunk)->second.rcvNumChunk++;
		        // ** Calcolo il tempo di download del singolo chunk e lo aggiungo alla somma dei download degli altri.
		        //int64_t increment = Simulator::Now().GetNanoSeconds() - download_time->find(cont_ric)->second;
		        int64_t increment = (Simulator::Now().GetNanoSeconds() - download_time->find(cont_ric)->second.sentTimeChunk_New)+download_time->find(cont_ric)->second.incrementalTime;

		        download_time_file->find(contenutoNoChunk)->second.sentTime+=increment;
                        download_time_file->find(contenutoNoChunk)->second.distance+=dist;
		        download_time->erase(cont_ric);

		        int64_t tempo_download_file = download_time_file->find(contenutoNoChunk)->second.sentTime;
                        int64_t tempo_invio_primo_int = download_time_file->find(contenutoNoChunk)->second.sentTimeFirst;
                        uint32_t mean_dist_file = round(download_time_file->find(contenutoNoChunk)->second.distance / num_chunk_tot);
                        App::OnDownloadTimeFile(&contenutoNoChunk, tempo_invio_primo_int, tempo_download_file, mean_dist_file);
                        download_time_file->erase(contenutoNoChunk);
		        //NS_LOG_UNCOND("CONTENUTO:\t" << contenutoNoChunk << "\tscaricato in:\t" << tempo_file);
                }
	        else
	        {
		        // In questo caso dovrei non aver ricevuto uno o più chunk, quindi ritengo il contenuto perso.
                        int64_t tempo_invio_primo_int = download_time_file->find(contenutoNoChunk)->second.sentTimeFirst;
		        download_time_file->erase(contenutoNoChunk);
		        download_time->erase(cont_ric);
		        //// ****** NBBBB ***** Aggiungere TRACING simile ad App::OnNumMaxRtx(header, tempo_invio) per i file interi
		        //Ptr<ContentObjectHeader> header_uncomplete_file = Create<ContentObjectHeader>();
		        //header_uncomplete_file->SetName(Create<NameComponents> (contenutoNoChunk));
                        App::OnUncompleteFile(&contenutoNoChunk, tempo_invio_primo_int);
	        }
        }
  }
  else
  {
        // Ho già eliminato il file
        download_time->erase(cont_ric);
  }
  
  // ****** NBBBBBBB ********
  // ALTRE COSE DA FARE : Aumentare la durata della simulazione;. Eventuali derivanti dai chunk

  NS_LOG_INFO ("< DATA for " << cont_ric << "\t" << "Sequence Number " << seq_ric);

  m_seqLifetimes.erase (seq_ric);
  m_seqTimeouts.erase (seq_ric);
  m_retxSeqs.erase (seq_ric);

  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tLunghezza CODA RITRASMISSIONE:\t" << m_retxSeqs.size() << "\tal Tempo:\t" <<  Simulator::Now());


  num_rtx->erase(seq_ric);

  // ***********  COMMENTO INSERITO  ************
  m_rtt->AckSeq (SequenceNumber32 (seq_ric+1));

}

void
Consumer::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> origPacket)
{
  if (!m_active) return;
  
  App::OnNack (interest, origPacket); // tracing inside
  
  NS_LOG_DEBUG ("Nack type: " << interest->GetNack ());

  NS_LOG_FUNCTION (this << interest);

  // NS_LOG_INFO ("Received NACK: " << boost::cref(*interest));
  uint32_t seq = boost::lexical_cast<uint32_t> (interest->GetName ().GetComponents ().back ());
  NS_LOG_INFO ("< NACK for " << seq);
  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s -> " << "NACK for " << seq << "\n"; 

  // put in the queue of interests to be retransmitted
  NS_LOG_INFO ("Before: " << m_retxSeqs.size ());

  // *****************   COMMENTO INSERITO   ************
  m_retxSeqs.insert (seq);
  NS_LOG_INFO ("After: " << m_retxSeqs.size ());

  m_rtt->IncreaseMultiplier ();             // Double the next RTO ??
  ScheduleNextPacket ();
}

// *******  FUNZIONE OnTimeout ORIGINARIA  **************

void
Consumer::OnTimeout (uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION (sequenceNumber);
  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " << m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";
  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tNumero ritrasmissione:\t" << num_rtx->find(sequenceNumber)->second << "\t Num Max:\t" << m_maxNumRtx << "\tTempo:\t" << Simulator::Now ());

  // Recupero il nome del contenuto
  GotoLine(file_richieste, (sequenceNumber+1));
  std::string line_rto;
  std::getline(file_richieste,line_rto);

  std::stringstream ss_rto;
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer_rto;
  boost::char_separator<char> sep_rto("\t");
  tokenizer_rto tokens_space_rto(line_rto, sep_rto);
  tokenizer_rto::iterator it_space_rto=tokens_space_rto.begin();
  std::string contenutoChunk_rto = *it_space_rto;
  it_space_rto++;
  const std::string expChunk_rto_string = *it_space_rto;
  const char* expChunk_rto = expChunk_rto_string.c_str();
  uint32_t numChunk_rto = std::atoi(expChunk_rto);

  // **** Calcolo il nuovo tempo incrementale per il chunk
  int64_t new_increment = Simulator::Now().GetNanoSeconds() - download_time->find(contenutoChunk_rto)->second.sentTimeChunk_New;

  // **** Aggiorno il tempo incrementale del chunk
  if(numChunk_rto != 0)     // Si tratta del primo chunk del contenuto, quindi devo aggiornare download_time_first
	  download_time_first->find(contenutoChunk_rto)->second.incrementalTime+=new_increment;
  else
	  download_time->find(contenutoChunk_rto)->second.incrementalTime+=new_increment;

  // Verifico se rientro nel numero max di ritrasmissioni
  if(num_rtx->find(sequenceNumber)->second < m_maxNumRtx)
  {
        // Incremento il contatore delle ritrasmissioni (con ulteriore controllo della presenza dell'elemento nella mappa)
        std::pair<std::map<uint32_t,uint32_t>::iterator,bool> ret;
        ret=num_rtx->insert (std::pair<uint32_t,uint32_t>(sequenceNumber,1) );
        if(ret.second==false)
        {
                // L'elemento è già presente, quindi posso aumentare il contatore
                uint32_t counter = num_rtx->find(sequenceNumber)->second;                        
                num_rtx->operator[](sequenceNumber)= counter+1;
        }
        else
        {
                NS_LOG_UNCOND("ERRORE NELLA MAPPA DELLE RITRASMISSIONI! ELEMENTO NON PRESENTE");
        }

        //num_rtx->find(sequenceNumber)->second++;
        // Recupero il nome del contenuto         
        //GotoLine(file_richieste, (sequenceNumber+1));
        //std::string line_rtr;
	    //file_richieste >> line_rtr;
        //Ptr<InterestHeader> header = Create<InterestHeader>();
	    //header->SetName(Create<NameComponents> (line_rtr));
        //// Creo l'evento per tracciare la ritrasmissione dell'Interest	
        //App::OnRtoScaduto(header);                        // Mi interessa capire quanti timeout scaduti genero e non il tempo esatto del timeout.

        //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tINTEREST RITRASMESSO!:\t" << line_rtr << "\t" << sequenceNumber);
        //m_rtt->IncreaseMultiplier ();             				// Double the next RTO
        m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1);                // make sure to disable RTT calculation for this sample
        m_retxSeqs.insert (sequenceNumber);

  }
  else
  {
        // Recupero il nome del contenuto
        GotoLine(file_richieste, (sequenceNumber+1));
        std::string line_rtr;
        std::getline(file_richieste,line_rtr);
  
        std::stringstream ss_rtr;
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_rtr;
        boost::char_separator<char> sep_rtr("\t");
        tokenizer_rtr tokens_space_rtr(line_rtr, sep_rtr);
        tokenizer_rtr::iterator it_space_rtr=tokens_space_rtr.begin();
        std::string contenutoChunk_rtr = *it_space_rtr;

        //  Devo eliminare il contenuto non recuperato qualora stessi eliminando, dopo 100 ritrasmissioni, l'ultimo chunk. Altrimenti, 
        //  la voce in Download_File rimarrebbe.

        Ptr<NameComponents> receivedContent = Create<NameComponents> (contenutoChunk_rtr);
        uint32_t numComponents = receivedContent->GetComponents().size();
  
        std::list<std::string> nome_co_ric (numComponents);
        nome_co_ric.assign(receivedContent->begin(), receivedContent->end());
        std::list<std::string>::iterator it = nome_co_ric.end();
 
        --it;
        std::string stringa_chunk = *it;     // Ultima parte del nome contenente il chunk ricevuto

        it = nome_co_ric.begin();           			// L'iterator punta alla prima componente
        nome_co_ric.pop_back();
        --numComponents;
	
        std::stringstream ss;
        for(uint32_t t = 0; t < numComponents; t++)
        {
	        ss << "/" << *it;
	        it++;
        }
        std::string contenutoNoChunk = ss.str();
        ss.str("");

        uint32_t num_chunk_tot = download_time_file->find(contenutoNoChunk)->second.expNumChunk;
        std::string check_chunk = "s_";
        ss << check_chunk << (num_chunk_tot-1);
        check_chunk = ss.str();
        if (stringa_chunk.compare(check_chunk) == 0)  // Il chunk che sto eliminando è l'ultimo del file; quindi vado ad eliminare la corrispondente
                                                      // voce nella struttura file_download_time.
                download_time_file->erase(contenutoNoChunk);





	int64_t tempo_invio = download_time->find(contenutoChunk_rtr)->second.sentTimeChunk_First;
        //Ptr<InterestHeader> header = Create<InterestHeader>();
	//header->SetName(Create<NameComponents> (line_rtr));
        // Creo l'evento per tracciare il raggiungimento del numero massimo di rtx e quindi l'eliminazione dell'interest
        App::OnNumMaxRtx(&contenutoChunk_rtr, tempo_invio);
        //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tINTEREST ELIMINATO!:\t" << line_rtr << "\t" << Application::GetNode()->GetId());
        // Lo cancello dalla lista degli interest ritrasmessi
        num_rtx->erase(sequenceNumber);
        seq_contenuto->erase(contenutoChunk_rtr);     /// **** MODIFICA
        m_retxSeqs.erase(sequenceNumber);
        m_seqLifetimes.erase (sequenceNumber);
        m_seqTimeouts.erase (sequenceNumber);
        download_time->erase(contenutoChunk_rtr);
        if(download_time_first->find(contenutoChunk_rtr)!= download_time_first->end())     /// **** MODIFICA
        	download_time_first->erase(contenutoChunk_rtr);

        m_rtt->AckSeq (SequenceNumber32 (sequenceNumber));

       
  }
  
  ScheduleNextPacket (); 
}

//  *******  FUNZIONE OnTimeout MODIFICATA  ***************
/*
void
Consumer::OnTimeout (std::string cont_rich)
{
  NS_LOG_FUNCTION (cont_rich);
  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " << m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

  m_rtt->IncreaseMultiplier ();             // Double the next RTO
  //m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert (cont_rich);
  ScheduleNextPacket ();
}
*/
} // namespace ndn
} // namespace ns3

