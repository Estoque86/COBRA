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

#ifndef NDN_CONSUMER_H
#define NDN_CONSUMER_H

#include "ndn-app.h"
#include "ns3/random-variable.h"
#include "ns3/ndn-name-components.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "../../internet/model/rtt-estimator.h"
#include "../model/Bloom_Filter/bloom_filter.h"
//#include "ns3/internet-module.h"

#include <set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <fstream>
#include <limits>
#include <map>

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn
 * \brief NDN application for sending out Interest packets
 */
class Consumer: public App
{
public: 
  static TypeId GetTypeId ();
        
  /**
   * \brief Default constructor 
   * Sets up randomizer function and packet sequence number
   */
  Consumer ();
  virtual ~Consumer () {};

  // From App
  // virtual void
  // OnInterest (const Ptr<const InterestHeader> &interest);

  virtual void
  OnNack (const Ptr<const InterestHeader> &interest, Ptr<Packet> packet);

  virtual void
  OnContentObject (const Ptr<const ContentObjectHeader> &contentObject,
                   Ptr<Packet> payload);

  // ********  FUNZIONE OnTimeout ORIGINARIA  *********

  /**
   * @brief Timeout event
   * @param sequenceNumber time outed sequence number
   */
  virtual void
  OnTimeout (uint32_t sequenceNumber);

// *******  FUNZIONE OnTimeout MODIFICATA  *********


/**
   * @brief Timeout event
   * @param cont_rich time outed content request
   */
/*  virtual void
  OnTimeout (std::string cont_rich);
*/

/**
   * @brief Actually send packet
   */
  // ***********  MODIFICA MICHELE  ***********
  // Il fine è quello di generare delle richieste secondo un patterm stabilito in un file differente per ciascun nodo.
  void SendPacket ();
  


protected:
  // from App    ************  MODIFICA MICHELE  ******************
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
   * \brief Funzione che consente di posizionarsi in corrispondenza della riga desiderata del file
   */
  std::ifstream& GotoLine(std::ifstream& file, uint32_t num);



protected:


    //  ***********  AGGIUNTA MICHELE  **********
        //  Ogni client deve avere accesso ad un ifstream associato al proprio file di richieste
  std::ifstream file_richieste;
  std::map<std::string, uint32_t> *seq_contenuto;
  //bloom_parameters parameters;
  //bloom_filter bf;

  uint32_t m_maxNumRtx;   // Numero Massimo di Ritrasmissioni consentite
  std::map<uint32_t, uint32_t> *num_rtx;  // Mappa che associa al numero di sequenza dell'Interest Inviato il numero di ritrasmissioni effettuate


  //boost::tokenizer<boost::char_separator<char> > tokenizer;
  //boost::char_separator<char> sep("\t");
  //tokenizer::iterator it_space;


  // **** STRUTTURA PER IL TRACCIAMENTO DEL DOWNLOAD TIME, ELIMINANDO I TEMPI MORTI DELLE RITRASMISSIONI ****
  /*
   *  - sentTime_First = è il tempo di invio dell'Interest;
   *  - sentTime_New = inizializzato a sentTime_First, e aggiornato ogni qualvolta ci sia una ritrasmissione;
   *  - incrementalTime = viene inizializzato a "0"; quando scade l'RTO, l'incremento viene calcolato come (istante di tempo di scadenza dell'RTO - istante INVIO),
   *  					  e viene aggiunto al vecchio incremento.
   *
   *  Alla ricezione del Chunk, il Tempo di Download viene calcolato come (Now - sentTime)+incrementalTime
   */

  struct DownloadEntry_Chunk
  {
	  DownloadEntry_Chunk (int64_t _sentTimeChunk_First, int64_t _sentTimeChunk_New, int64_t _incrementalTime): sentTimeChunk_First(_sentTimeChunk_First), sentTimeChunk_New(_sentTimeChunk_New), incrementalTime(_incrementalTime) {}

	  int64_t sentTimeChunk_First;
	  int64_t sentTimeChunk_New;
	  int64_t incrementalTime;
  };

  // **** MAPPA PER CALCOLARE IL TEMPO DI DOWNLOAD (o SEEK TIME) dei primi chunk di ciascun contenuto ****
  std::map<std::string, DownloadEntry_Chunk> *download_time_first;

  // **** MAPPA PER CALCOLARE IL TEMPO DI DOWNLOAD di tutti i chunk ****
  std::map<std::string, DownloadEntry_Chunk> *download_time;




  // **** STRUTTURA PER CALCOLARE IL TEMPO DI DOWNLOAD DELL'INTERO FILE *****
  /* La mappa, indicizzata con il nome del file non comprensivo del chunk, avrà una struttura al suo interno contenente:
   *
   * - Il tempo di invio del primo Interest per il primo chunk;
   * - Il numero di chunk attesi (letto dal file delle richieste in corrispondenza del primo chunk di ogni contenuto)
   * - Il numero di chunk ricevuti
   *
   * Se si riceve l'ultimo chunk (il controllo si potrebbe fare con una comparazione di stringhe) ma il numero di chunk ricevuti
   * non è uguale a quello atteso, il contenuto viene dichiarato perso.
   *
   */


