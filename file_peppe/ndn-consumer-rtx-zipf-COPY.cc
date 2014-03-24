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

#include "ndn-consumer-rtx-zipf.h"
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

NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerRtxZipf");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (ConsumerRtxZipf);

TypeId
ConsumerRtxZipf::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerRtxZipf")
    .SetGroupName ("Ndn")
    .SetParent<App> ()
    .AddAttribute ("StartSeq", "Initial sequence number",
                   IntegerValue (0),
                   MakeIntegerAccessor(&ConsumerRtxZipf::m_seq),
                   MakeIntegerChecker<int32_t>())

    .AddAttribute ("Prefix","Name of the Interest",
                   StringValue ("/"),
                   MakeNameAccessor (&ConsumerRtxZipf::m_interestName),
                   MakeNameChecker ())
    .AddAttribute ("LifeTime", "LifeTime for interest packet",
                   StringValue ("2s"),
                   MakeTimeAccessor (&ConsumerRtxZipf::m_interestLifeTime),
                   MakeTimeChecker ())

    .AddAttribute ("RetxTimer",
                   "Timeout defining how frequent retransmission timeouts should be checked",
                   StringValue ("25ms"),
                   MakeTimeAccessor (&ConsumerRtxZipf::GetRetxTimer, &ConsumerRtxZipf::SetRetxTimer),
                   MakeTimeChecker ())

    .AddAttribute ("MaxNumRtx", "Maximum Number of Interest Retransmissions",
                   StringValue ("10"),
                   MakeUintegerAccessor(&ConsumerRtxZipf::m_maxNumRtx),
                   MakeUintegerChecker<int32_t>())

    .AddAttribute ("NumberOfContents", "Number of the Contents in total",
                   StringValue ("100"),
                   MakeUintegerAccessor (&ConsumerRtxZipf::SetNumberOfContents, &ConsumerRtxZipf::GetNumberOfContents),
                   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("q", "parameter of improve rank",
                    StringValue ("0.7"),
                    MakeDoubleAccessor (&ConsumerRtxZipf::SetQ, &ConsumerRtxZipf::GetQ),
                    MakeDoubleChecker<double> ())

    .AddAttribute ("s", "parameter of power",
                    StringValue ("0.7"),
                    MakeDoubleAccessor (&ConsumerRtxZipf::SetS, &ConsumerRtxZipf::GetS),
                    MakeDoubleChecker<double> ())


    //.AddTraceSource ("LastRetransmittedInterestDataDelay", "Delay between last retransmitted Interest and received Data",
    //                 MakeTraceSourceAccessor (&Consumer::m_lastRetransmittedInterestDataDelay))

    //.AddTraceSource ("FirstInterestDataDelay", "Delay between first transmitted Interest and received Data",
    //                 MakeTraceSourceAccessor (&Consumer::m_firstInterestDataDelay))

    .AddTraceSource ("InterestApp", "InterestApp",
                   MakeTraceSourceAccessor (&ConsumerRtxZipf::m_interestApp))

    .AddTraceSource ("TimeOutTrace", "TimeOutTrace",
                   MakeTraceSourceAccessor (&ConsumerRtxZipf::m_timeOutTrace))

    .AddTraceSource ("DownloadTime", "DownloadTime",
                   MakeTraceSourceAccessor (&ConsumerRtxZipf::m_downloadTime))

    .AddTraceSource ("DownloadTimeFile", "DownloadTimeFile",
                   MakeTraceSourceAccessor (&ConsumerRtxZipf::m_downloadTimeFile))

    .AddTraceSource ("UncompleteFile", "UncompleteFile",
                   MakeTraceSourceAccessor (&ConsumerRtxZipf::m_uncompleteFile))

    .AddTraceSource ("NumMaxRtx", "NumMaxRtx",
                   MakeTraceSourceAccessor (&ConsumerRtxZipf::m_numMaxRtx))



    ;

  return tid;
}

