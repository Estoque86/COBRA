/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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
 * Author:
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ns2-mobility-helper.h"

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include "ndn-v2v-net-device-face.h"
#include "v2v-tracer.h"

using namespace ns3;
using namespace boost;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("Experiment");

Ptr<ndn::NetDeviceFace>
V2vNetDeviceFaceCallback (Ptr<Node> node, Ptr<ndn::L3Protocol> ndn, Ptr<NetDevice> device)
{
  NS_LOG_DEBUG ("Creating ndn::V2vNetDeviceFace on node " << node->GetId ());

  Ptr<ndn::NetDeviceFace> face = CreateObject<ndn::V2vNetDeviceFace> (node, device);
  ndn->AddFace (face);
  // NS_LOG_LOGIC ("Node " << node->GetId () << ": added NetDeviceFace as face #" << *face);

  return face;
}

int
main (int argc, char *argv[])
{
	// disable fragmentation
	Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
	Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
	Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("OfdmRate24Mbps"));

	// vanet hacks to CcnxL3Protocol
	Config::SetDefault ("ns3::ndn::V2vNetDeviceFace::MaxDelay", StringValue ("2ms"));
	Config::SetDefault ("ns3::ndn::V2vNetDeviceFace::MaxDelayLowPriority", StringValue ("5ms"));
	Config::SetDefault ("ns3::ndn::V2vNetDeviceFace::MaxDistance", StringValue ("150"));

	// !!! very important parameter !!!
	// Should keep PIT entry to prevent duplicate interests from re-propagating
	Config::SetDefault ("ns3::ndn::Pit::PitEntryPruningTimout", StringValue ("1s"));

	std::string traceFile;

	int    nodeNum;
	double duration;

	uint32_t run = 1;
  	string batches = "2s 1";


	// Parse command line attribute
	CommandLine cmd;
	cmd.AddValue ("traceFile", "Ns2 movement trace file", traceFile);
	cmd.AddValue ("nodeNum", "Number of nodes", nodeNum);
	cmd.AddValue ("duration", "Duration of Simulation", duration);
    cmd.AddValue ("run", "Run", run);
    cmd.AddValue ("batches", "Consumer interest batches", batches);


	cmd.Parse (argc,argv);

	// Check command line arguments
	if (traceFile.empty () || nodeNum <= 0 || duration <= 0 || logFile.empty ())
	{
	    std::cout << "Usage of " << argv[0] << " :\n\n"
	    "./waf --run \"ns2-mobility-trace"
	    " --traceFile=src/mobility/examples/default.ns_movements"
	    " --nodeNum=2 --duration=100.0 --logFile=ns2-mob.log\" \n\n"
	    "NOTE: ns2-traces-file could be an absolute or relative path. You could use the file default.ns_movements\n"
	    "      included in the same directory of this example file.\n\n"
	    "NOTE 2: Number of nodes present in the trace file must match with the command line argument and must\n"
	    "        be a positive number. Note that you must know it before to be able to load it.\n\n"
	    "NOTE 3: Duration must be a positive number. Note that you must know it before to be able to load it.\n\n";

	    return 0;
	}

    Config::SetGlobal ("RngRun", IntegerValue (run));


    //////////////////////
    //////////////////////
    WifiHelper wifi = WifiHelper::Default ();
    // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
    wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("OfdmRate24Mbps"));

    YansWifiChannelHelper wifiChannel;// = YansWifiChannelHelper::Default ();
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::ThreeLogDistancePropagationLossModel");
    wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

    //YansWifiPhy wifiPhy = YansWifiPhy::Default();
    YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
    wifiPhyHelper.SetChannel (wifiChannel.Create ());
    wifiPhyHelper.Set("TxPowerStart", DoubleValue(5));
    wifiPhyHelper.Set("TxPowerEnd", DoubleValue(5));

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
    wifiMac.SetType("ns3::AdhocWifiMac");


    // 1. Install Mobility Model to Vehicles

    // Create Ns2MobilityHelper with the specified trace log file as parameter
	Ns2MobilityHelper ns2MobH = Ns2MobilityHelper (traceFile);

	// Create all nodes.
	NodeContainer vehicles;
	vehicles.Create (nodeNum);

	ns2MobH.Install (); // configure movements for each node, while reading trace file


	// 2. Create RSUs as Static Nodes at Corners c1, c3, c5, c7, and c9
	NodeContainer rsu;
	int numRsu = 5;
	rsu.Create (numRsu);

	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (500.0, 800.0, 0.0)); // RSU at c1
	positionAlloc->Add (Vector (500.0, 0.0, 0.0)); // RSU at c7
	positionAlloc->Add (Vector (900.0, 400.0, 0.0)); // RSU at c5
	positionAlloc->Add (Vector (1300.0, 800.0, 0.0)); // RSU at c3
	positionAlloc->Add (Vector (1300.0, 0.0, 0.0)); // RSU at c9

	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

	mobility.Install(rsu);

	// Merge the two Node Containers

	NodeContainer allNodes;
	allNodes.Add(vehicles);
	allNodes.Add(rsu);


	// 2. Install Wifi
	NetDeviceContainer wifiNetDevices = wifi.Install (wifiPhyHelper, wifiMac, allNodes);

    // 3. Install NDN stack
    NS_LOG_INFO ("Installing NDN stack");
    ndn::StackHelper ndnHelper;
    ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback (V2vNetDeviceFaceCallback));
    ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::V2v");
    ndnHelper.SetContentStore ("ns3::ndn::cs::Lru", "MaxSize", "10000");
    ndnHelper.SetDefaultRoutes(true);
    ndnHelper.Install(nodes);

    // 4. Set up applications
    NS_LOG_INFO ("Installing Applications");

    ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerBatches");
    consumerHelper.SetPrefix ("/very-long-prefix-requested-by-client/this-interest-hundred-bytes-long-interest");
    consumerHelper.SetAttribute ("Batches", StringValue (batches));

    ndn::AppHelper producerHelper ("ns3::ndn::Producer");
    producerHelper.SetPrefix ("/");
    producerHelper.SetAttribute ("PayloadSize", StringValue("300"));

    // Install producer and consumer both on vehicles and RSUs.
    producerHelper.Install(allNodes);
    consumerHelper.Install(allNodes);

    boost::tuple< boost::shared_ptr<std::ostream>, std::list<boost::shared_ptr<ndn::V2vTracer> > >
    	tracing = ndn::V2vTracer::InstallAll ("results/urban-grid.txt");

    Simulator::Stop (Seconds (duration));

    NS_LOG_INFO ("Starting");

    Simulator::Run ();

    NS_LOG_INFO ("Done");

    Simulator::Destroy ();

    return 0;
}
