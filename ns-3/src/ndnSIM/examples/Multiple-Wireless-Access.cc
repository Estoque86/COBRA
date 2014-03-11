/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/lte-module.h"

#include "ns3/ndnSIM-module.h"

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ndn.MultipleWirelessAccess");

int
main (int argc, char *argv[])
{
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("OfdmRate24Mbps"));

  uint32_t nCsma = 3;
  uint32_t nWifi = 2;
  uint32_t nenbNodes = 1;
  uint32_t nueNodes = 1;

  CommandLine cmd;
  cmd.AddValue ("nCsma", "Number of extra CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("enbNodes", "Number of eNodeB", nenbNodes);
  cmd.AddValue ("ueNodes", "Number of LTE Devices", nueNodes);

  cmd.Parse (argc,argv);

  ////  P2P NODES

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  ////  CSMA NODES

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  ////  WIFI NODES

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  YansWifiChannelHelper wifiChannel;// = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

  //YansWifiPhy wifiPhy = YansWifiPhy::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.SetChannel (wifiChannel.Create ());
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(5));
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(5));

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("ndn.ssid");
  wifiMacHelper.SetType("ns3::NqstaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, wifiStaNodes);

  // Access Point configuration
  wifiMacHelper.SetType ("ns3::NqapWifiMac", "Ssid", SsidValue (ssid), "BeaconGeneration", BooleanValue (true), "BeaconInterval", TimeValue (Seconds (2.5)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, wifiApNode);


  ////  LTE Nodes
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  NodeContainer enbNodes;
  enbNodes.Create(nenbNodes);
  NodeContainer ueNodes;
  ueNodes.Create(nueNodes);
  ueNodes.Add(wifiStaNodes.Get(0));

  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  lteHelper->Attach (ueDevs, enbDevs.Get(0));

  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VIDEO;
  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (ueDevs, bearer);

  // MOBILITY MODEL

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX", DoubleValue (0.0),
    "MinY", DoubleValue (0.0),
    "DeltaX", DoubleValue (5.0),
    "DeltaY", DoubleValue (10.0),
    "GridWidth", UintegerValue (3),
    "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
      "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));


  NodeContainer mobileNodes;
  mobileNodes.Add(wifiStaNodes);
  mobileNodes.Add(ueNodes.Get(0));

  mobility.Install (mobileNodes);

  NodeContainer fixedNodes;
  fixedNodes.Add(wifiApNode);
  fixedNodes.Add(enbNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (fixedNodes);


  NodeContainer nodes;
  nodes.Add(csmaNodes);
  nodes.Add(wifiApNode);
  nodes.Add(wifiStaNodes);
  nodes.Add(ueNodes.Get(0));
  nodes.Add(enbNodes);

  //////////////////////
  //////////////////////

  // Install NDN stack
  NS_LOG_INFO ("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback (MyNetDeviceFaceCallback));
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Lru", "MaxSize", "1000");
  ndnHelper.SetDefaultRoutes (true);
  ndnHelper.Install(nodes);

  // 4. Set up applications
  NS_LOG_INFO ("Installing Applications");

  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix ("/test/prefix");
  consumerHelper.SetAttribute ("Frequency", DoubleValue (10.0));
  consumerHelper.Install (wifiStaNodes.Get(0));       // The Node with multiple wireless access

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetPrefix ("/");
  producerHelper.SetAttribute ("PayloadSize", StringValue("1200"));

  NodeContainer producers;
  producers.Add(csmaNodes.Get(1));
  producers.Add(csmaNodes.Get(3));
  producerHelper.Install (producers);

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll ();

  ndnGlobalRoutingHelper.AddOrigins ("/test/prefix", producers);
  ndn::GlobalRoutingHelper::CalculateRoutes ();


  ////////////////

  Simulator::Stop (Seconds (30.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
