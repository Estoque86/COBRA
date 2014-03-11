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

#include "ndn-consumer-rtx.h"
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
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndnSIM/utils/ndn-fw-hop-count-tag.h"


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

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerRtx");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ConsumerRtx);

TypeId
ConsumerRtx::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerRtx")
    .SetGroupName ("Ndn")
    .SetParent<App> ()
    .AddAttribute ("StartSeq", "Initial sequence number",
                   IntegerValue (0),
                   MakeIntegerAccessor(&ConsumerRtx::m_seq),
                   MakeIntegerChecker<int32_t>())

    .AddAttribute ("Prefix","Name of the Interest",
                   StringValue ("/"),
                   MakeNameAccessor (&ConsumerRtx::m_interestName),
                   MakeNameChecker ())
    .AddAttribute ("LifeTime", "LifeTime for interest packet",
                   StringValue ("2s"),
                   MakeTimeAccessor (&ConsumerRtx::m_interestLifeTime),
                   MakeTimeChecker ())

    .AddAttribute ("RetxTimer",
                   "Timeout defining how frequent retransmission timeouts should be checked",
                   StringValue ("25ms"),
                   MakeTimeAccessor (&ConsumerRtx::GetRetxTimer, &ConsumerRtx::SetRetxTimer),
                   MakeTimeChecker ())

    .AddAttribute ("MaxNumRtx", "Maximum Number of Interest Retransmissions",
                   StringValue ("10"),
                   MakeUintegerAccessor(&ConsumerRtx::m_maxNumRtx),
                   MakeUintegerChecker<int32_t>())

    //.AddTraceSource ("LastRetransmittedInterestDataDelay", "Delay between last retransmitted Interest and received Data",
    //                 MakeTraceSourceAccessor (&Consumer::m_lastRetransmittedInterestDataDelay))

    //.AddTraceSource ("FirstInterestDataDelay", "Delay between first transmitted Interest and received Data",
    //                 MakeTraceSourceAccessor (&Consumer::m_firstInterestDataDelay))

    .AddTraceSource ("InterestApp", "InterestApp",
                   MakeTraceSourceAccessor (&ConsumerRtx::m_interestApp))

    .AddTraceSource ("TimeOutTrace", "TimeOutTrace",
                   MakeTraceSourceAccessor (&ConsumerRtx::m_timeOutTrace))

    .AddTraceSource ("DownloadTime", "DownloadTime",
                   MakeTraceSourceAccessor (&ConsumerRtx::m_downloadTime))

    .AddTraceSource ("DownloadTimeFile", "DownloadTimeFile",
                   MakeTraceSourceAccessor (&ConsumerRtx::m_downloadTimeFile))

    .AddTraceSource ("UncompleteFile", "UncompleteFile",
                   MakeTraceSourceAccessor (&ConsumerRtx::m_uncompleteFile))

    .AddTraceSource ("NumMaxRtx", "NumMaxRtx",
                   MakeTraceSourceAccessor (&ConsumerRtx::m_numMaxRtx))



    ;

  return tid;
}

ConsumerRtx::ConsumerRtx ()
  : m_rand (0, std::numeric_limits<uint32_t>::max ())
  , m_seq (0)
  , m_seqMax (111000) // don't request anything
{
  NS_LOG_FUNCTION_NOARGS ();

  m_rtt = CreateObject<RttMeanDeviation> ();
  TimeValue value;
  this->GetAttributeFailSafe("RetxTimer", value);
  SetRetxTimer(value.Get());
  //SetRetxTimer(Seconds(0.025));
  seq_contenuto = new std::map<std::string, uint32_t> ();
  num_rtx = new std::map<uint32_t, uint32_t> ();
  download_time = new std::map<std::string, DownloadEntry_Chunk> ();
  m_maxNumRtx = 100;      // Num Max Rtx
}

ConsumerRtx::~ConsumerRtx()
{
/*  delete seq_contenuto;
  delete num_rtx;
  delete download_time;

  if(request_file)
        request_file.close();

 NS_LOG_UNCOND("STOP APPLICATION"); 

  // cancel periodic packet generation
  Simulator::Cancel (m_sendEvent);

  // cleanup base stuff
  App::StopApplication ();
*/
}

void
ConsumerRtx::SetRetxTimer (Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning ())
    {
      // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
      Simulator::Remove (m_retxEvent); // slower, but better for memory
    }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &ConsumerRtx::CheckRetxTimeout, this);
}

Time
ConsumerRtx::GetRetxTimer () const
{
  return m_retxTimer;
}