ConsumerRtxZipf::ConsumerRtxZipf ()
  : m_rand (0, std::numeric_limits<uint32_t>::max ())
  , m_seq (0)
  , m_seqMax (111000) // don't request anything
  , m_N (100) // needed here to make sure when SetQ/SetS are called, there is a valid value of N
  , m_q (0.7)
  , m_s (0.7)
  , m_SeqRng (0.0, 1.0)

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
  download_time_first = new std::map<std::string, DownloadEntry_Chunk> ();
  download_time_file = new std::map<std::string, DownloadEntry> ();
  m_maxNumRtx = 100;      // Num Max Rtx
}

ConsumerRtxZipf::~ConsumerRtxZipf()
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
ConsumerRtxZipf::SetNumberOfContents (uint32_t numOfContents)
{
  m_N = numOfContents;

  NS_LOG_DEBUG (m_q << " and " << m_s << " and " << m_N);

  m_Pcum = std::vector<double> (m_N + 1);

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
}

uint32_t
ConsumerRtxZipf::GetNumberOfContents () const
{
  return m_N;
}

void
ConsumerRtxZipf::SetQ (double q)
{
  m_q = q;
  SetNumberOfContents (m_N);
}

double
ConsumerRtxZipf::GetQ () const
{
  return m_q;
}

void
ConsumerRtxZipf::SetS (double s)
{
  m_s = s;
  SetNumberOfContents (m_N);
}

double
ConsumerRtxZipf::GetS () const
{
  return m_s;
}


void
ConsumerRtxZipf::SetRetxTimer (Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning ())
    {
      // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
      Simulator::Remove (m_retxEvent); // slower, but better for memory
    }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &ConsumerRtxZipf::CheckRetxTimeout, this);
}

Time
ConsumerRtxZipf::GetRetxTimer () const
{
  return m_retxTimer;
}

void
ConsumerRtxZipf::CheckRetxTimeout ()
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
                                     &ConsumerRtxZipf::CheckRetxTimeout, this);
}

// Application Methods
void
ConsumerRtxZipf::StartApplication () // Called at time specified by Start
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
ConsumerRtxZipf::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS ();

  delete seq_contenuto;
  delete num_rtx;
  delete download_time;
  delete download_time_file;
  delete download_time_first;

  if(request_file)
        request_file.close();

 NS_LOG_UNCOND("STOP APPLICATION"); 

  // cancel periodic packet generation
  Simulator::Cancel (m_sendEvent);

  // cleanup base stuff
  App::StopApplication ();
}


