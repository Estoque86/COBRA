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

#ifndef NDN_CONSUMER_RTX_H
#define NDN_CONSUMER_RTX_H

#include "ndn-app.h"
#include "ns3/random-variable.h"
#include "ns3/ndn-name.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "../model/bloom-filter/ndn-bloom-filter-base.h"
#include "../../internet/model/rtt-estimator.h"

#include <set>
#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <fstream>
#include <limits>

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn
 * \brief NDN application for sending out Interest packets
 */
class ConsumerRtx: public App
{
public:
  static TypeId GetTypeId ();

  /**
   * \brief Default constructor
   * Sets up randomizer function and packet sequence number
   */
  ConsumerRtx ();
  virtual ~ConsumerRtx ();

  // From App
  // virtual void
  // OnInterest (const Ptr<const Interest> &interest);

  virtual void
  OnNack (const Ptr<const Interest> &interest, Ptr<Packet> packet);

  virtual void
  OnContentObject (const Ptr<const ContentObject> &contentObject,
                   Ptr<Packet> payload);

  /**
   * @brief Timeout event
   * @param sequenceNumber time outed sequence number
   */
  virtual void
  OnTimeout (uint32_t sequenceNumber);

  /**
   * @brief Actually send packet
   */
  void
  SendPacket ();

  /**
   * @brief An event that is fired just before an Interest packet is actually send out (send is inevitable)
   *
   * The reason for "before" even is that in certain cases (when it is possible to satisfy from the local cache),
   * the send call will immediately return data, and if "after" even was used, this after would be called after
   * all processing of incoming data, potentially producing unexpected results.
   */
  virtual void
  WillSendOutInterest (uint32_t sequenceNumber);
  
protected:
  // from App
  virtual void
  StartApplication ();

  virtual void
  StopApplication ();

  /**
   * \brief Constructs the Interest packet and sends it using a callback to the underlying NDN protocol
   */
  virtual void
  ScheduleNextPacket () = 0;

  /**
   * \brief Checks if the packet need to be retransmitted becuase of retransmission timer expiration
   */
  void
  CheckRetxTimeout ();

  /**
   * \brief Modifies the frequency of checking the retransmission timeouts
   * \param retxTimer Timeout defining how frequent retransmission timeouts should be checked
   */
  void
  SetRetxTimer (Time retxTimer);

  /**
   * \brief Returns the frequency of checking the retransmission timeouts
   * \return Timeout defining how frequent retransmission timeouts should be checked
   */
  Time
  GetRetxTimer () const;

  /**
   * \brief Allows the positioning in a file at a specified line.
   */
  std::ifstream& GotoLine(std::ifstream& file, uint32_t num);

protected:
  //  *** (MT) ***
  std::ifstream request_file;    ///< @brief ifstream associated to the file containing the requests of the client.
  std::map<std::string, uint32_t> *seq_contenuto;    ///< @brief map the content name to the sequence number.

  uint32_t m_maxNumRtx;       ///< @brief maximum number of allowed retransmission.
  std::map<uint32_t, uint32_t> *num_rtx;  ///< @brief map the sequence number of the content to the relative number of retransmission.

  // *** (MT) Structure used to track the DOWNLOAD TIME of a single chunk; it is needed to eliminate the time elapsed between the  ****
           // effective retransmission of an Interest and its scheduling.
  /*
   *  - sentTime_First = time when the Interest is sent.
   *  - sentTime_New = time updated every time there is a retransmission.
   *  - incrementalTime = it is initialized to '0'; when the RTO elapses, the increment is calculated as the current time (i.e., when the RTO elapses) - sending time.
   *  					  this value is added to the old increment.
   *
   *  When the corresponding Chunk is received, the Download Time is calculated as " (Now - sentTime_New)+incrementalTime.
   */

  struct DownloadEntry_Chunk
  {
	  DownloadEntry_Chunk (Time _sentTimeChunk_First, Time _sentTimeChunk_New, Time _incrementalTime): sentTimeChunk_First(_sentTimeChunk_First), sentTimeChunk_New(_sentTimeChunk_New), incrementalTime(_incrementalTime) {}

	  Time sentTimeChunk_First;
	  Time sentTimeChunk_New;
	  Time incrementalTime;
  };

  std::map<std::string, DownloadEntry_Chunk> *download_time_first;       ///< @brief map the content name of the first chunk to its download entry. It is needed to calculate the SEEK TIME of a Content.
  std::map<std::string, DownloadEntry_Chunk> *download_time;		     ///< @brief map the content name of every chunk to their download entry.