void
ConsumerRtx::CheckRetxTimeout ()
{
  //NS_LOG_FUNCTION(Application::GetNode()->GetId() << "\t con CheckRetxTimer:\t" << m_retxTimer << "\t e Interest LifeTime:\t" << m_interestLifeTime);

  Time now = Simulator::Now ();

  Time rto = m_rtt->RetransmitTimeout ();
  // NS_LOG_DEBUG ("Current RTO: " << rto.ToDouble (Time::S) << "s");

  while (!m_seqTimeouts.empty ())
    {
      SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
        m_seqTimeouts.get<i_timestamp> ().begin ();
      if (entry->time + rto <= now) // timeout expired?
        {
          uint32_t seqNo = entry->seq;
          m_seqTimeouts.get<i_timestamp> ().erase (entry);
          OnTimeout (seqNo);
        }
      else
        break; // nothing else to do. All later packets need not be retransmitted
    }

  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &ConsumerRtx::CheckRetxTimeout, this);
}

// Application Methods
void
ConsumerRtx::StartApplication () // Called at time specified by Start
{
	  NS_LOG_FUNCTION_NOARGS ();

	  // do base stuff
	  App::StartApplication ();

	  uint32_t ID_NODO;
	  std::string PATH_PREFIX = "/media/DATI/tortelli/Simulazioni_BF_SpecialIssue/ndnSIM_Routing_NewBf/RICHIESTE/";

	  std::string PATH_SUFFIX;
	  // Metodo per recuperare le variabili globali settate con cmd line
	  DoubleValue av;
	  std::string adv = "g_alphaValue";
	  ns3::GlobalValue::GetValueByNameFailSafe(adv, av);
	  double alpha = av.Get();

	  NS_LOG_UNCOND("Il Valore di Alpha è:" << alpha);

	  if(alpha == 1)
		PATH_SUFFIX = "RICHIESTE_ALPHA_1/request_";
                //PATH_SUFFIX = "FILE_RICHIESTE_1/richieste";
	  else if (alpha == 12)
                PATH_SUFFIX = "RICHIESTE_ALPHA_12/request_";
	        //PATH_SUFFIX = "FILE_RICHIESTE_12/richieste";
	  else{
		NS_LOG_UNCOND("Valore di alpha non supportato");
		exit(0);
	  }

	  std::string EXT = ".txt";
	  std::stringstream ss;
	  Ptr<ns3::Node> node = Application::GetNode();

	// Metodo per recuperare le variabili globali settate con cmd line
	  UintegerValue st;
	  std::string sit = "g_simRun";
	  ns3::GlobalValue::GetValueByNameFailSafe(sit, st);
	  uint64_t sim = st.Get();


	  //ID_NODO = ((*node).GetId())%22;                                 // 21 is the maximum number of clients

          ID_NODO = (*node).GetId();
	  
          ss << PATH_PREFIX << PATH_SUFFIX << ID_NODO << "_" << sim << EXT;
	  std::string PERCORSO = ss.str();
	  const char *PATH_C = PERCORSO.c_str();
	  ss.str("");
	  request_file.open(PATH_C, std::fstream::in);
	  if(!request_file){
	  	  	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n" << PATH_C << "\t" << sim << "\t" << st.Get();
	 		exit(0);
	  }

	  ScheduleNextPacket ();
}

void
ConsumerRtx::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS ();

  delete seq_contenuto;
  delete num_rtx;
  delete download_time;

  if(request_file)
        request_file.close();

 NS_LOG_UNCOND("STOP APPLICATION"); 

  // cancel periodic packet generation
  Simulator::Cancel (m_sendEvent);

  // cleanup base stuff
  App::StopApplication ();
}


