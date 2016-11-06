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

#include "ndn-consumer-zipf.h"
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


NS_LOG_COMPONENT_DEFINE ("ndn.ConsumerZipf");

namespace ns3 {
namespace ndn {



NS_OBJECT_ENSURE_REGISTERED (ConsumerZipf);
    
TypeId
ConsumerZipf::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::ConsumerZipf")
    .SetGroupName ("Ndn")
    .SetParent<App> ()
        
    .AddAttribute ("NumberOfContents", "Number of the Contents in total",
                   StringValue ("100"),
                   //MakeUintegerAccessor (&ConsumerZipf::NumberOfContents, &ConsumerZipf::GetNumberOfContents),
                   MakeUintegerAccessor (&ConsumerZipf::GetNumberOfContents),
                   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("q", "parameter of improve rank",
                   StringValue ("0.7"),
                   MakeDoubleAccessor (&ConsumerZipf::SetQ, &ConsumerZipf::GetQ),
                   MakeDoubleChecker<double> ())
    
    .AddAttribute ("s", "parameter of power",
                   StringValue ("0.7"),
                   MakeDoubleAccessor (&ConsumerZipf::SetS, &ConsumerZipf::GetS),
                   MakeDoubleChecker<double> ())
  
    
    .AddAttribute ("StartSeq", "Initial sequence number",
                   IntegerValue (0),
                   MakeIntegerAccessor(&ConsumerZipf::m_seq),
                   MakeIntegerChecker<int32_t>())

    .AddAttribute ("Prefix","Name of the Interest",
                   StringValue ("/"),
                   MakeNameComponentsAccessor (&ConsumerZipf::m_interestName),
                   MakeNameComponentsChecker ())
    .AddAttribute ("LifeTime", "LifeTime for interest packet",
                   StringValue ("2s"),
                   MakeTimeAccessor (&ConsumerZipf::m_interestLifeTime),
                   MakeTimeChecker ())
    .AddAttribute ("MinSuffixComponents", "MinSuffixComponents",
                   IntegerValue(-1),
                   MakeIntegerAccessor(&ConsumerZipf::m_minSuffixComponents),
                   MakeIntegerChecker<int32_t>())
    .AddAttribute ("MaxSuffixComponents", "MaxSuffixComponents",
                   IntegerValue(-1),
                   MakeIntegerAccessor(&ConsumerZipf::m_maxSuffixComponents),
                   MakeIntegerChecker<int32_t>())
    .AddAttribute ("ChildSelector", "ChildSelector",
                   BooleanValue(false),
                   MakeBooleanAccessor(&ConsumerZipf::m_childSelector),
                   MakeBooleanChecker())
    .AddAttribute ("Exclude", "only simple name matching is supported (use NameComponents)",
                   NameComponentsValue (),
                   MakeNameComponentsAccessor (&ConsumerZipf::m_exclude),
                   MakeNameComponentsChecker ())

    .AddAttribute ("RetxTimer",
                   "Timeout defining how frequent retransmission timeouts should be checked",
                   StringValue ("25ms"),
                   //StringValue ("100s"),
                   MakeTimeAccessor (&ConsumerZipf::GetRetxTimer, &ConsumerZipf::SetRetxTimer),
                   MakeTimeChecker ())

    .AddAttribute ("MaxNumRtx", "Maximum Number of Interest Retransmissions",
                   StringValue ("10"),
                   MakeUintegerAccessor(&ConsumerZipf::m_maxNumRtx),
                   MakeUintegerChecker<int32_t>())


    .AddTraceSource ("PathWeightsTrace", "PathWeightsTrace",
                    MakeTraceSourceAccessor (&ConsumerZipf::m_pathWeightsTrace))


    ;

  return tid;
}
    
ConsumerZipf::ConsumerZipf ()
  : m_rand (0, std::numeric_limits<uint32_t>::max ())
  , m_seq (0)
  , m_seqMax (51000) // don't request anything
  , m_N (90000) // needed here to make sure when SetQ/SetS are called, there is a valid value of N
  , m_q (0)
  //, m_s (1.2)
  , m_s (1)
  , m_SeqRng (0.0, 1.0)
{
  // SetNumberOfContents is called by NS-3 object system during the initialization
  NS_LOG_FUNCTION_NOARGS ();

  m_rtt = CreateObject<RttMeanDeviation> ();
  TimeValue value;
  this->GetAttributeFailSafe("RetxTimer", value);
  //SetRetxTimer(value.Get());
  //SetRetxTimer(Seconds(0.025));
  SetRetxTimer(Seconds(0.5));
  seq_contenuto = new std::map<std::string, uint32_t> ();
  num_rtx = new std::map<uint32_t, uint32_t> ();
  ////  ******  NNNNBBBBB ******* Aumentato il numero di ritrasmissioni consentite al client
  m_maxNumRtx = 100;
}

void
ConsumerZipf::NumberOfContents (uint32_t numOfContents)
{
  NS_LOG_UNCOND("CREAZIONE CATALOGO!!");
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
ConsumerZipf::GetNumberOfContents () const
{
  return m_N;
}

void
ConsumerZipf::SetQ (double q)
{
  m_q = q;
  //SetNumberOfContents (m_N);
}

double
ConsumerZipf::GetQ () const
{
  return m_q;
}

void
ConsumerZipf::SetS (double s)
{
  m_s = s;
  //SetNumberOfContents (m_N);
}

double
ConsumerZipf::GetS () const
{
  return m_s;
}




void
ConsumerZipf::SetRetxTimer (Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning ())
    {
      // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
      Simulator::Remove (m_retxEvent); // slower, but better for memory
    }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule (m_retxTimer,
                                     &ConsumerZipf::CheckRetxTimeout, this); 
}

Time
ConsumerZipf::GetRetxTimer () const
{
  return m_retxTimer;
}

//  ***************  FUNZIONE CHECKRETXTIMEOUT ORIGINARIA  **************


void
ConsumerZipf::CheckRetxTimeout ()
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
                                     &ConsumerZipf::CheckRetxTimeout, this); 
}