  struct DownloadEntry
  {
	  DownloadEntry (int64_t _sentTimeFirst, int64_t _sentTime, uint32_t _expNumChunk, uint32_t _rcvNumChunk, uint32_t _distance) : sentTimeFirst (_sentTimeFirst), sentTime (_sentTime), expNumChunk (_expNumChunk), rcvNumChunk (_rcvNumChunk), distance (_distance) { }

	  int64_t sentTimeFirst;
          int64_t sentTime;
	  uint32_t expNumChunk;
	  uint32_t rcvNumChunk;
          uint32_t distance;

  };

  std::map<std::string, DownloadEntry> *download_time_file;

  UniformVariable m_rand; ///< @brief nonce generator

  uint32_t        m_seq;  ///< @brief currently requested sequence number
  uint32_t        m_seqMax;    ///< @brief maximum number of sequence number
  EventId         m_sendEvent; ///< @brief EventId of pending "send packet" event
  Time            m_retxTimer; ///< @brief Currently estimated retransmission timer
  EventId         m_retxEvent; ///< @brief Event to check whether or not retransmission should be performed

  Ptr<RttEstimator> m_rtt; ///< @brief RTT estimator
  
  Time               m_offTime;             ///< \brief Time interval between packets
  NameComponents     m_interestName;        ///< \brief NDN Name of the Interest (use NameComponents)
  Time               m_interestLifeTime;    ///< \brief LifeTime for interest packet
  int32_t            m_minSuffixComponents; ///< \brief MinSuffixComponents. See InterestHeader for more information
  int32_t            m_maxSuffixComponents; ///< \brief MaxSuffixComponents. See InterestHeader for more information
  bool               m_childSelector;       ///< \brief ChildSelector. See InterestHeader for more information
  NameComponents     m_exclude;             ///< \brief Exclude. See InterestHeader for more information



// *********  STRUTTURE ORIGINARIE PER LA VERIFICA DEI TIMEOUT DEGLI INTEREST  ******************


/// @cond include_hidden  
  //*
  // * \struct This struct contains sequence numbers of packets to be retransmitted

  struct RetxSeqsContainer :
    public std::set<uint32_t> { };
  
  RetxSeqsContainer m_retxSeqs;             ///< \brief ordered set of sequence numbers to be retransmitted

  //*
  // * \struct This struct contains a pair of packet sequence number and its timeout

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
  //*
  // * \struct This struct contains a multi-index for the set of SeqTimeout structs

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
  SeqTimeoutsContainer m_seqLifetimes;
  
  TracedCallback<Ptr<Node>, Ptr<Node>, uint32_t, uint32_t > m_pathWeightsTrace;

/// @endcond
};

} // namespace ndn
} // namespace ns3



  // *********  STRUTTURE MODIFICATE PER LA VERIFICA DEI TIMEOUT DEGLI INTEREST  ******************

/*
/// @cond include_hidden
 // *
 //  * \struct This struct contains sequence numbers of packets to be retransmitted

  struct RetxSeqsContainer :
    public std::set<std::string> { };

  RetxSeqsContainer m_retxSeqs;             ///< \brief ordered set of sequence numbers to be retransmitted

  //  *
  //   * \struct This struct contains a pair of packet sequence number and its timeout

    struct SeqTimeout
    {
      SeqTimeout (std::string _contenuto, Time _time) : contenuto (_contenuto), time (_time) { }

      std::string contenuto;
      Time time;
    };
  /// @endcond

  /// @cond include_hidden
    class i_contenuto { };
    class i_timestamp { };
  /// @endcond

  /// @cond include_hidden
  //  *
  //   * \struct This struct contains a multi-index for the set of SeqTimeout structs

    struct SeqTimeoutsContainer :
      public boost::multi_index::multi_index_container<
      SeqTimeout,
      boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
          boost::multi_index::tag<i_contenuto>,
          boost::multi_index::member<SeqTimeout, std::string, &SeqTimeout::contenuto>
          >,
        boost::multi_index::ordered_non_unique<
          boost::multi_index::tag<i_timestamp>,
          boost::multi_index::member<SeqTimeout, Time, &SeqTimeout::time>
          >
        >
      > { } ;

    SeqTimeoutsContainer m_seqTimeouts;       ///< \brief multi-index for the set of SeqTimeout structs
    SeqTimeoutsContainer m_seqLifetimes;

    TracedCallback<Ptr<Node>, Ptr<Node>, uint32_t, uint32_t > m_pathWeightsTrace;
  /// @endcond
};

} // namespace ndn
} // namespace ns3
*/
#endif