void
ConsumerRtx::SendPacket ()
{
  if (!m_active) return;

  NS_LOG_FUNCTION_NOARGS ();

  bool retransmit = false;

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid

  while (m_retxSeqs.size ())
    {
      seq = *m_retxSeqs.begin ();
      m_retxSeqs.erase (m_retxSeqs.begin ());
      retransmit = true;
      break;
    }

  if (seq == std::numeric_limits<uint32_t>::max ())             // Enter here if there is nothing to retransmit
    {
      if (m_seqMax != std::numeric_limits<uint32_t>::max ())
        {
          if (m_seq >= m_seqMax)
            {
              return; // we are totally done
            }
        }

      seq = m_seq++;
    }

  /* *** (MT) Retrieve the content name of the request ***
   *
   * The request file is accessed using the 'seq' number in order to retrieve the corresponding line. Each line is composed by:
   *
   * - /chunk_name/ + "expected_num_chunk",
   *
   * where chunk_name = name of the chunk
   *       exp_num_chunk = num of chunks of the content, for the first chunk, otherwise is 0.
   */

  GotoLine(request_file,(seq+1));
  std::string line;
  std::getline(request_file,line);

  if(line.empty())
    return;

  // (MT) If it is NOT a retransmission
  if(!retransmit)
  {
  	 //std::string intApp = contentChunk;
  
//	 m_interestApp(&line);
	 m_interestApp(&line, int64_t(0), "Tx");

  	 //App::OnInterestApp(&intApp);                 // APP-LEVEL tracing for the sent Interests
     std::map<std::string, uint32_t>::iterator it;
     it = seq_contenuto->find(line);

     if(it!=seq_contenuto->end())                 // Go next if the content to fetch has already been asked but not retrieve yet
     {
        m_rtt->IncrNext(SequenceNumber32 (seq), 1);
        ScheduleNextPacket ();
        return;
     }
     else     // Different content
     {
         //NS_LOG_UNCOND("Content to ask:\t" << contenutChunk << "\t with exp_num_chunks:\t" << expChunk_string);
         num_rtx->insert(std::pair<uint32_t, uint32_t> (seq,1));
         seq_contenuto->insert(std::pair<std::string,uint32_t>(line,seq));   // Bind the chunk name to the relative sequence number
     	 download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (line, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));
     }
  }

  else    //  (MT) It IS a retransmission; in this case, the new "sent_time" must be updated.
  {
	  if(download_time->find(line)!=download_time->end())
		  download_time->find(line)->second.sentTimeChunk_New = Simulator::Now();
	  else
		  NS_LOG_UNCOND("Impossible to find the retransmitted content in the map!!");
  }

  Ptr<Interest> interestHeader = Create<Interest>();
  interestHeader->SetName(Create<Name> (line));
  interestHeader->SetNonce               (m_rand.GetValue ());
  interestHeader->SetInterestLifetime    (m_interestLifeTime);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (*interestHeader);
  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());

  //WillSendOutInterest (seq);

  //FwHopCountTag hopCountTag;
  //packet->AddPacketTag (hopCountTag);

  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  m_transmittedInterests (interestHeader, this, m_face);

  m_rtt->SentSeq (SequenceNumber32 (seq), 1);

  if(retransmit)
	  m_timeOutTrace (&line, 0, "Rtx");         // APP-LEVEL Trace for retransmitted Interests
      // m_timeOutTrace (interestHeader);         // APP-LEVEL Trace for retransmitted Interests

  m_protocolHandler (packet);

  ScheduleNextPacket ();
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////


void
ConsumerRtx::OnContentObject (const Ptr<const ContentObject> &contentObject,
                               Ptr<Packet> payload)
{
  if (!m_active) return;

  App::OnContentObject (contentObject, payload); // tracing inside

  NS_LOG_FUNCTION (this << contentObject << payload);

  // (MT) Hop count info extraction
  uint32_t dist = contentObject->GetAdditionalInfo().GetHopCount();

  std::stringstream ss;
  ss << contentObject -> GetName();
  const std::string cont_ric = ss.str();
  std::string cont_ric_app = ss.str();
  uint32_t seq_ric = seq_contenuto->find(cont_ric)->second;   // Retrieve the sequence number associated to the received content
  ss.str("");

  if((seq_ric % 100) == 0)   // Print a log message every 100 chunks.

          NS_LOG_UNCOND("NODE:\t" << Application::GetNode()->GetId() << "\t APP-LAYER RECEIVED DATA:\t" << cont_ric << "\t" << Simulator::Now().GetMicroSeconds());

  seq_contenuto->erase(cont_ric);           // Erase the association content_name <-> seq number


  if(download_time->find(cont_ric)!=download_time->end())
  {
	  //int64_t tempo_invio = download_time_first->find(cont_ric)->second;
	  Time tempo_primo_invio = download_time->find(cont_ric)->second.sentTimeChunk_First;

	  Time tempo_ultimo_invio = download_time->find(cont_ric)->second.sentTimeChunk_New;

      Time tempo_download = (Simulator::Now() - tempo_ultimo_invio)+download_time->find(cont_ric)->second.incrementalTime;

	  // LE INFO DEL TEMPO DI DOWNLOAD E DEL HOP COUNT VENGONO INSERITE IN UN UNICO TRACING FILE.
      //m_downloadTime (&cont_ric_app, tempo_primo_invio.GetMicroSeconds(), tempo_download.GetMicroSeconds(), dist);
      m_downloadTime (&cont_ric_app, tempo_primo_invio.GetMicroSeconds(), tempo_download.GetMicroSeconds(), dist, "First");

      download_time->erase(cont_ric);
	  //download_time->erase(cont_ric);
  }
  else
  {
	  NS_LOG_UNCOND("CONTENUTO GIA' ELIMINATO DALLA STRUTTURA DOWNLOAD TIME");
	  //exit(0);
  }


  m_seqLifetimes.erase (seq_ric);
  m_seqTimeouts.erase (seq_ric);
  m_retxSeqs.erase (seq_ric);

  num_rtx->erase(seq_ric);

  m_rtt->AckSeq (SequenceNumber32 (seq_ric+1));
}