// Application Methods
void 
ConsumerZipf::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS ();

  // do base stuff
  App::StartApplication ();

  ScheduleNextPacket ();
}
    
void 
ConsumerZipf::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS ();

  // cancel periodic packet generation
  Simulator::Cancel (m_sendEvent);

  // cleanup base stuff
  App::StopApplication ();
  //file_richieste.close();
}
    

void
ConsumerZipf::SendPacket ()
{
  if (!m_active) return;
  
  bool retransmitt = true;

  NS_LOG_FUNCTION_NOARGS ();

  uint32_t seq=std::numeric_limits<uint32_t>::max (); //invalid


  while (m_retxSeqs.size ())                        // LISTA ORDINATA dei numeri di sequenza degli interest da ritrasmettere
  {
	  seq = *m_retxSeqs.begin ();

	  m_retxSeqs.erase (m_retxSeqs.begin ());

	  break;
  }

  if (seq == std::numeric_limits<uint32_t>::max ())              // Dovrebbe entrare in questo 'if' se non c'è nessun Interest da ritrasmettere
  {
	retransmitt = false; 
    if (m_seqMax != std::numeric_limits<uint32_t>::max ())       // Se è uguale, nn ci sono limiti al numero di Interest da inviare
    {
       if (m_seq >= m_seqMax)                                    // Numero massimo di Interest da inviare raggiunto
       {
           return; // we are totally done
        }
    }
    
    //seq = ConsumerZipf::GetNextSeq();  
    //m_seq++;                                             // Aumenta il numero di sequenza
    seq = m_seq++;
  }
  
  uint32_t req_cont = ConsumerZipf::GetNextSeq();
  std::string content;
  std::stringstream ss;
  //ss << "/" << seq;
  ss << "/" << req_cont;
  content = ss.str();
  ss.str("");
  
    
  std::map<std::string, uint32_t>::iterator it;
  it = seq_contenuto->find(content);
  if(it!=seq_contenuto->end())
  {
       m_rtt->IncrNext(SequenceNumber32 (seq), 1);
       ScheduleNextPacket ();
       return;
  }
  else
  {
    	num_rtx->insert(std::pair<uint32_t, uint32_t> (seq,1));
       	seq_contenuto->insert(std::pair<std::string,uint32_t>(content,seq));
  }
  
  if(retransmitt)
       NS_LOG_UNCOND ("NODONODO:\t" << Application::GetNode()->GetId() << "\t Interest RITRASMESSO CONSUMER:\t" << content << "\t" << Simulator::Now().GetNanoSeconds());


  Ptr<InterestHeader> interestHeader = Create<InterestHeader>();
  interestHeader->SetName(Create<NameComponents> (content));
  interestHeader->SetNonce               (m_rand.GetValue ());
  interestHeader->SetInterestLifetime    (m_interestLifeTime);
  interestHeader->SetChildSelector       (m_childSelector);
  if (m_exclude.size ()>0)
    {
      interestHeader->SetExclude (Create<NameComponents> (m_exclude));
    }
  interestHeader->SetMaxSuffixComponents (m_maxSuffixComponents);    // Se è negativo nn c'è limite al numero di componenti
  interestHeader->SetMinSuffixComponents (m_minSuffixComponents);



  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (*interestHeader);
  NS_LOG_DEBUG ("Interest packet size: " << packet->GetSize ());

  // ***** m_seqTimeouts ci dice quanti Interest sono ancora in volo
  NS_LOG_DEBUG ("Trying to add " << seq << " with " << Simulator::Now () << ". already " << m_seqTimeouts.size () << " items");

  m_seqTimeouts.insert (SeqTimeout (seq, Simulator::Now ()));
  m_seqLifetimes.insert (SeqTimeout (seq, Simulator::Now () + m_interestLifeTime)); // only one insert will work. if entry exists, nothing will happen... nothing should happen
  m_transmittedInterests (interestHeader, this, m_face);

  m_rtt->SentSeq (SequenceNumber32 (seq), 1);

  m_protocolHandler (packet);                                      // Il pacchetto creato viene passato al livello inferiore.

  ScheduleNextPacket ();
}