  // *** (MT) Structure used to track the DOWNLOAD TIME of an entire content ***
  /* It is indexed through the content name of the file (i.e., excluding the last chunk component)
   *
   *  - sentTimeFirst = time when the first Interest is sent.
   *  - downloadTime  = download time of the entire file.
   *  - expNumChunk   = expected number of chunks; it is read from the request file of the client  (from the line corresponding to the first chunk of each file).
   *  - rcvNumChunk   = received number of chunks.
   *  - distance      = hit distance of the entire file; it is calculated by averaging the hit distances of each chunk.
   *
   */

  struct DownloadEntry
  {
	  DownloadEntry (Time _sentTimeFirst, Time _downloadTime, uint32_t _expNumChunk, uint32_t _rcvNumChunk, uint32_t _distance) : sentTimeFirst (_sentTimeFirst), downloadTime (_downloadTime), expNumChunk (_expNumChunk), rcvNumChunk (_rcvNumChunk), distance (_distance) { }

	  Time sentTimeFirst;
	  Time downloadTime;
	  uint32_t expNumChunk;
	  uint32_t rcvNumChunk;
      uint32_t distance;

  };

  std::map<std::string, DownloadEntry> *download_time_file;     ///< @brief map the content name of every file to their download entry.

  UniformVariable m_rand; ///< @brief nonce generator

  uint32_t        m_seq;  ///< @brief currently requested sequence number
  uint32_t        m_seqMax;    ///< @brief maximum number of sequence number
  EventId         m_sendEvent; ///< @brief EventId of pending "send packet" event
  Time            m_retxTimer; ///< @brief Currently estimated retransmission timer
  EventId         m_retxEvent; ///< @brief Event to check whether or not retransmission should be performed

  Ptr<RttEstimator> m_rtt; ///< @brief RTT estimator

  Time         m_offTime;             ///< \brief Time interval between packets
  Name         m_interestName;        ///< \brief NDN Name of the Interest (use Name)
  Time         m_interestLifeTime;    ///< \brief LifeTime for interest packet

  /// @cond include_hidden
  /**
   * \struct This struct contains sequence numbers of packets to be retransmitted
   */
  struct RetxSeqsContainer :
    public std::set<uint32_t> { };

  RetxSeqsContainer m_retxSeqs;             ///< \brief ordered set of sequence numbers to be retransmitted

  /**
   * \struct This struct contains a pair of packet sequence number and its timeout
   */
  struct SeqTimeout
  {
    SeqTimeout (uint32_t _seq, Time _time) : seq (_seq), time (_time) { }

    uint32_t seq;
    Time time;
  };
/// @endcond

/// @cond include_hidden
  class i_seq { };
  class i_timestamp { };
/// @endcond

/// @cond include_hidden
  /**
   * \struct This struct contains a multi-index for the set of SeqTimeout structs
   */
  struct SeqTimeoutsContainer :
    public boost::multi_index::multi_index_container<
    SeqTimeout,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
        boost::multi_index::tag<i_seq>,
        boost::multi_index::member<SeqTimeout, uint32_t, &SeqTimeout::seq>
        >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<i_timestamp>,
        boost::multi_index::member<SeqTimeout, Time, &SeqTimeout::time>
        >
      >
    > { } ;

  SeqTimeoutsContainer m_seqTimeouts;       ///< \brief multi-index for the set of SeqTimeout structs

  SeqTimeoutsContainer m_seqLastDelay;
  SeqTimeoutsContainer m_seqFullDelay;
  std::map<uint32_t, uint32_t> m_seqRetxCounts;
  SeqTimeoutsContainer m_seqLifetimes;

  //TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */,
  //               Time /* delay */, int32_t /*hop count*/> m_lastRetransmittedInterestDataDelay;
  //TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */,
  //               Time /* delay */, uint32_t /*retx count*/,
  //               int32_t /*hop count*/> m_firstInterestDataDelay;

  //TracedCallback<const std::string*> m_interestApp;
  TracedCallback<const std::string*, int64_t, std::string> m_interestApp;
  
  //TracedCallback<Ptr<const Interest> > m_timeOutTrace;
  TracedCallback<const std::string*, int64_t, std::string > m_timeOutTrace;

//TracedCallback<const std::string*, int64_t, int64_t, uint32_t > m_downloadTime;
  TracedCallback<const std::string*, int64_t, int64_t, uint32_t, std::string > m_downloadTime;

//  TracedCallback<const std::string*, int64_t, int64_t, uint32_t > m_downloadTimeFile;
  TracedCallback<const std::string*, int64_t, int64_t, uint32_t, std::string > m_downloadTimeFile;

//  TracedCallback<const std::string*, int64_t> m_uncompleteFile;
  TracedCallback<const std::string*, int64_t, std::string> m_uncompleteFile;

  //TracedCallback<const std::string*, int64_t > m_numMaxRtx;
  TracedCallback<const std::string*, int64_t, std::string > m_numMaxRtx;
/// @endcond
};

} // namespace ndn
} // namespace ns3

#endif