void
ConsumerRtx::OnNack (const Ptr<const Interest> &interest, Ptr<Packet> origPacket)
{
  if (!m_active) return;

  App::OnNack (interest, origPacket); // tracing inside

  // NS_LOG_DEBUG ("Nack type: " << interest->GetNack ());

  // NS_LOG_FUNCTION (interest->GetName ());

  // NS_LOG_INFO ("Received NACK: " << boost::cref(*interest));
  uint32_t seq = boost::lexical_cast<uint32_t> (interest->GetName ().GetComponents ().back ());
  NS_LOG_INFO ("< NACK for " << seq);
  // std::cout << Simulator::Now ().ToDouble (Time::S) << "s -> " << "NACK for " << seq << "\n";

  // put in the queue of interests to be retransmitted
  // NS_LOG_INFO ("Before: " << m_retxSeqs.size ());
  m_retxSeqs.insert (seq);
  // NS_LOG_INFO ("After: " << m_retxSeqs.size ());

  m_seqTimeouts.erase (seq);

  m_rtt->IncreaseMultiplier ();             // Double the next RTO ??
  ScheduleNextPacket ();
}

void
ConsumerRtx::OnTimeout (uint32_t sequenceNumber)
{
	  NS_LOG_FUNCTION (sequenceNumber);
	  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " << m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";
	  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tNumero ritrasmissione:\t" << num_rtx->find(sequenceNumber)->second << "\t Num Max:\t" << m_maxNumRtx << "\tTempo:\t" << Simulator::Now ());

	  // Recupero il nome del contenuto
	  GotoLine(request_file, (sequenceNumber+1));
	  std::string line_rto;
	  std::getline(request_file,line_rto);

	  // **** Calcolo il nuovo tempo incrementale per il chunk
	  Time new_increment = Simulator::Now() - download_time->find(line_rto)->second.sentTimeChunk_New;

	  download_time->find(line_rto)->second.incrementalTime+=new_increment;

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

	        m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1);                // make sure to disable RTT calculation for this sample
	        m_retxSeqs.insert (sequenceNumber);

	  }
	  else
	  {
	      Time tempo_invio = download_time->find(line_rto)->second.sentTimeChunk_First;
	     
              //m_numMaxRtx(&line_rto, tempo_invio.GetMicroSeconds());
	      m_numMaxRtx(&line_rto, tempo_invio.GetMicroSeconds(), "El");
	      
              download_time->erase(line_rto);
	      num_rtx->erase(sequenceNumber);
	      seq_contenuto->erase(line_rto);     /// **** MODIFICA
	      m_retxSeqs.erase(sequenceNumber);
	      m_seqLifetimes.erase (sequenceNumber);
	      m_seqTimeouts.erase (sequenceNumber);
	      m_rtt->AckSeq (SequenceNumber32 (sequenceNumber));
	  }

	  ScheduleNextPacket ();
}

void
ConsumerRtx::WillSendOutInterest (uint32_t sequenceNumber)
{
  NS_LOG_DEBUG ("Trying to add " << sequenceNumber << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");

  m_seqTimeouts.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));
  m_seqFullDelay.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));

  m_seqLastDelay.erase (sequenceNumber);
  m_seqLastDelay.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));

  m_seqRetxCounts[sequenceNumber] ++;

  m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1);
}

std::ifstream& ConsumerRtx::GotoLine(std::ifstream& file, uint32_t num) {
  file.seekg(std::ios::beg);
  for(uint32_t i=0; i < num - 1; ++i){
      file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
  }
  return file;
}

} // namespace ndn
} // namespace ns3

