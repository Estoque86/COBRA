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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Ilya Moiseenko <iliamo@cs.ucla.edu>
 */


// **** SELECTIVE FLOODING CON STRATEGIA INCREMENTALE (bisogna settare maxRetxCount della pitEntry = 1)  ****


#include "SelectiveFlooding.h"

#include "ns3/ndn-interest.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-pit-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
namespace ll = boost::lambda;

NS_LOG_COMPONENT_DEFINE ("ndn.fw.SelectiveFlooding");

namespace ns3 {
namespace ndn {
namespace fw {

NS_OBJECT_ENSURE_REGISTERED (SelectiveFlooding);
    
TypeId SelectiveFlooding::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndn::fw::SelectiveFlooding")
    .SetGroupName ("Ndn")
    .SetParent <Nacks> ()
    .AddConstructor <SelectiveFlooding> ()
    ;
  return tid;
}
    
SelectiveFlooding::SelectiveFlooding ()
{
}

bool
SelectiveFlooding::DoPropagateInterest (Ptr<Face> inFace,
                               Ptr<const Interest> header,
                               Ptr<const Packet> origPacket,
                               Ptr<pit::Entry> pitEntry)
{
  NS_LOG_FUNCTION (this);

  int propagatedCount = 0;


  if(!inFace->GetNode()->GetObject<ForwardingStrategy>()->GetBfFib()->GetBloomFilterFlag())
  {
	  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
      {
		  NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));
		  if (metricFace.GetStatus () == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
			  break;

		  if (!TrySendOutInterest (inFace, metricFace.GetFace (), header, origPacket, pitEntry))
		  {
			  continue;
		  }
		  propagatedCount++;
	  }
  }
  else
  {
	  bool increment = true;
	  bool proceed = false;
	  uint32_t outInterf = pitEntry->GetOutgoing().size();              // Get the number of outgoing faces associated to the Pit Entry.
	  uint32_t nodeInterf = inFace->GetNode()->GetNDevices();           // Get the number of the node interfaces.

	  if(outInterf == nodeInterf-1 && nodeInterf > 1)                   // This check is needed to verify if the node has already used/probed
		  increment = false;  	  	  	  	  	  	  	  	  	  	    // all of its interfaces. In that case, the Interest will be sent in flooding

	  NS_LOG_FUNCTION (this);

	  int32_t currentMetric = 0;       // It is the metric of the selected interface.

	  if(!increment)     // ** [MT] ** All the interfaces of the node have already been probed. If retransmissions are still arriving,
	                 //            flooding strategy will be used to forward them.
	  {
		  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
		  {
			  if (metricFace.GetFace() == inFace)
			  {
				  NS_LOG_DEBUG ("continue (same as incoming)");
				  continue; // same face as incoming, don't forward
			  }

			  bool ok = metricFace.GetFace()->IsUp();
			  if (!ok)
			  {
				  continue;
			  }
			  else
			  {
				  //transmission
				  Ptr<Packet> packetToSend = origPacket->Copy ();
				  bool txOk = metricFace.GetFace()->Send (packetToSend);
				  if(!txOk)
					  super::m_dropInterestFailure();

				  else
				  {
					  DidSendOutInterest (inFace, metricFace.GetFace(), header, origPacket, pitEntry);

					  propagatedCount++;

					  pitEntry->AddOutgoing (metricFace.GetFace());

					  pitEntry->IncreaseAllowedRetxCount ();          // ** [MT] ** Verify its utility.
				  }
			  }
		  }
		  return propagatedCount > 0;
	  }

	  else
	  {
		  if(!increment)  // ** [MT] ** The incremental probing of the interfaces has terminated its alternatives; no other increments are allowed.
		  {
			  return propagatedCount > 0;
		  }

		  else  		  // ** [MT] ** Otherwise, there are still new interfaces to be probed. Remember that the probing procedure will consider,
		              //            step by step, only the interface(s) that have a slightly lower rank of the one(s) that have been probed
		              //            at the previous (re)transmission.
		  {
			  BOOST_FOREACH (const fib::FaceMetric &metricFace, pitEntry->GetFibEntry ()->m_faces.get<fib::i_metric> ())
			  {
				  //NS_LOG_UNCOND("BOOST FOR EACH NODO: " << inFace->GetNode()->GetId() << "\tINTERFACE: " << metricFace.GetFace()->GetId() << "\tMETRIC: " << metricFace.GetRoutingCost());
				  if(currentMetric == 0)     // ** [MT] ** It is the first step of the probing procedure.
				  {
					  currentMetric = metricFace.GetRoutingCost();             // ** [MT] ** It will consider the interfaces in an increasing order, according
			  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//            to their routing cost (i.e., first the interfaces with the lowest cost)
					  //NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << "\t SELECTED METRIC FIRST INCREMENT: " << currentMetric);

					  NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));

					  if (metricFace.GetStatus() == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
						  break;

					  if (metricFace.GetFace() == inFace)
					  {
						  NS_LOG_DEBUG ("continue (same as incoming)");
						  continue; // same face as incoming, don't forward
					  }

					  if (inFace->GetNode()->GetNDevices() > 1)       // ** [MT] ** This procedure is worthless for the nodes that have just one interface
					  {
						  bool ok = metricFace.GetFace()->IsUp ();
						  if (!ok)
							  continue;

						  pit::Entry::out_iterator outgoing = pitEntry->GetOutgoing ().find (metricFace.GetFace());
						  if(outgoing!=pitEntry->GetOutgoing().end())    // ** [MT] ** It means that the nodes has already probed the selected interface,
		      			  	  	  	  	  	  	  	  	  	  	  	 //            so a further increment is allowed.
						  {
							  proceed = true;
						  }
						  else      // ** [MT] ** If it is a never probed interface, a further increment is not allowed.
						  {
							  proceed = false;
						  }
					  }

					  else      // ** [MT] ** The node has only one interface.
					  {
						  if (!WillSendOutInterest (metricFace.GetFace(), header, pitEntry))
						  {
							  continue;
						  }
					  }

					  //transmission
					  Ptr<Packet> packetToSend = origPacket->Copy ();
					  bool txOk = metricFace.GetFace()->Send (packetToSend);
//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << " PRIMO INCREMENTO TX RESULT:\t" << txOk << "\tInterface: " << metricFace.GetFace()->GetId());
					  if(!txOk)
						  super::m_dropInterestFailure();
					  else
					  {
						  DidSendOutInterest (inFace, metricFace.GetFace(), header, origPacket, pitEntry);

						  propagatedCount++;

						  pitEntry->AddOutgoing (metricFace.GetFace());
					  }
				  }

				  else	   // ** [MT] ** It is not the first step of the probing procedure; the node can add another interface with the same
				       //            routing cost or with a slightly higher routing cost than the one(s) selected before, if allowed
				  	   //            (i.e., proceed=true).
				  {
					  if(metricFace.GetRoutingCost() == currentMetric)     // ** [MT] ** The selected interface has the same metric that the one(s) selected previously.
					  {
						  currentMetric = metricFace.GetRoutingCost();
						  NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));

						  if (metricFace.GetStatus() == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
							  break;

						  if (metricFace.GetFace() == inFace)
						  {
							  NS_LOG_DEBUG ("continue (same as incoming)");
							  continue; // same face as incoming, don't forward
						  }

						  if (inFace->GetNode()->GetNDevices() > 1)
						  {
							  bool ok = metricFace.GetFace()->IsUp ();
							  if (!ok)
								  continue;

							  pit::Entry::out_iterator outgoing = pitEntry->GetOutgoing ().find (metricFace.GetFace());
							  if(outgoing!=pitEntry->GetOutgoing().end())    // ** [MT] ** It means that the nodes has already probed the selected interface,
 	  	  	  	  	  	  	  	  	 	 	 	 	 	 	 	 	 	 //            so a further increment is allowed.
							  {
								  proceed = true;
							  }
							  else
							  {
								  proceed = false;
							  }

						  }

						  else      // ** [MT] ** The node has only one interface.
						  {
							  if (!WillSendOutInterest (metricFace.GetFace(), header, pitEntry))
							  {
								  continue;
							  }
						  }

						  //transmission
						  Ptr<Packet> packetToSend = origPacket->Copy ();
						  bool txOk = metricFace.GetFace()->Send (packetToSend);
//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << " INCREMENTO METRICA UGUALE TX RESULT:\t" << txOk << "\tInterface: " << metricFace.GetFace()->GetId());
						  if(!txOk)
							  super::m_dropInterestFailure();
						  else
						  {
							  DidSendOutInterest (inFace, metricFace.GetFace(), header, origPacket, pitEntry);

							  propagatedCount++;
//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << " PROPAGETED COUNT METRICA UGUALE TX:\t" << propagatedCount);

							  pitEntry->AddOutgoing (metricFace.GetFace());
						  }
					  }

					  else      // ** [MT] ** The selected interface has a slightly higher routing cost than the one(s) selected before.
					  {
						  if(propagatedCount==0)   // ** [MT] ** The node has not forwarded any Interest yet; so it does not mind of the 'proceed' flag.
						  {
							  currentMetric = metricFace.GetRoutingCost();
							  NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));

							  if (metricFace.GetStatus() == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
								  break;

							  if (metricFace.GetFace() == inFace)
							  {
								  NS_LOG_DEBUG ("continue (same as incoming)");
								  continue; // same face as incoming, don't forward
							  }

							  if (inFace->GetNode()->GetNDevices() > 1)
							  {
	  		  				 	bool ok = metricFace.GetFace()->IsUp ();
	  		  				 	if (!ok)
	  		  					  continue;

	  		  				 	pit::Entry::out_iterator outgoing = pitEntry->GetOutgoing ().find (metricFace.GetFace());
	  		  				 	if(outgoing!=pitEntry->GetOutgoing().end())
	  		  				 	{
					      		  proceed = true;
	  		  				 	}
	  		  				 	else
	  		  				 	{
					      		  proceed = false;
	  		  				 	}
							  }

							  else      // ** [MT] ** The node has only one interface.
							  {
								  if (!WillSendOutInterest (metricFace.GetFace(), header, pitEntry))
								  {
									  continue;
								  }
							  }

							  //transmission
							  Ptr<Packet> packetToSend = origPacket->Copy ();
							  bool txOk = metricFace.GetFace()->Send (packetToSend);
//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << " METRICA PIU' ALTA NESSUNO INOLTARTO PRIMA TX RESULT:\t" << txOk << "\tInterface: " << metricFace.GetFace()->GetId());
							  if(!txOk)
								  super::m_dropInterestFailure();

							  else
							  {
								  DidSendOutInterest (inFace, metricFace.GetFace(), header, origPacket, pitEntry);

								  propagatedCount++;

								  pitEntry->AddOutgoing (metricFace.GetFace());
							  }
						  }

						  else   // ** [MT] ** Other Interest(s) have already been forwarded; so another interface(s) is added according to the 'proceed' flag.
						  {
							  //NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << "\t ENTRO CICLO INCREMENTO METRICA MAGGIORE!\tINTERFACE: " << metricFace.GetFace()->GetId());
							  if(proceed)
							  {
								  currentMetric = metricFace.GetRoutingCost();

								  NS_LOG_DEBUG ("Trying " << boost::cref(metricFace));

								  if (metricFace.GetStatus() == fib::FaceMetric::NDN_FIB_RED) // all non-read faces are in the front of the list
									  break;

								  if (metricFace.GetFace() == inFace)
								  {
									  NS_LOG_DEBUG ("continue (same as incoming)");
									  continue; // same face as incoming, don't forward
								  }

								  if (inFace->GetNode()->GetNDevices() > 1)
								  {
									  bool ok = metricFace.GetFace()->IsUp ();
									  if (!ok)
										  continue;

									  pit::Entry::out_iterator outgoing = pitEntry->GetOutgoing ().find (metricFace.GetFace());
									  if(outgoing!=pitEntry->GetOutgoing().end())    // Vuol dire che l'interfaccia selezionata è gia tra le outgoing faces della PIT,
			  			      				                                             // quindi il succesivo cambio di metrica è consentito.
									  {
										  proceed = true;
									  }
									  else
									  {
										  proceed = false;
									  }

								  }

								  else      // ** [MT] ** The node has only one interface.
								  {
									  if (!WillSendOutInterest (metricFace.GetFace(), header, pitEntry))
									  {
										  continue;
									  }
								  }

								  //transmission
								  Ptr<Packet> packetToSend = origPacket->Copy ();
								  bool txOk = metricFace.GetFace()->Send (packetToSend);
//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << " METRICA PIU' ALTA INCREMENTO CONSENTITO TX RESULT:\t" << txOk << "\tInterface: " << metricFace.GetFace()->GetId());
								  if(!txOk)
									  super::m_dropInterestFailure();
								  else
								  {
									  DidSendOutInterest (inFace, metricFace.GetFace(), header, origPacket, pitEntry);

									  propagatedCount++;

									  pitEntry->AddOutgoing (metricFace.GetFace());

									  pitEntry->IncreaseAllowedRetxCount ();   // ** [MT] ** Verify its validity
								  }
							  }

							  else     // ** [MT] ** Another increment is not allowed, so break.
								  break;
			  		  }  // Different routing cost and already forwarder Interests

	  		  	  } // Different routing cost

		  	  } // Routing cost != 0

		  }  // End BOOST_FOREACH

	  }  // Other increments are allowed

    }
  }

  //NS_LOG_UNCOND ("NODE:\t" << inFace->GetNode()->GetId() << "\tPropagated to " << propagatedCount << " faces");
//NS_LOG_UNCOND("NODO: " << inFace->GetNode()->GetId() << " PROPAGETED COUNT SELECTED:\t" << propagatedCount << "\t CONTENUTO: " << header->GetName());
return propagatedCount > 0;
}

} // namespace fw
} // namespace ndn
} // namespace ns3