void
ConsumerRtxZipf::SendPacket ()
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

      //seq = m_seq++;
      seq = ConsumerRtxZipf::GetNextSeq();
      m_seq ++;

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
  std::string lineFull;
  std::getline(request_file,lineFull);

  if(lineFull.empty())
    return;

  const std::string line = lineFull;

  // Tokenize the string in order to separate content name from the number of chunks of that content
  std::stringstream ss;
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("\t");
  tokenizer tokens_space(line, sep);
  tokenizer::iterator it_space=tokens_space.begin();
  const std::string contentName = *it_space;           // It should be the content name without the chunk part
  it_space++;
  const std::string expContentChunks = *it_space;      // It should be the number of expected chunks for that content

  // NB *** Costruire il nome completo comprensivo della parte chunk.

  // ** If it is NOT a RETRANSMISSION
  if(!retransmit)
  {
  	 std::string intApp = contentName;     // NB: Sostituirlo con il nome completo (compresivo della parte chunk).
  	 	 	 	 	 	 	 	 	 	   //	  Così come tutti i contentName usati di seguito!!
  
	 m_interestApp(&intApp, int64_t(0), "Tx");  // APP-LEVEL Tracing for transmitted Interests.

     std::map<std::string, uint32_t>::iterator it;
     it = seq_contenuto->find(contentName);

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
         seq_contenuto->insert(std::pair<std::string,uint32_t>(contentName,seq));   // Bind the chunk name to the relative sequence number

     	 // NB: *** Sostituire la verifica del chunk atteso letto da file con quella del contatore che tiene traccia
         //         dei chunk chiesti e che dovranno essere chiesti. Questo dovrebbe evitare accessi inutili a file.

         const char* expChunk = expContentChunks.c_str();
     	 uint32_t numChunk = std::atoi(expChunk);
     	 if(numChunk != 0)     // If it is the FIRST CHUNK, all the structures must be updated: download_time_first,
     		 	 	 	 	   // download_time, and download_time_file.
     	 {
     		download_time_first->insert(std::pair<std::string,DownloadEntry_Chunk> (contentName, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));
     		download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (contentName, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));

     		// Cut the chunk part to obtain the name that shall be insterted into the download_time_file

     		Ptr<NameComponents> requestedContent = Create<NameComponents> (contentName);
     		uint32_t numComponents = requestedContent->GetComponents().size();
            std::list<std::string> fullName (numComponents);
			fullName.assign(requestedContent->begin(), requestedContent->end());
		    std::list<std::string>::iterator it;
		    it = fullName.begin();      // The iterator points to the first component

     		fullName.pop_back();     // Cut the chunk part
     		--numComponents;

      		for(uint32_t t = 0; t < numComponents; t++)
     		{
     			ss << "/" << *it;
     			it++;
     		}
     		const std::string contentWithoutChunk = ss.str();
     		ss.str("");

     		//**** NB: Nella mappa "download_time_file" inserisco solo il nome del contenuto senza il chunk
            download_time_file->insert(std::pair<std::string,DownloadEntry> (contentWithoutChunk, DownloadEntry(Simulator::Now(), Simulator::Now(),numChunk,0,0)) );

     	}
     	else
     	{
     		download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (contentName, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));
     	}
     }
  }

  else    //  If it IS a RETRANSMISSION, the "sent_time" must be updated.
  {
	  const char* expChunk = expContentChunks.c_str();
	  uint32_t numChunk = std::atoi(expChunk);
	  if(numChunk != 0)     // It is the FIRST Chunk that needs to be retransmitted
	  {
		  if(download_time_first->find(contentName)!=download_time_first->end())
			  download_time_first->find(contentName)->second.sentTimeChunk_New = Simulator::Now();
	  	  else
	  		  NS_LOG_UNCOND("Impossible to find the retransmitted content in the map!!");
	  }
	  else // It is NOT the FIRST Chunk that needs to be retransmitted
	  {
		  if(download_time->find(line)!=download_time->end())
			  download_time->find(line)->second.sentTimeChunk_New = Simulator::Now();
		  else
			  NS_LOG_UNCOND("Impossible to find the retransmitted content in the map!!");
	  }
  }

  Ptr<Interest> interestHeader = Create<Interest>();
  interestHeader->SetName(Create<Name> (contentName));
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
	  m_timeOutTrace (&contentName, 0, "Rtx");         // APP-LEVEL Trace for retransmitted Interests

  m_protocolHandler (packet);

  ScheduleNextPacket ();
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////


