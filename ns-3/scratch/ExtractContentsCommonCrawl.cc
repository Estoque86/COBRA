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


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-grid.h"
#include "ns3/mobility-module.h"
#include "../src/ndnSIM/helper/ndn-global-routing-helper.h"
#include "../src/ndnSIM/model/ndn-global-router.h"
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <fstream>
#include "ns3/random-variable-stream.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <boost/tokenizer.hpp>
#include <boost/ref.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>


using namespace ns3;
using namespace ndn;

NS_LOG_COMPONENT_DEFINE ("ndn.ExtractContents");




int
main (int argc, char *argv[4])
{

  // ** [MT] ** Parameters passed with command line

	uint32_t numFiles = 1;
	uint32_t numLinesPerFile = 100;
	uint32_t lineLimit = 10000000;

	CommandLine cmd;
	cmd.AddValue ("numFiles", "Number of Files the Catalog is Divided in", numFiles);
	cmd.AddValue ("numLinesPerFile", "Number of Lines that will be extracted from each file", numLinesPerFile);
	cmd.AddValue ("lineLimit", "Max Line ID that can be Extracted", lineLimit);

	cmd.Parse (argc, argv);

	Time finishTime = Seconds (5);

	std::stringstream ss;

	std::ofstream fout;


	UniformVariable m_SeqRngContents (1, lineLimit);     // The maximum random number is equal to the number of core nodes

	for(uint32_t j=0; j<=numFiles; j++)
	{
		  bool completeContents = false;

		  std::vector<uint32_t>* extractedLinesID = new std::vector<uint32_t>(numLinesPerFile) ;
		  for(uint32_t i=0; i<extractedLinesID->size(); i++)
		  {
			  extractedLinesID->operator[](i) = lineLimit+1;
		  }

		  uint32_t lineExtractedNum = 0;
		  uint32_t randLineExtracted;

		  while(!completeContents)
		  {
			  bool alreadyExtracted = false;
			  while(!alreadyExtracted)
			  {
				  randLineExtracted = (uint32_t)round(m_SeqRngContents.GetValue());

				  for(uint32_t i=0; i<extractedLinesID->size(); i++)
				  {
					  if(extractedLinesID->operator[](i)==randLineExtracted)
					  {
						alreadyExtracted = true;
						break;
					  }
				  }
				  if(alreadyExtracted==true)
					alreadyExtracted = false;        // It keeps going
				  else
					alreadyExtracted = true;
			  }

			  extractedLinesID->operator[](lineExtractedNum) = randLineExtracted;
			  lineExtractedNum = lineExtractedNum + 1;
			  if(lineExtractedNum == extractedLinesID->size())
				completeContents = true;
		  }

		  ss << "CommonCrawl/index_" << j << "_lines";
		  const char* strOutputFile = ss.str().c_str();
		  ss.str("");

		  fout.open(strOutputFile, std::ofstream::out);
		  if(!fout) {
			  std::cout << "\nERROR: Impossible to open the output file!\n";
			  exit(0);
		  }

		  for(uint32_t i=0; i < extractedLinesID->size(); i++)
		  {
			  fout << extractedLinesID->operator[](i) << std::endl;
		  }

		  fout.close();
		  delete extractedLinesID;
	}

	Simulator::Stop (finishTime);

	NS_LOG_INFO ("Run Simulation.");
	Simulator::Run ();

	Simulator::Destroy ();
	NS_LOG_INFO ("Done!");

	return 0;
}
