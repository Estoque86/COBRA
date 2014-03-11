/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef _NDN_PIT_IMPL_H_
#define	_NDN_PIT_IMPL_H_

#include "ndn-pit.h"
#include "../bloom-filter/ndn-bloom-filter-base.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include "../../utils/trie/trie-with-policy.h"
#include "ndn-pit-entry-impl.h"

#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/ndn-forwarding-strategy.h"
#include "ns3/ndn-name.h"


namespace ns3 {
namespace ndn {

class ForwardingStrategy;
class BloomFilterBase;

namespace pit {

/**
 * \ingroup ndn
 * \brief Class implementing Pending Interests Table
 */
template<class Policy>
class PitImpl : public Pit
              , protected ndnSIM::trie_with_policy<Name,
                                                   ndnSIM::smart_pointer_payload_traits< EntryImpl< PitImpl< Policy > > >,
                                                   // ndnSIM::persistent_policy_traits
                                                   Policy
                                                   >
{
public:
  typedef ndnSIM::trie_with_policy<Name,
                                   ndnSIM::smart_pointer_payload_traits< EntryImpl< PitImpl< Policy > > >,
                                   // ndnSIM::persistent_policy_traits
                                   Policy
                                   > super;
  typedef EntryImpl< PitImpl< Policy > > entry;

  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static TypeId GetTypeId ();

  /**
   * \brief PIT constructor
   */
  PitImpl ();

  /**
   * \brief Destructor
   */
  virtual ~PitImpl ();

  // inherited from Pit
  virtual Ptr<Entry>
  Lookup (const ContentObject &header);

  virtual Ptr<Entry>
  Lookup (const Interest &header);

  virtual Ptr<Entry>
  Find (const Name &prefix);

  virtual Ptr<Entry>
  Create (Ptr<const Interest> header);

  virtual Ptr<Entry>
  CreateType (Ptr<const Interest> header, const Ptr<Face> &incomingFace);


  virtual void
  MarkErased (Ptr<Entry> entry);

  virtual void
  Print (std::ostream &os) const;

  virtual uint32_t
  GetSize () const;

  virtual Ptr<Entry>
  Begin ();

  virtual Ptr<Entry>
  End ();

  virtual Ptr<Entry>
  Next (Ptr<Entry>);

  const typename super::policy_container &
  GetPolicy () const { return super::getPolicy (); }

  typename super::policy_container &
  GetPolicy () { return super::getPolicy (); }

protected:
  void RescheduleCleaning ();
  void CleanExpired ();

  // inherited from Object class
  virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
  virtual void DoDispose (); ///< @brief Do cleanup

private:
  uint32_t
  GetMaxSize () const;

  void
  SetMaxSize (uint32_t maxSize);

  uint32_t
  GetCurrentSize () const;

private:
  EventId m_cleanEvent;
  Ptr<Fib> m_fib; ///< \brief Link to FIB table
  Ptr<ForwardingStrategy> m_forwardingStrategy;


  static LogComponent g_log; ///< @brief Logging variable

  // indexes
  typedef
  boost::intrusive::multiset<entry,
                        boost::intrusive::compare < TimestampIndex< entry > >,
                        boost::intrusive::member_hook< entry,
                                                       boost::intrusive::set_member_hook<>,
                                                       &entry::time_hook_>
                        > time_index;
  time_index i_time;

  friend class EntryImpl< PitImpl >;
};

//////////////////////////////////////////
////////// Implementation ////////////////
//////////////////////////////////////////

template<class Policy>
LogComponent PitImpl< Policy >::g_log = LogComponent (("ndn.pit." + Policy::GetName ()).c_str ());


template<class Policy>
TypeId
PitImpl< Policy >::GetTypeId ()
{
  static TypeId tid = TypeId (("ns3::ndn::pit::"+Policy::GetName ()).c_str ())
    .SetGroupName ("Ndn")
    .SetParent<Pit> ()
    .AddConstructor< PitImpl< Policy > > ()
    .AddAttribute ("MaxSize",
                   "Set maximum size of PIT in bytes. If 0, limit is not enforced",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PitImpl< Policy >::GetMaxSize,
                                         &PitImpl< Policy >::SetMaxSize),
                   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("CurrentSize", "Get current size of PIT in bytes",
                   TypeId::ATTR_GET,
                   UintegerValue (0),
                   MakeUintegerAccessor (&PitImpl< Policy >::GetCurrentSize),
                   MakeUintegerChecker<uint32_t> ())
    ;

  return tid;
}

template<class Policy>
uint32_t
PitImpl<Policy>::GetCurrentSize () const
{
  return super::getPolicy ().size ();
}

template<class Policy>
PitImpl<Policy>::PitImpl ()
{
}

template<class Policy>
PitImpl<Policy>::~PitImpl ()
{
}

template<class Policy>
uint32_t
PitImpl<Policy>::GetMaxSize () const
{
  return super::getPolicy ().get_max_size ();
}

template<class Policy>
void
PitImpl<Policy>::SetMaxSize (uint32_t maxSize)
{
  super::getPolicy ().set_max_size (maxSize);
}

template<class Policy>
void
PitImpl<Policy>::NotifyNewAggregate ()
{
  if (m_fib == 0)
    {
      m_fib = GetObject<Fib> ();
    }
  if (m_forwardingStrategy == 0)
    {
      m_forwardingStrategy = GetObject<ForwardingStrategy> ();
    }

  Pit::NotifyNewAggregate ();
}

template<class Policy>
void
PitImpl<Policy>::DoDispose ()
{
  super::clear ();

  m_forwardingStrategy = 0;
  m_fib = 0;

  Pit::DoDispose ();
}

template<class Policy>
void
PitImpl<Policy>::RescheduleCleaning ()
{
  // m_cleanEvent.Cancel ();
  Simulator::Remove (m_cleanEvent); // slower, but better for memory
  if (i_time.empty ())
    {
      // NS_LOG_DEBUG ("No items in PIT");
      return;
    }

  Time nextEvent = i_time.begin ()->GetExpireTime () - Simulator::Now ();
  if (nextEvent <= 0) nextEvent = Seconds (0);

  NS_LOG_DEBUG ("Schedule next cleaning in " <<
                nextEvent.ToDouble (Time::S) << "s (at " <<
                i_time.begin ()->GetExpireTime () << "s abs time");

  m_cleanEvent = Simulator::Schedule (nextEvent,
                                      &PitImpl<Policy>::CleanExpired, this);
}

template<class Policy>
void
PitImpl<Policy>::CleanExpired ()
{
  NS_LOG_LOGIC ("Cleaning PIT. Total: " << i_time.size ());
  Time now = Simulator::Now ();

  // uint32_t count = 0;
  while (!i_time.empty ())
    {
      typename time_index::iterator entry = i_time.begin ();
      if (entry->GetExpireTime () <= now) // is the record stale?
        {
          //m_forwardingStrategy->WillEraseTimedOutPendingInterest (entry->to_iterator ()->payload ());  // ** [MT] ** Tracing disabled
          super::erase (entry->to_iterator ());
          // count ++;
        }
      else
        break; // nothing else to do. All later records will not be stale
    }

  if (super::getPolicy ().size ())
    {
      NS_LOG_DEBUG ("Size: " << super::getPolicy ().size ());
      NS_LOG_DEBUG ("i_time size: " << i_time.size ());
    }
  RescheduleCleaning ();
}

// ** [MT] ** ORIGINAL VERSION. It is a LongestPrefixMatch searching
/*template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Lookup (const ContentObject &header)
{
  /// @todo use predicate to search with exclude filters
  typename super::iterator item = super::longest_prefix_match_if (header.GetName (), EntryIsNotEmpty ());

  if (item == super::end ())
    return 0;
  else
    return item->payload (); // which could also be 0
}*/

// ** [MT] ** MODIFIED VERSION. It is an ExactMatch searching
template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Lookup (const ContentObject &header)
{
  typename super::iterator foundItem, lastItem;
  bool reachLast;
  boost::tie (foundItem, reachLast, lastItem) = super::getTrie ().find (header.GetName ());

  if (!reachLast || lastItem == super::end ())
   return 0;
  else
    return lastItem->payload (); // which could also be 0
}

// ** [MT] ** When receiving an Interest, is always an exact match searching
template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Lookup (const Interest &header)
{
  // NS_LOG_FUNCTION (header.GetName ());
  NS_ASSERT_MSG (m_fib != 0, "FIB should be set");
  NS_ASSERT_MSG (m_forwardingStrategy != 0, "Forwarding strategy  should be set");

  typename super::iterator foundItem, lastItem;
  bool reachLast;
  boost::tie (foundItem, reachLast, lastItem) = super::getTrie ().find (header.GetName ());

  if (!reachLast || lastItem == super::end ())
    return 0;
  else
    return lastItem->payload (); // which could also be 0
}

template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Find (const Name &prefix)
{
  typename super::iterator item = super::find_exact (prefix);

  if (item == super::end ())
    return 0;
  else
    return item->payload ();
}


template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Create (Ptr<const Interest> header)
{
  NS_LOG_DEBUG (header->GetName ());
  Ptr<fib::Entry> fibEntry = m_fib->LongestPrefixMatch (*header);
  if (fibEntry == 0)
    return 0;

  // NS_ASSERT_MSG (fibEntry != 0,
  //                "There should be at least default route set" <<
  //                " Prefix = "<< header->GetName() << ", NodeID == " << m_fib->GetObject<Node>()->GetId() << "\n" << *m_fib);

  Ptr< entry > newEntry = ns3::Create< entry > (boost::ref (*this), header, fibEntry);
  std::pair< typename super::iterator, bool > result = super::insert (header->GetName (), newEntry);
  if (result.first != super::end ())
    {
      if (result.second)
        {
          newEntry->SetTrie (result.first);
          return newEntry;
        }
      else
        {
          // should we do anything?
          // update payload? add new payload?
          return result.first->payload ();
        }
    }
  else
    return 0;
}

/* ** [MT] ** Associates a FIB entry to the PIT entry under construction according to the info stored in the BFs of the node.
 *
 *   The BFs lookup returns a struct containing the interfaces of the node ordered according to the number of
 *   name components matching found in the respective BFs. The highest this number the highest the rank of the
 *   respective interface.
 *
 */

template<class Policy>
Ptr<Entry>
PitImpl<Policy>::CreateType (Ptr<const Interest> header, const Ptr<Face> &incomingFace)
{

  BloomFilterBase::bestFibEntry* best_entry = &(m_forwardingStrategy->GetBfFib()->LookupOrderedBloomFilters(header, incomingFace));

  Ptr< fib::Entry > fibEntry = ns3::Create<fib::Entry> (m_fib, header->GetNamePtr());
  for(uint32_t g = 0; g < best_entry->orderedFaces.size(); g++)
  {
	  fibEntry->AddOrUpdateRoutingMetric(best_entry->orderedFaces[g], best_entry->metrics[g]);
	  //NS_LOG_UNCOND("Added Metrics:\t" << best_entry->metrics[g]);
	  if(g == 0)
		  fibEntry->UpdateStatus(best_entry->orderedFaces[g], fib::FaceMetric::NDN_FIB_GREEN);  // Put GREEN status to the highest ranked interface.
  }

  delete best_entry;   // Free allocated space.

  // ** CREATE THE PIT ENTRY
  Ptr< entry > newEntry = ns3::Create< entry > (boost::ref (*this), header, fibEntry);
  std::pair< typename super::iterator, bool > result = super::insert (header->GetName (), newEntry);
  if (result.first != super::end ())
  {
      if (result.second)
      {
          newEntry->SetTrie (result.first);
          return newEntry;
      }
      else
      {
          // should we do anything?
          // update payload? add new payload?
          return result.first->payload ();
      }
  }
  else
    return 0;
}


template<class Policy>
void
PitImpl<Policy>::MarkErased (Ptr<Entry> item)
{
  if (this->m_PitEntryPruningTimout.IsZero ())
    {
      super::erase (StaticCast< entry > (item)->to_iterator ());
    }
  else
    {
      item->OffsetLifetime (this->m_PitEntryPruningTimout - item->GetExpireTime () + Simulator::Now ());
    }
}


template<class Policy>
void
PitImpl<Policy>::Print (std::ostream& os) const
{
  // !!! unordered_set imposes "random" order of item in the same level !!!
  typename super::parent_trie::const_recursive_iterator item (super::getTrie ()), end (0);
  for (; item != end; item++)
    {
      if (item->payload () == 0) continue;

      os << item->payload ()->GetPrefix () << "\t" << *item->payload () << "\n";
    }
}

template<class Policy>
uint32_t
PitImpl<Policy>::GetSize () const
{
  return super::getPolicy ().size ();
}

template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Begin ()
{
  typename super::parent_trie::recursive_iterator item (super::getTrie ()), end (0);
  for (; item != end; item++)
    {
      if (item->payload () == 0) continue;
      break;
    }

  if (item == end)
    return End ();
  else
    return item->payload ();
}

template<class Policy>
Ptr<Entry>
PitImpl<Policy>::End ()
{
  return 0;
}

template<class Policy>
Ptr<Entry>
PitImpl<Policy>::Next (Ptr<Entry> from)
{
  if (from == 0) return 0;

  typename super::parent_trie::recursive_iterator
    item (*StaticCast< entry > (from)->to_iterator ()),
    end (0);

  for (item++; item != end; item++)
    {
      if (item->payload () == 0) continue;
      break;
    }

  if (item == end)
    return End ();
  else
    return item->payload ();
}


} // namespace pit
} // namespace ndn
} // namespace ns3

#endif	/* NDN_PIT_IMPL_H */