void
ConsumerRtxZipf::OnContentObject (const Ptr<const ContentObject> &contentObject,
                               Ptr<Packet> payload)
{
  if (!m_active) return;

  App::OnContentObject (contentObject, payload); // tracing inside

  NS_LOG_FUNCTION (this << contentObject << payload);

  // ** HOP COUNT info extraction
  uint32_t dist = contentObject->GetAdditionalInfo().GetHopCount();

  std::stringstream ss;
  ss << contentObject -> GetName();
  const std::string cont_ric = ss.str();
  std::string cont_ric_app = ss.str();

  // Retrieve the SEQ NUM associated to the received content
  uint32_t seq_ric = seq_contenuto->find(cont_ric)->second;
  ss.str("");

  // Print a LOG MSG every 100 chunks.
  if((seq_ric % 100) == 0)

          NS_LOG_UNCOND("NODE:\t" << Application::GetNode()->GetId() << "\t APP-LAYER RECEIVED DATA:\t" << cont_ric << "\t" << Simulator::Now().GetMicroSeconds());

  // Erase the association content_name <-> seq number
  seq_contenuto->erase(cont_ric);

  // *** The SEEK TIME should be calculated only concerning the FIRST Chunk.
  if(download_time_first->find(cont_ric)!=download_time_first->end())
  {
	  Time firstTxTime = download_time_first->find(cont_ric)->second.sentTimeChunk_First;

	  Time lastTxTime = download_time_first->find(cont_ric)->second.sentTimeChunk_New;

      Time downloadTime = (Simulator::Now() - lastTxTime)+download_time_first->find(cont_ric)->second.incrementalTime;

      // It is the same Tracing File both for the Download Time and for the Hop Count
      m_downloadTime (&cont_ric_app, firstTxTime.GetMicroSeconds(), downloadTime.GetMicroSeconds(), dist, "First");

      download_time_first->erase(cont_ric);
  }

  // Checking the received Chunk; Update the structure "download_time_file" and, eventually, calculate the file download time

  Ptr<NameComponents> receivedContent = Create<NameComponents> (cont_ric);
  uint32_t numComponents = receivedContent->GetComponents().size();
  std::list<std::string> nameReceivedContent (numComponents);
  nameReceivedContent.assign(receivedContent->begin(), receivedContent->end());
  std::list<std::string>::iterator it = nameReceivedContent.end();

  --it;
  // Extract the chunk part of the name.
  std::string chunkString = *it;

  it = nameReceivedContent.begin();
  nameReceivedContent.pop_back();
  --numComponents;

  for(uint32_t t = 0; t < numComponents; t++)
  {
	ss << "/" << *it;
	it++;
  }
  std::string contentWithoutChunk = ss.str();
  ss.str("");

  // Control if the respective content of the received chunk has been already deleted from the structure.
  // This can happen when one ore more chunks are received after the last chunk.
  if(download_time_file->find(contentWithoutChunk)!=download_time_file->end())
  {
	  uint32_t totNumChunks = download_time_file->find(contentWithoutChunk)->second.expNumChunk;
      std::string checkChunk = "s_";
      ss << checkChunk << (totNumChunks-1);
      checkChunk = ss.str();
      if (chunkString.compare(checkChunk) != 0)    // The received chunk does not correspond to the last one; so, only the counter of received chunk is incremented.
	  	  	  	  	  	                           // If exactly the last chunk is lost, the respective contents will stay into the download_time_file structure.
      {
    	  download_time_file->find(contentWithoutChunk)->second.rcvNumChunk++;

    	  // ** The download time of the single chunk is calculated and added to the sum the refers to the respective content
    	  Time incrementTime = Simulator::Now() - download_time->find(cont_ric)->second.sentTimeChunk_New;
    	  if(incrementTime > NanoSeconds(100000000)) // vuol dire che la ritrasmissione schedulata non √® stata ancora effettuata.
    		  incrementTime = NanoSeconds(100000000) + download_time->find(cont_ric)->second.incrementalTime;
          else
        	  incrementTime = (Simulator::Now() - download_time->find(cont_ric)->second.sentTimeChunk_New)+download_time->find(cont_ric)->second.incrementalTime;

	      download_time_file->find(contentWithoutChunk)->second.downloadTime+=incrementTime;
          download_time_file->find(contentWithoutChunk)->second.distance+=dist;
	      download_time->erase(cont_ric);
      }
      else  // The received chunk is the LAST one. If rcvChunks == expChunks --> calculate Download Time;
    	    // otherwise, the content is marked as lost.
      {
    	  if(download_time_file->find(contentWithoutChunk)->second.rcvNumChunk == (totNumChunks - 1))
	      {
        	  download_time_file->find(contentWithoutChunk)->second.rcvNumChunk++;

        	  // ** The download time of the single chunk is calculated and added to the sum the refers to the respective content
        	  Time incrementTime = Simulator::Now() - download_time->find(cont_ric)->second.sentTimeChunk_New;
        	  if(incrementTime > NanoSeconds(100000000)) // vuol dire che la ritrasmissione schedulata non √® stata ancora effettuata.
        		  incrementTime = NanoSeconds(100000000) + download_time->find(cont_ric)->second.incrementalTime;
              else
            	  incrementTime = (Simulator::Now() - download_time->find(cont_ric)->second.sentTimeChunk_New)+download_time->find(cont_ric)->second.incrementalTime;

    	      download_time_file->find(contentWithoutChunk)->second.downloadTime+=incrementTime;
              download_time_file->find(contentWithoutChunk)->second.distance+=dist;
    	      download_time->erase(cont_ric);

		      Time downloadTimeFinal = download_time_file->find(contentWithoutChunk)->second.downloadTime;
              Time firstChunkTime = download_time_file->find(contentWithoutChunk)->second.sentTimeFirst;
              uint32_t meanHitDistance = round(download_time_file->find(contentWithoutChunk)->second.distance / totNumChunks);

              m_downloadTime (&contentWithoutChunk, firstChunkTime.GetMicroSeconds(), downloadTimeFinal.GetMicroSeconds(), meanHitDistance, "File");

              download_time_file->erase(contentWithoutChunk);
	      }
    	  else   // One or more chunks have been not received; so the entire content is marked as LOST
    	  {
    		  Time firstChunkTime = download_time_file->find(contentWithoutChunk)->second.sentTimeFirst;
		      download_time_file->erase(contentWithoutChunk);
		      download_time->erase(cont_ric);

		      // APP-LEVEL Tracing of Incomplete Files
		      m_uncompleteFile(&contentWithoutChunk, firstChunkTime.GetMicroSeconds(),"Unc");
    	  }
      }
  }
  else
  {
	  download_time->erase(cont_ric);
  }


  m_seqLifetimes.erase (seq_ric);
  m_seqTimeouts.erase (seq_ric);
  m_retxSeqs.erase (seq_ric);

  num_rtx->erase(seq_ric);

  m_rtt->AckSeq (SequenceNumber32 (seq_ric+1));
}

