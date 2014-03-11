/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011,2012 University of California, Los Angeles
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
 *         Ilya Moiseenko <iliamo@cs.ucla.edu>
 *
 */

#include "ndn-repo.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/ndn-name.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"

NS_LOG_COMPONENT_DEFINE ("ndn.rp.Repo");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (Repo);

TypeId
Repo::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::Repo")
    .SetGroupName ("Ndn")
    .SetParent<Object> ()

    .AddTraceSource ("RepoHits", "Trace called every time there is a cache hit",
                     MakeTraceSourceAccessor (&Repo::m_repoHitsTrace))

    .AddTraceSource ("RepoMisses", "Trace called every time there is a cache miss",
                     MakeTraceSourceAccessor (&Repo::m_repoMissesTrace))
    ;

  return tid;
}


Repo::~Repo ()
{
}

namespace rp {

//////////////////////////////////////////////////////////////////////

Entry::Entry (Ptr<Repo> rp, Ptr<const ContentObject> header, Ptr<const Packet> packet)
  : m_rp (rp)
  , m_header (header)
  , m_packet (packet->Copy ())
{
}

Ptr<Packet>
Entry::GetFullyFormedNdnPacket () const
{
  static ContentObjectTail tail; ///< \internal for optimization purposes

  Ptr<Packet> packet = m_packet->Copy ();
  packet->AddHeader (*m_header);
  packet->AddTrailer (tail);
  return packet;
}

const Name&
Entry::GetName () const
{
  return m_header->GetName ();
}

Ptr<const ContentObject>
Entry::GetHeader () const
{
  return m_header;
}

Ptr<const Packet>
Entry::GetPacket () const
{
  return m_packet;
}

Ptr<Repo>
Entry::GetRepo ()
{
  return m_rp;
}


} // namespace cs
} // namespace ndn
} // namespace ns3
