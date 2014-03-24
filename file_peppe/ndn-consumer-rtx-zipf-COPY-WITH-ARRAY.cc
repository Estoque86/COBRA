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

    .AddAttribute ("NumberOfContentsZipf", "Number of the Contents in total",
                   StringValue ("100"),
                   MakeUintegerAccessor (&ConsumerRtxZipf::SetNumberOfContents, &ConsumerRtxZipf::GetNumberOfContents),
                   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("qZipf", "parameter of improve rank",
                    StringValue ("0.7"),
                    MakeDoubleAccessor (&ConsumerRtxZipf::SetQ, &ConsumerRtxZipf::GetQ),
                    MakeDoubleChecker<double> ())

    .AddAttribute ("sZipf", "parameter of power",
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
  , m_seq(0)       // Serve come indice incrementale
  , m_currentChunk(0)
  , m_seqMax (111000) // don't request anything
  , m_N (100) // needed here to make sure when SetQ/SetS are called, there is a valid value of N
  , m_q (0.7)
  , m_s (0.7)
  , m_SeqRng (0.0, 1.0)

{
  NS_LOG_FUNCTION_NOARGS ();

  //m_seq->push_back(m_seq_start);        // The first element is the Sequence Number;
  //m_seq->push_back(0);       			// The second element is the Chunk Number.

  m_rtt = CreateObject<RttMeanDeviation> ();
  TimeValue value;
  this->GetAttributeFailSafe("RetxTimer", value);
  SetRetxTimer(value.Get());
  //SetRetxTimer(Seconds(0.025));
  seq_contenuto = new std::map<std::string, std::vector<uint32_t>* > ();
  num_rtx = new std::map<std::vector<uint32_t>*, uint32_t> ();
  contentInfoSeqNum = new std::map<std::vector<uint32_t>*, uint32_t> ();
  download_time = new std::map<std::string, DownloadEntry_Chunk> ();
  download_time_first = new std::map<std::string, DownloadEntry_Chunk> ();
  download_time_file = new std::map<std::string, DownloadEntry> ();
  m_maxNumRtx = 100;      // Num Max Rtx

  NS_LOG_UNCOND("COSTRUTTORE APP!!");
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

  NS_LOG_UNCOND("CHECK RETX TO - APP\t" << Simulator::Now());

  Time now = Simulator::Now ();

  Time rto = m_rtt->RetransmitTimeout ();

  NS_LOG_UNCOND("CHECK RETX TO - APP: Current RTO\t" << rto);
  // NS_LOG_DEBUG ("Current RTO: " << rto.ToDouble (Time::S) << "s");

  while (!m_seqTimeouts.empty ())
    {
      SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
        m_seqTimeouts.get<i_timestamp> ().begin ();
      NS_LOG_UNCOND("CHECK RETX TO - APP: Timestamp entry\t" << entry->time);
      if (entry->time + rto <= now) // timeout expired?
        {
          //uint32_t seqNo = entry->seq->operator[](0);
          //m_seqTimeouts.get<i_timestamp> ().erase (entry);
          //OnTimeout (seqNo);

          SeqTimeoutsContainer::index<i_seq>::type::iterator entrySeq =
            m_seqTimeouts.get<i_seq> ().begin ();

          //std::vector<uint32_t>* seqNo = (entry->seq);
          m_seqTimeouts.get<i_timestamp> ().erase (entry);

          //uint32_t contID = seqNo->operator[](0);
          NS_LOG_UNCOND("APP: ON TIMEOUT!" << Simulator::Now());

          //OnTimeout (*temp);
          //OnTimeout(*entry->seq);
          OnTimeout(*entrySeq->seq);

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

	  NS_LOG_UNCOND("START APPLICATION!! ");

	  // do base stuff
	  App::StartApplication ();

	  uint32_t ID_NODO;
	  std::stringstream ss;

	  std::string PATH_PREFIX = "/Users/michele/Desktop/Dottorato/Parigi/LINCS/COBRA/Simulatore/COBRA_SIM/RICHIESTE/";

	  // Metodo per recuperare le variabili globali settate con cmd line
	  // DoubleValue av;
	  // std::string adv = "g_alphaValue";
	  // ns3::GlobalValue::GetValueByNameFailSafe(adv, av);
	  // double alpha = av.Get();

	  // NS_LOG_UNCOND("Il Valore di Alpha �:" << alpha);

	  std::string EXT = ".txt";

	  ss << PATH_PREFIX << "request_PROVA" << EXT;

	  std::string PERCORSO = ss.str();
	  const char *PATH_C = PERCORSO.c_str();
	  ss.str("");
	  request_file.open(PATH_C, std::fstream::in);
	  if(!request_file){
	  	  	std::cout << "\nERRORE: Impossibile leggere dal file dei contenuti!\n" << PATH_C ;
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
  delete contentInfoSeqNum;
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
  bool newContent = false;   // Used to differentiate the request of a new content instead of a new
  	  	  	  	  	  	  	 // chunk of the same content.
  //NS_LOG_UNCOND("SEND PACKET!!");
  uint32_t contentID=std::numeric_limits<uint32_t>::max (); //invalid
  uint32_t chunkNum;

  while (m_retxSeqs.size ())
  {
	  NS_LOG_UNCOND("SEND PACKET - APP: RETX QUEUE SIZE\t" << m_retxSeqs.size () << "\t" << Simulator::Now());
	  std::set< std::vector<uint32_t>* >::iterator it;
	  it = m_retxSeqs.begin();
      //seq = *m_retxSeqs.begin ();
	  const std::vector<uint32_t>* temp = (*it);
	  contentID = temp->operator[](0);
	  chunkNum = temp->operator[](1);
	  NS_LOG_UNCOND("SEND PACKET - APP: RTX for contID: " << contentID << "\t and ChunkNum: " << chunkNum);
      //m_retxSeqs.erase (m_retxSeqs.begin ());
	  m_retxSeqs.erase (it);
      retransmit = true;
      break;
  }

  if (contentID == std::numeric_limits<uint32_t>::max ())             // Enter here if there is nothing to retransmit
  {
	  if (m_seqMax != std::numeric_limits<uint32_t>::max ())
      {
		  if (m_seq >= m_seqMax)
          {
			  return; // we are totally done
          }
      }
      //seq = m_seq++;
	  if(m_currentChunk==0)            // It means that a new content needs to be requested.
	  {
		  contentID = ConsumerRtxZipf::GetNextSeq();
		  m_currentContentID = contentID;
		  newContent = true;
		  NS_LOG_UNCOND("SEND PACKET - APP: EXTRACTED CONTENT ID\t" << m_currentContentID << "\t" << Simulator::Now());
	  }
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

  std::string contentName, chunkStr, fullName;
  std::stringstream ss;

  if(newContent || retransmit)
  {
	  GotoLine(request_file,(contentID));
	  std::string lineFull;
	  std::getline(request_file,lineFull);

	  //NS_LOG_UNCOND("APP SENDING NEW INTEREST: " << lineFull);

	  if(lineFull.empty())
		  return;

	  const std::string line = lineFull;

	  // Tokenize the string in order to separate content name from the number of chunks of that content
	  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	  boost::char_separator<char> sep("\t");
	  tokenizer tokens_space(line, sep);
	  tokenizer::iterator it_space=tokens_space.begin();
	  contentName = *it_space;           // It should be the content name without the chunk part
	  if(!retransmit)     // It means that a new content is being requested
	  {
		  it_space++;
		  const std::string expContentChunks = *it_space;      // It should be the number of expected chunks for that content
		  m_expChunk = std::atoi(expContentChunks.c_str());
		  m_currentContentName = contentName;
		  ss << m_currentChunk;      // m_currentChunk should be already to '0'
		  chunkStr = ss.str();
		  ss.str("");
	  }
	  else
	  {
		  ss << chunkNum;
		  chunkStr = ss.str();   // Assign the chunk num calculated before.
		  ss.str("");
	  }
  }
  else   // Another chunk of the current content is going to be requested.
  {
	  contentName = m_currentContentName;
	  ss << m_currentChunk;
	  chunkStr = ss.str();
	  ss.str("");
  }



  // Build the complete content name

  std::string chunkStrCompl;
  ss << "s_" << chunkStr;
  chunkStrCompl = ss.str();
  ss.str("");

  Ptr<NameComponents> mainName = Create<NameComponents> (contentName);
  uint32_t numComponents = mainName->GetComponents().size();
  std::list<std::string> mainNameLst (numComponents);
  mainNameLst.assign(mainName->begin(), mainName->end());
  mainNameLst.push_back(chunkStrCompl);
  ++numComponents;
  std::list<std::string>::iterator it = mainNameLst.begin();
  for(uint32_t t = 0; t < numComponents; t++)
  {
        ss << "/" << *it;
        it++;
  }
  fullName = ss.str();
  ss.str("");


  //NS_LOG_UNCOND("APP: Sending a New Interest:\t" << fullName);


  // ** If it is NOT a RETRANSMISSION
  if(!retransmit)
  {
	  NS_LOG_UNCOND("SEND PACKET - APP: Sending NEW Content\t" << fullName << "\t" << Simulator::Now());


  	 std::string intApp = fullName;

  	 std::string nodeType = this->GetNode()->GetObject<ForwardingStrategy>()->GetNodeType();
  
	 m_interestApp(&intApp, int64_t(0), "TX", nodeType);  // APP-LEVEL Tracing for transmitted Interests.

     std::map<std::string, std::vector<uint32_t>* >::iterator it;
     it = seq_contenuto->find(fullName);

     if(it!=seq_contenuto->end())                 // Go next if the content to fetch has already been asked but not retrieve yet
     {
        m_rtt->IncrNext(SequenceNumber32 (m_seq), 1);    // NB: Verify if it's "m_seq-1" or "m_seq".
        ScheduleNextPacket ();
        return;
     }
     else     // Different content
     {
         //NS_LOG_UNCOND("Content to ask:\t" << contenutChunk << "\t with exp_num_chunks:\t" << expChunk_string);
         std::vector<uint32_t> *p;
         p = new std::vector<uint32_t>(2);
         p->operator[](0) = (m_currentContentID);
         p->operator[](1) = (m_currentChunk);
    	 num_rtx->insert(std::pair<std::vector<uint32_t>*, uint32_t> (p,1));

         seq_contenuto->insert(std::pair<std::string,std::vector<uint32_t>* > (fullName, p));   // Bind the chunk name to the relative sequence number
         contentInfoSeqNum->insert(std::pair<std::vector<uint32_t>*, uint32_t> (p,m_seq));

     	 if(m_currentChunk == 0)     // If it is the FIRST CHUNK, all the structures must be updated: download_time_first,
     		 	 	 	 	   // download_time, and download_time_file.
     	 {
     		download_time_first->insert(std::pair<std::string,DownloadEntry_Chunk> (fullName, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));
     		download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (fullName, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));

     		// In the 'download_time_file' map, only the content name is inserted (without the chunk part).
            download_time_file->insert(std::pair<std::string,DownloadEntry> (contentName, DownloadEntry(Simulator::Now(), Simulator::Now(),m_expChunk,0,0)) );
     	 }
     	 else
     	 {
     		download_time->insert(std::pair<std::string,DownloadEntry_Chunk> (fullName, DownloadEntry_Chunk(Simulator::Now(), Simulator::Now(), MicroSeconds(0))));
     	 }
     }

  }

  else    //  If it IS a RETRANSMISSION, the "sent_time" must be updated.
  {
      //NS_LOG_UNCOND("APP: Sending a RETRANSMISSION:\t" << fullName);

	  NS_LOG_UNCOND("SEND PACKET - APP: Sending a RETRANSMISSION\t" << fullName << "\t" << Simulator::Now());

	  if(chunkNum != 0)     // It is the FIRST Chunk that needs to be retransmitted
	  {
		  if(download_time_first->find(fullName)!=download_time_first->end())
			  download_time_first->find(fullName)->second.sentTimeChunk_New = Simulator::Now();
	  	  else
	  		  NS_LOG_UNCOND("Impossible to find the retransmitted content in the map!!");
	  }
	  else // It is NOT the FIRST Chunk that needs to be retransmitted
	  {
		  if(download_time->find(fullName)!=download_time->end())
			  download_time->find(fullName)->second.sentTimeChunk_New = Simulator::Now();
		  else
			  NS_LOG_UNCOND("Impossible to find the retransmitted content in the map!!");
	  }
  }

  Ptr<Interest> interestHeader = Create<Interest>();
  interestHeader->SetName(Create<Name> (fullName));
  interestHeader->SetNonce               (m_rand.GetValue ());
  interestHeader->SetInterestLifetime    (m_interestLifeTime);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (*interestHeader);
  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());

  //WillSendOutInterest (seq);

  //FwHopCountTag hopCountTag;
  //packet->AddPacketTag (hopCountTag);

  std::vector<uint32_t> *p;
  p = new std::vector<uint32_t>(2);
  p->operator[](0) = (m_currentContentID);
  p->operator[](1) = (m_currentChunk);

  if(!retransmit)
  {
	  ++m_currentChunk;
	  if(m_currentChunk == m_expChunk)   // The last chunk of the current content is going to be requested, so I need
		  m_currentChunk = 0; 			   // to set m_currentChunk = 0 in order to request a new content in the next iteration
  }

  m_seqTimeouts.insert (SeqTimeout (p, Simulator::Now ()));
  m_seqLifetimes.insert (SeqTimeout (p, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  m_transmittedInterests (interestHeader, this, m_face);

  m_rtt->SentSeq (SequenceNumber32 (m_seq), 1);     // NB: Verify if it's "m_seq-1" or "m_seq".

  if(retransmit)
  {
	  std::string nodeType = this->GetNode()->GetObject<ForwardingStrategy>()->GetNodeType();
	  m_timeOutTrace (&fullName, 0, "RTX", nodeType);         // APP-LEVEL Trace for retransmitted Interests
  }


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

  NS_LOG_UNCOND("ON DATA - APP: Received CONTENT OBJECT\t" << cont_ric << "\t" << Simulator::Now());


  // Retrieve the ContentID and the ChunkNumber associated to the received content
  std::vector<uint32_t>* contentInfo = seq_contenuto->find(cont_ric)->second;
  ss.str("");

  uint32_t seqNumRic = contentInfoSeqNum->find(contentInfo)->second;   // Sequential seq num associated to contentInfo

  uint32_t contentID = contentInfo->operator[](0);
  uint32_t chunkNum = contentInfo->operator[](1);

  NS_LOG_UNCOND("ON DATA - APP: Received CONTENT OBJECT with SeqNum\t" << seqNumRic << "\tand content ID: "<< contentID << "\tand chunk Num: "<< chunkNum << "\t" << Simulator::Now());


  // Print a LOG MSG every 100 chunks.
  if((seqNumRic % 100) == 0)

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
	  std::string nodeType = this->GetNode()->GetObject<ForwardingStrategy>()->GetNodeType();
      m_downloadTime (&cont_ric_app, firstTxTime.GetMicroSeconds(), downloadTime.GetMicroSeconds(), dist, "FIRST", nodeType);

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
    	  if(incrementTime > NanoSeconds(100000000)) // vuol dire che la ritrasmissione schedulata non è stata ancora effettuata.
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
        	//  if(incrementTime > NanoSeconds(100000000)) // vuol dire che la ritrasmissione schedulata non è stata ancora effettuata.
        	//	  incrementTime = NanoSeconds(100000000) + download_time->find(cont_ric)->second.incrementalTime;
            //  else
            //	  incrementTime = (Simulator::Now() - download_time->find(cont_ric)->second.sentTimeChunk_New)+download_time->find(cont_ric)->second.incrementalTime;

    	      download_time_file->find(contentWithoutChunk)->second.downloadTime+=incrementTime;
              download_time_file->find(contentWithoutChunk)->second.distance+=dist;
    	      download_time->erase(cont_ric);

		      Time downloadTimeFinal = download_time_file->find(contentWithoutChunk)->second.downloadTime;
              Time firstChunkTime = download_time_file->find(contentWithoutChunk)->second.sentTimeFirst;
              uint32_t meanHitDistance = round(download_time_file->find(contentWithoutChunk)->second.distance / totNumChunks);

        	  std::string nodeType = this->GetNode()->GetObject<ForwardingStrategy>()->GetNodeType();
              m_downloadTimeFile (&contentWithoutChunk, firstChunkTime.GetMicroSeconds(), downloadTimeFinal.GetMicroSeconds(), meanHitDistance, "FILE", nodeType);

              download_time_file->erase(contentWithoutChunk);
	      }
    	  else   // One or more chunks have been not received; so the entire content is marked as LOST
    	  {
    		  Time firstChunkTime = download_time_file->find(contentWithoutChunk)->second.sentTimeFirst;
		      download_time_file->erase(contentWithoutChunk);
		      download_time->erase(cont_ric);

		      // APP-LEVEL Tracing of Incomplete Files
			  std::string nodeType = this->GetNode()->GetObject<ForwardingStrategy>()->GetNodeType();
		      m_uncompleteFile(&contentWithoutChunk, firstChunkTime.GetMicroSeconds(),"ELMFILE", nodeType);
    	  }
      }
  }
  else
  {
	  download_time->erase(cont_ric);
  }


  m_seqLifetimes.erase (contentInfo);

  NS_LOG_UNCOND("ON DATA - APP: SEQ TIMEOUT SIZE BEFORE\t" << m_seqTimeouts.size() << "\t" << Simulator::Now());
  m_seqTimeouts.erase (contentInfo);
  NS_LOG_UNCOND("ON DATA - APP: SEQ TIMEOUT SIZE AFTER\t" << m_seqTimeouts.size() << "\t" << Simulator::Now());


  m_retxSeqs.erase (contentInfo);

  num_rtx->erase(contentInfo);

  m_rtt->AckSeq (SequenceNumber32 (seqNumRic+1));
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
  std::vector<uint32_t> p;
  p.push_back(0);
  p.push_back(0);

  m_retxSeqs.insert (&p);
  // NS_LOG_INFO ("After: " << m_retxSeqs.size ());

  m_seqTimeouts.erase (&p);

  m_rtt->IncreaseMultiplier ();             // Double the next RTO ??
  ScheduleNextPacket ();
}

//void
//ConsumerRtxZipf::OnTimeout (uint32_t sequenceNumber)
void
ConsumerRtxZipf::OnTimeout (std::vector<uint32_t>& seqNum)
{
	  //NS_LOG_FUNCTION (sequenceNumber);
	  //NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\tNumero ritrasmissione:\t" << num_rtx->find(sequenceNumber)->second << "\t Num Max:\t" << m_maxNumRtx << "\tTempo:\t" << Simulator::Now ());

	  // Retrieve the correspondent line
	  uint32_t sequenceNumber = seqNum.operator[](0);
	  uint32_t chunkNumber = seqNum.operator[](1);

	  NS_LOG_UNCOND("APP - RTX: Sequence number: " << sequenceNumber);
	  NS_LOG_UNCOND("APP - RTX: Chunk number: " << chunkNumber);

	  GotoLine(request_file, (sequenceNumber));
	  std::string line_rto;
	  std::getline(request_file,line_rto);

	  // Retrieve the content name
	  std::stringstream ss_rto;
	  typedef boost::tokenizer<boost::char_separator<char> > tokenizer_rto;
	  boost::char_separator<char> sep_rto("\t");
	  tokenizer_rto tokens_space_rto(line_rto, sep_rto);
	  tokenizer_rto::iterator it_space_rto=tokens_space_rto.begin();
	  std::string contentNameRto = *it_space_rto;

	  // Compose the full name with the chunk part
	  std::string chunkStr;
	  ss_rto << "s_" << chunkNumber;
	  chunkStr = ss_rto.str();
	  ss_rto.str("");

      Ptr<NameComponents> rtoContent = Create<NameComponents> (contentNameRto);
      uint32_t numComponents = rtoContent->GetComponents().size();
      std::list<std::string> nameRtoContent (numComponents);
      nameRtoContent.assign(rtoContent->begin(), rtoContent->end());
      nameRtoContent.push_back(chunkStr);
      ++numComponents;
      std::list<std::string>::iterator it = nameRtoContent.begin();
      for(uint32_t t = 0; t < numComponents; t++)
      {
	        ss_rto << "/" << *it;
	        it++;
      }
      std::string fullNameRto = ss_rto.str();
      ss_rto.str("");

      NS_LOG_UNCOND("APP - FULL NAME CONTENT RTO: " << fullNameRto);


	  // ** Calculate and Update the new Incremental Time for the respective chunk
	  Time new_increment = Simulator::Now() - download_time->find(fullNameRto)->second.sentTimeChunk_New;

	  if(chunkNumber != 0)     // It is the First Chunk.
		  download_time_first->find(fullNameRto)->second.incrementalTime+=new_increment;
	  else
		  download_time->find(fullNameRto)->second.incrementalTime+=new_increment;

	  // ** Check if another Retransmission is Allowed.
	  if(num_rtx->find(&seqNum)->second < m_maxNumRtx)
	 //if(num_rtx->find(trial)->second < m_maxNumRtx)
	  {
	        // Increment the Rtx Counter (with another check of the presence of the element)
	        std::pair<std::map<std::vector<uint32_t>*,uint32_t>::iterator,bool> ret;
	        ret=num_rtx->insert (std::pair<std::vector<uint32_t>*,uint32_t>(&seqNum,1) );
	        if(ret.second==false)
	        {
	        	uint32_t counter = num_rtx->find(&seqNum)->second;
	            num_rtx->operator[](&seqNum)= counter+1;          // *** NB  VERIFICARE BENE
	        }
	        else
	        {
	            NS_LOG_UNCOND("ERROR INSIDE THE RTX MAP! ELEMENT NOT FOUND");
	        }

	        uint32_t seqNumber = contentInfoSeqNum->find(&seqNum)->second;

	        NS_LOG_UNCOND("ON TIMEOUT - CONTENT INFO SEQ NUM: \t" << seqNumber << "\t" << Simulator::Now());

	        m_rtt->SentSeq (SequenceNumber32 (seqNumber), 1);                // make sure to disable RTT calculation for this sample

	        m_retxSeqs.insert (&seqNum);

	  }
	  else
	  {
            uint32_t numChunkTot = download_time_file->find(contentNameRto)->second.expNumChunk;
	        std::string checkChunk = "s_";
	        ss_rto << checkChunk << (numChunkTot-1);
	        checkChunk = ss_rto.str();
	        ss_rto.str("");
	        if (chunkStr.compare(checkChunk) == 0)  // I'm eliminating the last chunk of the content; so, the corespondent file is erased
	                                                   // too from the structure file_download_time.
	                download_time_file->erase(contentNameRto);

	        Time firstTxTime = download_time->find(fullNameRto)->second.sentTimeChunk_First;
	     
	        // APP-LEVEL Tracing for eliminated files
	     	std::string nodeType = this->GetNode()->GetObject<ForwardingStrategy>()->GetNodeType();
	        m_numMaxRtx(&fullNameRto, firstTxTime.GetMicroSeconds(), "ELM", nodeType);
	      
            download_time->erase(fullNameRto);
	        num_rtx->erase(&seqNum);
	        seq_contenuto->erase(fullNameRto);
	        m_retxSeqs.erase(&seqNum);
	        m_seqLifetimes.erase (&seqNum);

	        NS_LOG_UNCOND("ON TIMEOUT - APP: SEQ TIMEOUT SIZE BEFORE\t" << m_seqTimeouts.size() << "\t" << Simulator::Now());
	        m_seqTimeouts.erase (&seqNum);
	        NS_LOG_UNCOND("ON TIMEOUT - APP: SEQ TIMEOUT SIZE AFTER\t" << m_seqTimeouts.size() << "\t" << Simulator::Now());


	        uint32_t seqNumber = contentInfoSeqNum->find(&seqNum)->second;


	        m_rtt->AckSeq (SequenceNumber32 (seqNumber));
	        if(download_time_first->find(fullNameRto)!= download_time_first->end())     /// **** MODIFICA
	        	download_time_first->erase(fullNameRto);
	  }
	  ScheduleNextPacket ();
}

void
ConsumerRtxZipf::WillSendOutInterest (uint32_t sequenceNumber)
{
  NS_LOG_DEBUG ("Trying to add " << sequenceNumber << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");

  std::vector<uint32_t> p;
  p.push_back(0);
  p.push_back(0);

  m_seqTimeouts.insert (SeqTimeout (&p, Simulator::Now ()));
  m_seqFullDelay.insert (SeqTimeout (&p, Simulator::Now ()));

  std::vector<uint32_t> trial;
  trial.push_back(0);
  trial.push_back(0);

  m_seqLastDelay.erase (&trial);
  m_seqLastDelay.insert (SeqTimeout (&trial, Simulator::Now ()));

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