void
ConsumerRtxZipf::OnNack (const Ptr<const Interest> &interest, Ptr<Packet> origPacket)
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
ConsumerRtxZipf::OnTimeout (uint32_t sequenceNumber)
{
	  NS_LOG_FUNCTION (sequenceNumber);
	  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tNumero ritrasmissione:\t" << num_rtx->find(sequenceNumber)->second << "\t Num Max:\t" << m_maxNumRtx << "\tTempo:\t" << Simulator::Now ());

	  // Retrieve the correspondent line
	  GotoLine(request_file, (sequenceNumber+1));
	  std::string line_rto;
	  std::getline(request_file,line_rto);

	  // Retrieve the content name
	  std::stringstream ss_rto;
	  typedef boost::tokenizer<boost::char_separator<char> > tokenizer_rto;
	  boost::char_separator<char> sep_rto("\t");
	  tokenizer_rto tokens_space_rto(line_rto, sep_rto);
	  tokenizer_rto::iterator it_space_rto=tokens_space_rto.begin();
	  std::string contentNameRto = *it_space_rto;
	  it_space_rto++;
	  const std::string expChunkRto = *it_space_rto;
	  const char* expChunksRto = expChunkRto.c_str();
	  uint32_t numChunkRto = std::atoi(expChunksRto);

	  // ** Calculate and Update the new Incremental Time for the respective chunk
	  Time new_increment = Simulator::Now() - download_time->find(contentNameRto)->second.sentTimeChunk_New;

	  if(numChunkRto != 0)     // It is the First Chunk.
		  download_time_first->find(contentNameRto)->second.incrementalTime+=new_increment;
	  else
		  download_time->find(contentNameRto)->second.incrementalTime+=new_increment;

	  // ** Check if another Retransmission is Allowed.
	  if(num_rtx->find(sequenceNumber)->second < m_maxNumRtx)
	  {
	        // Increment the Rtx Counter (with another check of the presence of the element)
	        std::pair<std::map<uint32_t,uint32_t>::iterator,bool> ret;
	        ret=num_rtx->insert (std::pair<uint32_t,uint32_t>(sequenceNumber,1) );
	        if(ret.second==false)
	        {
	        	uint32_t counter = num_rtx->find(sequenceNumber)->second;
	            num_rtx->operator[](sequenceNumber)= counter+1;
	        }
	        else
	        {
	            NS_LOG_UNCOND("ERROR INSIDE THE RTX MAP! ELEMENT NOT FOUND");
	        }

	        m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1);                // make sure to disable RTT calculation for this sample
	        m_retxSeqs.insert (sequenceNumber);

	  }
	  else
	  {
	        // Retrieve the Content Name
	        GotoLine(request_file, (sequenceNumber+1));
	        std::string line_rtr;
	        std::getline(request_file,line_rtr);

	        std::stringstream ss_rtr;
	        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_rtr;
	        boost::char_separator<char> sep_rtr("\t");
	        tokenizer_rtr tokens_space_rtr(line_rtr, sep_rtr);
	        tokenizer_rtr::iterator it_space_rtr=tokens_space_rtr.begin();
	        std::string contentChunkRtr = *it_space_rtr;

	        //  Devo eliminare il contenuto non recuperato qualora stessi eliminando, dopo 100 ritrasmissioni, l'ultimo chunk. Altrimenti,
	        //  la voce in Download_File rimarrebbe.

	        Ptr<NameComponents> receivedContent = Create<NameComponents> (contentChunkRtr);
	        uint32_t numComponents = receivedContent->GetComponents().size();

	        std::list<std::string> nameRicContent (numComponents);
	        nameRicContent.assign(receivedContent->begin(), receivedContent->end());
	        std::list<std::string>::iterator it = nameRicContent.end();

	        --it;
	        std::string chunkString = *it;
	        it = nameRicContent.begin();
	        nameRicContent.pop_back();
	        --numComponents;

	        std::stringstream ss;
	        for(uint32_t t = 0; t < numComponents; t++)
	        {
		        ss << "/" << *it;
		        it++;
	        }
	        std::string contenutWithoutChunk = ss.str();
	        ss.str("");

	        uint32_t numChunkTot = download_time_file->find(contenutWithoutChunk)->second.expNumChunk;
	        std::string checkChunk = "s_";
	        ss << checkChunk << (numChunkTot-1);
	        checkChunk = ss.str();
	        if (chunkString.compare(checkChunk) == 0)  // I'm eliminating the last chunk of the content; so, the corespondent file is erased
	                                                   // too from the structure file_download_time.
	                download_time_file->erase(contenutWithoutChunk);

	        Time firstTxTime = download_time->find(contentChunkRtr)->second.sentTimeChunk_First;
	     
	        // APP-LEVEL Tracing for eliminated files
	        m_numMaxRtx(&contentChunkRtr, firstTxTime.GetMicroSeconds(), "El");
	      
            download_time->erase(contentChunkRtr);
	        num_rtx->erase(sequenceNumber);
	        seq_contenuto->erase(contentChunkRtr);
	        m_retxSeqs.erase(sequenceNumber);
	        m_seqLifetimes.erase (sequenceNumber);
	        m_seqTimeouts.erase (sequenceNumber);
	        m_rtt->AckSeq (SequenceNumber32 (sequenceNumber));
	        if(download_time_first->find(contentChunkRtr)!= download_time_first->end())     /// **** MODIFICA
	        	download_time_first->erase(contentChunkRtr);
	  }
	  ScheduleNextPacket ();
}