uint32_t
ConsumerZipf::GetNextSeq()
{
  uint32_t content_index = 1; //[1, m_N]
  std::vector<double>* local_cs = App::GetCumSum();
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
      //p_sum = m_Pcum[i];   //m_Pcum[i] = m_Pcum[i-1] + p[i], p[0] = 0;   e.g.: p_cum[1] = p[1], p_cum[2] = p[1] + p[2]
      p_sum = local_cs->operator[](i);
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


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////


void
ConsumerZipf::OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                               Ptr<Packet> payload)
{
  if (!m_active) return;

  NS_LOG_FUNCTION (this << contentObject << payload);

  NS_LOG_INFO ("Received content object: " << boost::cref(*contentObject));
  
  App::OnContentObject (contentObject, payload); // tracing inside
  
  std::stringstream ss;
  ss << contentObject -> GetName();
  const std::string cont_ric = ss.str();
  std::string cont_ric_app = ss.str();
  uint32_t seq_ric = seq_contenuto->find(cont_ric)->second;
  ss.str("");
  
  if((seq_ric % 10) == 0)   // Ogni 100 chunk ricevuti, printa una riga

        NS_LOG_UNCOND("NODONODO:\t" << Application::GetNode()->GetId() << "\t DATA RICEVUTO APP:\t" << cont_ric_app << "\t" << Simulator::Now().GetNanoSeconds());

  
  seq_contenuto->erase(cont_ric);
  
  NS_LOG_INFO ("< DATA for " << cont_ric << "\t" << "Sequence Number " << seq_ric);

  m_seqLifetimes.erase (seq_ric);
  m_seqTimeouts.erase (seq_ric);
  m_retxSeqs.erase (seq_ric);

  num_rtx->erase(seq_ric);

  m_rtt->AckSeq (SequenceNumber32 (seq_ric+1));

}

void
ConsumerZipf::OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> origPacket)
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
ConsumerZipf::OnTimeout (uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION (sequenceNumber);

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
        m_rtt->IncreaseMultiplier (); 
        m_rtt->SentSeq (SequenceNumber32 (sequenceNumber), 1);                // make sure to disable RTT calculation for this sample
        m_retxSeqs.insert (sequenceNumber);

  }
  else
  {

        num_rtx->erase(sequenceNumber);

        std::string content;
        std::stringstream ss;
        ss << "/" << sequenceNumber;
        content = ss.str();
        ss.str("");
        
        seq_contenuto->erase(content);     /// **** MODIFICA
        m_retxSeqs.erase(sequenceNumber);
        m_seqLifetimes.erase (sequenceNumber);
        m_seqTimeouts.erase (sequenceNumber);
        m_rtt->AckSeq (SequenceNumber32 (sequenceNumber));
    
  }
  
  ScheduleNextPacket (); 
}


} // namespace ndn
} // namespace ns3