void
ConsumerRtxZipf::WillSendOutInterest (uint32_t sequenceNumber)
{
  NS_LOG_DEBUG ("Trying to add " << sequenceNumber << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");

  m_seqTimeouts.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));
  m_seqFullDelay.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));

  m_seqLastDelay.erase (sequenceNumber);
  m_seqLastDelay.insert (SeqTimeout (sequenceNumber, Simulator::Now ()));

  m_seqRetxCounts[sequenceNumber] ++;

  m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1);
}

uint32_t
ConsumerRtxZipf::GetNextSeq()
{
  uint32_t content_index = 1; //[1, m_N]
  double p_sum = 0;

  double p_random = m_SeqRng.GetValue();
  while (p_random == 0)
    {
      p_random = m_SeqRng.GetValue();
    }
  //if (p_random == 0)
  NS_LOG_LOGIC("p_random="<<p_random);
  for (uint32_t i=1; i<=m_N; i++)
    {
      p_sum = m_Pcum[i];   //m_Pcum[i] = m_Pcum[i-1] + p[i], p[0] = 0;   e.g.: p_cum[1] = p[1], p_cum[2] = p[1] + p[2]
      if (p_random <= p_sum)
        {
          content_index = i;
          break;
        } //if
    } //for
  //content_index = 1;
  NS_LOG_DEBUG("RandomNumber="<<content_index);
  return content_index;
}


std::ifstream& ConsumerRtxZipf::GotoLine(std::ifstream& file, uint32_t num) {
  file.seekg(std::ios::beg);
  for(uint32_t i=0; i < num - 1; ++i){
      file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
  }
  return file;
}

} // namespace ndn
} // namespace ns3

