/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Polytechnic of Bari, Italy
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
 * Author: Michele Tortelli <m.tortelli@poliba.it>
 *
 * This code is mostly inspired and adapted from the Open Bloom Filter code,
 * written by Arash Partow in 2000 (http://www.partow.net).
 * In this implementation, differently from the one by Arash Partow,
 * boost dynamic_bitset structure is used to represent Bloom Filters
 * and to implement all the characteristics of the Stable Bloom Filters.
 */



#include <iostream>
#include <math.h>
#include <cstddef>
#include <cstdio>
#include <climits>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <deque>
#include <set>
#include <string>

#include "ndn-bloom-filter-base.h"
#include "ns3/log.h"


namespace ns3 {
namespace ndn {

//NS_LOG_COMPONENT_DEFINE (BloomFilterBase::GetLogName ().c_str ());
NS_LOG_COMPONENT_DEFINE ("ndn.bf");
NS_OBJECT_ENSURE_REGISTERED (BloomFilterBase);

std::string
BloomFilterBase::GetLogName ()
{
  return "ndn.bf";
}

TypeId
BloomFilterBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::BloomFilterBase") // cheating ns3 object system
    .SetParent<Object> ()
    .SetGroupName ("Ndn")

    .AddAttribute ("NodeInterfaces",
                   "Number of interfaces of the node",
                   StringValue ("1"),
                   MakeUintegerAccessor (&BloomFilterBase::numberOfInterfaces),
                   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("Scope",
                   "Scope the filter is used for",
                   StringValue ("fib"),
                   MakeStringAccessor (&BloomFilterBase::scope),
                   MakeStringChecker ())

    .AddAttribute ("InitMethod",
                   "Filter Initialization Method (Optimal or Custom)",
                   StringValue ("optimal"),
                   MakeStringAccessor (&BloomFilterBase::initMethod),
                   MakeStringChecker ())

    .AddAttribute ("ContentCatalog",
                   "User Inserted Content Catalog Cardinality",
                    StringValue ("1000"),
                    MakeUintegerAccessor (&BloomFilterBase::insertedContentCatalog),
                    MakeUintegerChecker<uint64_t> ())

    .AddAttribute ("DesiredPfp",
                   "User Inserted Probability of False Positives",
                    StringValue ("0.01"),
                    MakeDoubleAccessor (&BloomFilterBase::insertedPfp),
                    MakeDoubleChecker<double> ())

    .AddAttribute ("CellWidth",
                   "User Inserted Cell Width",
                    StringValue ("1"),
                    MakeUintegerAccessor (&BloomFilterBase::insertedCellWidth),
                    MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("FilterCustomLength",
                   "User Inserted Custom Length of the Filter",
                    StringValue ("1000"),
                    MakeUintegerAccessor (&BloomFilterBase::insertedCustomFilterLength),
                    MakeUintegerChecker<uint64_t> ())

    .AddAttribute ("FilterCustomHashes",
                   "User Inserted Number of Hashes",
                    StringValue ("3"),
                    MakeUintegerAccessor (&BloomFilterBase::insertedCustomHashes),
                    MakeUintegerChecker<uint32_t> ())
 ;
  return tid;
}


BloomFilterBase::BloomFilterBase()
{
	bloomFilter = new std::vector<boost::dynamic_bitset<> > ();
	bfSeeds = new std::vector<uint32_t> ();
	bfInsertedElements = new std::vector<uint32_t> ();
	bfDeletedElements = new std::vector<uint32_t> ();
	//bfParam = new BloomFilterBase::bfParameters ();
}

BloomFilterBase::~BloomFilterBase()
{
NS_LOG_UNCOND("DISTRUTTORE BLOOM STABLE!");

	if(bloomFilter)
		delete bloomFilter;
	if(bfSeeds)
		delete bfSeeds;
	if(bfInsertedElements)
		delete bfInsertedElements;
	if(bfDeletedElements)
		delete bfDeletedElements;
	if(bfParam)
		delete bfParam;
}

void BloomFilterBase::MakeBfInitialization()
{
	randomSeed = (0xA5A5A5A5 + 1);

        bfParam = new bfParameters (0, 0, 0, 0, 0, 0, 0.0, 0);
        SetBloomFilterFlag(false);

        numberOfInterfaces = m_forwardingStrategy->GetObject<Node>()->GetNDevices();

        if(numberOfInterfaces > 1)
	{
		SetBloomFilterFlag(true);

	    if(scope.compare("cache") == 0)    // The user wants to employ BFs only for the CACHE
	    {
	    	if(initMethod.compare("optimal") == 0)
	    	{
	    		InitBloomFilterOptimal(insertedContentCatalog, insertedPfp, insertedCellWidth, 1);   // It is just one BF for the cache
	    	}
	    	else if(initMethod.compare("custom") == 0)
	    	{
	    		InitBloomFilterCustom(insertedContentCatalog, insertedPfp, insertedCellWidth, 1, insertedCustomFilterLength, insertedCustomHashes);
	    	}
	    	else
	    	{
	    		NS_LOG_UNCOND("Incorrect initialization method for the Bloom Filter. Please choose between 'optimal' and 'custom'!\t");
	    		exit(1);
	    	}
	    }

	    else if(scope.compare("fib") == 0)    // The user wants to employ BFs only for the FIB
	    {
	    	if(initMethod.compare("optimal") == 0)
	    	{
	    		InitBloomFilterOptimal(insertedContentCatalog, insertedPfp, insertedCellWidth, numberOfInterfaces);
    			//NS_LOG_UNCOND("*** ENTRATO NELLO SCOPE FIB-STABLE-OPTIMAL ***" << "\n CATALOG:\t" << contentCatalogFib << "\n PFP:\t" << desiredPfpFib << "\n CELL W:\t" << cellWidthBfFib << "\n INTERFACCE:\t" << numInterf);
	    	}
	    	else if(initMethod.compare("custom") == 0)
	    	{
	    		InitBloomFilterCustom(insertedContentCatalog, insertedPfp, insertedCellWidth, numberOfInterfaces, insertedCustomFilterLength, insertedCustomHashes);
	    	}
	    	else
	    	{
	    		NS_LOG_UNCOND("Incorrect initialization method for the Bloom Filter. Please choose between 'optimal' and 'custom'!\t");
	    		exit(1);
	    	}
	    }
	}

}


template<typename T>
void BloomFilterBase::InsertFootprint(const T& t, const std::size_t interface)
{
	// Note: T must be a C++ POD type.
	InsertFootprint(reinterpret_cast<const unsigned char*>(&t),sizeof(T), interface);
}

template<typename InputIterator>
void BloomFilterBase::InsertFootprint(const InputIterator begin, const InputIterator end, const std::size_t interface)
{
	InputIterator itr = begin;
    while (end != itr)
    {
    	InsertFootprint(*(itr++), interface);
    }
}

// RESET FOOTPRINT CELLS

void BloomFilterBase::ResetFootprintCells(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface)
{

      std::size_t bit_index = 0;
      std::size_t bit = 0;

      for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
      {

    	  //ComputeIndicesStableFib(hash_ap(key_begin,length,bfStableCacheSeeds[i]),bit_index,bit);
    	  ComputeIndicesFilter(MurmurHash3(key_begin,length,bfSeeds->operator [](i)),bit_index,bit);
    	  SetZeroFilterCell(bit_index, interface);

      }

      ++bfDeletedElements->operator [](interface);

}

template<typename T>
   void BloomFilterBase::ResetFootprintCells(const T& t, const std::size_t interface)
   {
      // Note: T must be a C++ POD type.
	BloomFilterBase::ResetFootprintCells(reinterpret_cast<const unsigned char*>(&t),sizeof(T), interface);
   }

   void BloomFilterBase::ResetFootprintCells(const std::string& key, const std::size_t interface)
   {
	BloomFilterBase::ResetFootprintCells(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), interface);
   }

template<typename InputIterator>
   void BloomFilterBase::ResetFootprintCells(const InputIterator begin, const InputIterator end, const std::size_t interface)
   {
      InputIterator itr = begin;
      while (end != itr)
      {
    	  BloomFilterBase::ResetFootprintCells(*(itr++), interface);
      }
   }


// DECREMENT FOOTPRINT CELLS

inline void BloomFilterBase::DecrementFootprintCells(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface)
{

      std::size_t bit_index = 0;
      std::size_t bit = 0;

      for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
      {

    	  //ComputeIndicesStableFib(hash_ap(key_begin,length,bfStableCacheSeeds[i]),bit_index,bit);
    	  ComputeIndicesFilter(MurmurHash3(key_begin,length,bfSeeds->operator [](i)),bit_index,bit);
    	  DecrementFilterCell(bit_index, interface);

      }

      ++bfDeletedElements->operator [](interface);

}

template<typename T>
   inline void BloomFilterBase::DecrementFootprintCells(const T& t, const std::size_t interface)
   {
      // Note: T must be a C++ POD type.
	BloomFilterBase::DecrementFootprintCells(reinterpret_cast<const unsigned char*>(&t),sizeof(T), interface);
   }

inline void BloomFilterBase::DecrementFootprintCells(const std::string& key, const std::size_t interface)
   {
	BloomFilterBase::DecrementFootprintCells(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), interface);
   }

template<typename InputIterator>
   inline void BloomFilterBase::DecrementFootprintCells(const InputIterator begin, const InputIterator end, const std::size_t interface)
   {
      InputIterator itr = begin;
      while (end != itr)
      {
    	  BloomFilterBase::DecrementFootprintCells(*(itr++), interface);
      }
   }



// INCREMENT FILTER CELL
bool BloomFilterBase::IncrementFilterCell (std::size_t cell, std::size_t interface)
{
   if (cell > bfParam->bfRawLengthBit)
   {
	   std::cout << "Cell index greater than filter size! Element not inserted!";
       exit(1);
   }

   std::size_t msb = bloomFilter->operator [](interface).size() - (cell * bfParam->bfCellWidth) - 1;
   std::size_t lsb = msb - (bfParam->bfCellWidth - 1);
   std::size_t i;

   for (i=lsb; i<=msb; i++)
   {
	   if(!bloomFilter->operator [](interface)[i])
	   {
		   bloomFilter->operator [](interface)[i] = true;
		   while (i && i>lsb)
		   {
			   bloomFilter->operator [](interface)[--i] = false;
		   }
		   return true;
	   }
   }
   return false;
}


// DECREMENT FILTER CELL
bool BloomFilterBase::DecrementFilterCell (std::size_t cell, std::size_t interface)
{
	   if (cell > bfParam->bfRawLengthBit) {

		   std::cout << "Cell index greater than filter size! Element not inserted!";
		   exit(1);
	   }

	   std::size_t msb = bloomFilter->operator [](interface).size() - (cell * bfParam->bfCellWidth) - 1;
	   std::size_t lsb = msb - (bfParam->bfCellWidth - 1);
	   std::size_t i;

	   for (i = lsb; i <= msb; i++)
	   {
		  if (bloomFilter->operator [](interface)[i])
		  {
			  bloomFilter->operator [](interface)[i] = false;
			  while (i && i > lsb)
			  {
				  bloomFilter->operator [](interface)[--i] = true;
			  }
			return true;
		  }
	   }
	   return false;
}


// SET MAX FILTER CELL
bool BloomFilterBase::SetMaxFilterCell(std::size_t cell, std::size_t interface)
{
   if (cell > bfParam->bfRawLengthBit) {
       std::cout << "Cell number greater than the filter size! Impossible to insert the element!";
   	   exit(1);
   }

   std::size_t msb = bloomFilter->operator [](interface).size() - (cell * bfParam->bfCellWidth) - 1;
   std::size_t lsb = msb - (bfParam->bfCellWidth - 1);
   std::size_t i;

   for (i = lsb; i <= msb; i++) {
	   bloomFilter->operator [](interface)[i] = true;
   }
   return true;
}


// SET ZERO FILTER CELL
inline bool BloomFilterBase::SetZeroFilterCell(std::size_t cell, std::size_t interface)
{
	if (cell > bfParam->bfRawLengthBit) {
	       std::cout << "Cell number greater than the filter size! Impossible to insert the element!";
	   	   exit(1);
	}

	std::size_t msb = bloomFilter->operator [](interface).size() - (cell * bfParam->bfCellWidth) - 1;
	std::size_t lsb = msb - (bfParam->bfCellWidth - 1);
	std::size_t i;

	for (i = lsb; i <= msb; i++) {
	   bloomFilter->operator [](interface)[i] = false;
	}
	return true;
}


// LOOKUP

inline bool BloomFilterBase::LookupFilter(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface)
{
	std::size_t bit_index = 0;
    std::size_t bit = 0;

	for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
	{
	    //ComputeIndicesStableFib(hash_ap(key_begin, length, bfStableFibSeeds[i]),bit_index,bit);
	    ComputeIndicesFilter(MurmurHash3(key_begin, length, bfSeeds->operator [](i)),bit_index,bit);

	    uint32_t counter = CountCell(bit_index, interface);
		if (counter==0)
		{
			  return false;
		}
	}
	return true;
}


template<typename T>
   inline bool BloomFilterBase::LookupFilter(const T& t, const std::size_t interface)
   {
      return BloomFilterBase::LookupFilter(reinterpret_cast<const unsigned char*>(&t),static_cast<std::size_t>(sizeof(T)), interface);
   }
   inline bool BloomFilterBase::LookupFilter(const std::string& key, const std::size_t interface)
   {
      return BloomFilterBase::LookupFilter(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), interface);
   }

template<typename InputIterator>
   inline bool BloomFilterBase::LookupFilter(const InputIterator begin, const InputIterator end, const std::size_t interface)
   {
      InputIterator itr = begin;
      while (end != itr)
      {
         return BloomFilterBase::LookupFilter(*(itr++), interface);
      }
   }


// COUNT

inline uint32_t BloomFilterBase::CountCell(std::size_t cell, std::size_t interface) const
{

	if (cell > bfParam->bfRawLengthBit) {
	       std::cout << "Cell number greater than the filter size! Impossible to count the element!";
	   	   exit(1);
	}

	uint32_t counter = 0;
	uint32_t numPos = 0;

	std::size_t msb = bloomFilter->operator [](interface).size() - (cell * bfParam->bfCellWidth) - 1;
	std::size_t lsb = msb - (bfParam->bfCellWidth - 1);
	std::size_t i;

	for (i=lsb; i<=msb; i++)
	{
       if (bloomFilter->operator [](interface)[i]) {
    	   counter = counter + std::pow(2,numPos);
    	   numPos++;
       }
	}
    return counter;
}


// ** [MT] ** Function that constructs the FIB Entry for the requested content. It will contain the ordered interfaces and the respective metrics.

BloomFilterBase::bestFibEntry& BloomFilterBase::LookupOrderedBloomFilters (Ptr<const Interest> header, const Ptr<Face> &inFace) const
{
	bestFibEntry* bfFibEntry = new bestFibEntry ();

	uint32_t idIncFace = inFace->GetId();

	TypeId faceType = inFace->GetInstanceTypeId();
	std::string faceType_name = faceType.GetName();

	uint32_t nodeInterfaces = m_l3prot->GetObject<Node>()->GetNDevices();

	bool evict = false;

	if(faceType_name.compare("ns3::ndn::AppFace") == 0)                // The Interest comes from the App level, so the lookup will be done on all
																	   // the NetDeviceInterfaces.
	{
		evict = false;
		bfFibEntry->orderedFaces.resize(nodeInterfaces,NULL);
		bfFibEntry->metrics.resize(nodeInterfaces,0);
	}
	else
	{
		evict = true;
		bfFibEntry->orderedFaces.resize(nodeInterfaces,NULL);
		bfFibEntry->metrics.resize(nodeInterfaces,0);
	}


	std::stringstream ss;
	std::string fullName;

	ss << header->GetName();
	fullName = ss.str();
	ss.str("");

	std::vector<std::vector<uint32_t> > * orderedInt = MakeCustomLookupBloomFilters(fullName, header, idIncFace, nodeInterfaces, evict);

	bfFibEntry->found = true;
	bfFibEntry->prefix = header->GetNamePtr();    // The content name is associated to the FIB entry

	std::vector<Ptr<Face> >::iterator it = bfFibEntry->orderedFaces.begin();

	for(uint32_t z = 0; z < orderedInt->size(); z++)
    {
    	(*it).operator =(m_l3prot->GetFace(orderedInt->operator [](z)[0]));
//NS_LOG_UNCOND("FACE: " << orderedInt->operator [](z)[0] << "\t METRIC: " << orderedInt->operator [](z)[1] << "\tNODO:\t" << inFace->GetNode()->GetId());
		bfFibEntry->metrics.operator [](z) = orderedInt->operator [](z)[1];
		it++;
    }

    delete orderedInt;

    return (*bfFibEntry);
}

/**
 *   Check the presence of the requested content name inside the SBFs of the node. Step by step, the hierarchical name is deprived of the
 *   last component. The function returns a list containing the interfaces of the node which are ordered according to the length of the
 *   name (i.e., number of components) their SBF provides a match for.
 *   This function is called by the "lookup_bf" function, which in turn is called by the "create_type" function of the ndn-pit-impl.cc file.
 *
 *   @param
 *   key_begin = content name;
 *   length = length of the content name string;
 *   header = Interest header;
 *   inc_interface = incoming interface;
 *   num_interfaces = number of interfaces of the node;
 *   evict = evict or not the incoming interface form the research.
 *
 *   @return
 *   Matrix containing the IDs of the ordered interfaces of the node, and their respective routing costs.
 */


inline std::vector<std::vector<uint32_t> > * BloomFilterBase::MakeCustomLookupBloomFilters(const unsigned char* key_begin, const std::size_t length, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const
{

	//NS_LOG_UNCOND("Interest incoming interface:\t" << inc_interface);
    //NS_LOG_UNCOND("NODE:\t" << this->m_forwardingStrategy->GetObject<Node>()->GetId() << "\tLOOKUP Interest:\t" << header->GetName() << "\t received on Interface:\t" << inc_interface);

    std::vector<bool> insertedInterfaces(nodeInterfaces, false);   // ** [MT] ** Vector to keep track of the interfaces that are progressively inserted.
    uint32_t countInsInterf = 0;								   // ** [MT] ** Counter for the inserted interfaces.

    // ** [MT] ** Matrix containing the IDs of the interfaces of the node, ordered according to the number of matchings with the components of the content name.
    //            Every row is composed by the ID of the interface and by the associated metric.
    std::vector<std::vector<uint32_t> > * orderedInterfaces = new std::vector<std::vector<uint32_t> > ();

    uint32_t check;
	uint32_t metric = 1;
	//uint32_t maxCost = std::numeric_limits<uint32_t>::max ();

	if(evict)        // ** [MT] ** The incoming interface must be avoided.
	{
        countInsInterf = nodeInterfaces - 1;
        check = countInsInterf;
        insertedInterfaces[idIncInterface] = true;
	}
	else
	{
		countInsInterf = nodeInterfaces;
		check = countInsInterf;
	}

	// ** [MT] ** Initialize the matrix to be returned
	orderedInterfaces->resize(nodeInterfaces);
	for (std::size_t i = 0; i < nodeInterfaces; ++i)
    {
	   orderedInterfaces->operator [](i).resize(2, 0);
	}


	// ** [MT] ** Searching in the SBFs

	std::size_t bitIndex = 0;
    std::size_t bit = 0;

    std::vector<uint32_t> countersInterf (nodeInterfaces, 0);    	// ** [MT] ** Vector containing the counters associated with each interface.

    std::vector<bool> present (nodeInterfaces, true);               	// ** [MT] ** As soon as one of the 'k' query is not satisfied, it means that
    																	//			  the element is not in the SBF; so the counter of the corresponding
    																	//            interface must be put to zero.

	if (evict)
       present[idIncInterface] = false;


	// ** [MT] ** The content name is extracted from the header; each name component will be inserted in a separate
	//            component of the string list.

	uint32_t numNameComponents = header->GetNamePtr()->GetComponents().size();
	bool first = true;
	std::list<std::string> interestName (numNameComponents);
	interestName.assign(header->GetNamePtr()->GetComponents().begin(), header->GetNamePtr()->GetComponents().end());
    std::list<std::string>::iterator it;
    it = interestName.begin();                 // The iterator points to the first component.
    std::stringstream ss;
    std::string prefixName;


	// ** [MT] ** FOR cycle for every component of the name
    /* It starts from the hash of the entire name and, step by step, the last component is evicted. At each step, it checks the interfaces
     * that provide a match for the current name; in case of a positive outcome, they are inserted inside the vector 'insertedInterfaces'.
     * Furthermore, at each step the routing cost is increased by one. After having inserted all the interfaces, the searching procedure
     * stops, without continuing to evict components from the name.
     */
	for(uint32_t t = numNameComponents; t > 0 ; t--)
    {
    	if(countInsInterf == 0)               // ** [MT] ** It means that all the interfaces have been ordered and added to the vector.
    	{
			if(evict == true)                 // ** [MT] ** The incoming interface is even inserted in the vector containing the ordered interfaces,
											  //            but it is inserted in the last position.
			{
				orderedInterfaces->operator [](check)[0] = idIncInterface;
				//orderedInterfaces->operator [](check)[1] = maxCost;         			// ** [MT] ** Put a higher metric.
                                orderedInterfaces->operator [](check)[1] = 1000;
			}
			return orderedInterfaces;
		}

    	if(!first)
    		interestName.pop_back();         // ** [MT] ** Evict the last component of the name.

    	for(uint32_t d=1; d <=t; d++ )
    	{
    		ss << "/" << (*it);
    		it++;
    	}
    	prefixName = ss.str();
    	first = false;
    	//NS_LOG_UNCOND("Searching for Interest name: \t" << composed_name);
    	it = interestName.begin();
    	ss.str("");

        const unsigned char* prefName = reinterpret_cast<const unsigned char*>(prefixName.c_str());
		const std::size_t prefixSize = prefixName.size();

        // ** [MT] ** Searching for a match for each of the 'k' hash function.
		for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
		{
		    //ComputeIndicesSimpleFib(hash_ap(prefName,prefixSize,bfSimpleFibSeeds[i]),bitIndexSimple,bitSimple);
		    ComputeIndicesFilter(MurmurHash3(prefName,prefixSize,bfSeeds->operator [](i)),bitIndex,bit);

			for (std::size_t j = 0; j < nodeInterfaces; j++)
			{
			   if (present[j])
			   {
				  uint32_t counter = CountCell(bitIndex, j);
				  if (counter!=0)
				  {
					  countersInterf[j] += counter;
				  }
				  else
				  {
					  present[j] = false;
					  countersInterf[j] = 0;
				  }
			  }
		   }
		}
                
                //NS_LOG_UNCOND("CONTATORI INTERFACCE NODO:\t" << this->m_forwardingStrategy->GetObject<Node>()->GetId() << "\t" << Simulator::Now().GetMicroSeconds());
                //for (std::size_t j = 0; j < nodeInterfaces; j++)
		//{
                //        NS_LOG_UNCOND("INTERFACCIA: " << j << "\t CONTATORE: " << countersInterf[j]);
                //}


		// ** [MT] ** If multiple interfaces provide a match for the current name, they are ordered according to their counter, which is
		//            incremented by the value of the counter of the matched 'j-th' cell (with 1<=j<=k). The bigger the counter, the
		//            smaller the routing cost. This is done because a higher counter could indicate a more recent caching operation
		//            of the required content towards the selected interface. Remember that for a SBF, at every insertion 'p' cells
		//            are decremented by 1, while 'k' cells are set to Max.
		std::vector<uint32_t>::iterator iterMax;
		uint32_t intMax;

		for(uint32_t d = 0; d < countersInterf.size(); d++)
		{
		   iterMax = std::max_element(countersInterf.begin(), countersInterf.end());

		   if(*iterMax!=0)       // The counter of an interface which has not provided a match is zero.
		   {
		  	   // ** [MT] ** Retrieve the interface ID through the index of the max element.
			   intMax = std::distance(countersInterf.begin(), max_element(countersInterf.begin(), countersInterf.end()));
			   //NS_LOG_UNCOND("NODE:\t" << this->m_forwardingStrategy->GetObject<Node>()->GetId() << "\t Found matching with name of length:\t" << t << "over\t" << numNameComponents << "NAME:\t" << composed_name << "\tMAX FOUND:\t" << *iterMaxSBF << "\t AT POSITION:\t" << intMaxSBF << "\t" << Simulator::Now().GetNanoSeconds());
			   //NS_LOG_UNCOND("NODE:\t" << this->m_forwardingStrategy->GetObject<Node>()->GetId() << "\t Inserting the interface:\t" << intMax  << "\t with counter:\t" << countersInterf[intMax] << "\t" << Simulator::Now().GetMicroSeconds());
		  	   orderedInterfaces->operator [](check-countInsInterf)[0] = intMax;    	     // ** Add the selected interface.
		  	   orderedInterfaces->operator [](check-countInsInterf)[1] = metric;            // ** Add the respective metric.
		  	   //NS_LOG_UNCOND("NODE:\t" << this->m_forwardingStrategy->GetObject<Node>()->GetId() << "\t Metric for the inserted interface:\t" << orderedInterfaces->operator [](check-countInsInterf)[1] << "\t" << Simulator::Now().GetMicroSeconds());
		  	   countInsInterf--;
		  	   insertedInterfaces[intMax] = true;
		  	   countersInterf[intMax] = 0;
		   }
		   else
		   {
			   break;       // There are not any more interfaces whose SBF provides a match with the current name.
		   }
		}

		metric++;   		// Increase the metric.

		// ** [MT] **  For the already inserted interface(s), the flag 'present' is set to false in order to avoid them in the next
		//             step (when the searching will be done with the current name without the last component).
		for(uint32_t j = 0; j < nodeInterfaces; j++)
		{
			if(!insertedInterfaces[j])
				present[j] = true;
			else
			{
				present[j] = false;
				countersInterf[j] = 0;
			}
		}

		if(evict)
			present[idIncInterface] = false;

    }

	if(countInsInterf == 0)  // ** [MT] ** A match has been found for all the interfaces, so the research can be stopped.
	{
		//NS_LOG_UNCOND("SBF: VERIFY LOOKUP PROCESS...\t" << "Interfaces of the node:\t" << check << "\t Returned interfaces:\t" << orderedInterfaces->size());
		if(evict == true)
		{
			orderedInterfaces->operator [](check)[0] = idIncInterface;
			//orderedInterfaces->operator [](check)[1] = maxCost;
                        orderedInterfaces->operator [](check)[1] = 1000;
		}

		return orderedInterfaces;
	}


	else    // ** [MT] ** There are still other interfaces to be "classified" after having finished all the name components. All these interfaces
		    //            will be associated with the same metric.
	{
  		for(uint32_t p = 0; p < insertedInterfaces.size(); p++)
  		{
  		   if(insertedInterfaces[p]== false)
  		   {
  			   orderedInterfaces->operator [](check-countInsInterf)[0] = p;
       	  	   orderedInterfaces->operator [](check-countInsInterf)[1] = metric;
			   insertedInterfaces[p] = true;
			   countInsInterf--;
		   }
  		}

		if(countInsInterf == 0 && evict == true)
		{
			orderedInterfaces->operator [](check)[0] = idIncInterface;
			//orderedInterfaces->operator [](check)[1] = maxCost;
                        orderedInterfaces->operator [](check)[1] = 1000;
		}

    	return orderedInterfaces;
  	}
}

template<typename T>
inline std::vector<std::vector<uint32_t> > * BloomFilterBase::MakeCustomLookupBloomFilters(const T& t, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const
{
   return MakeCustomLookupBloomFilters(reinterpret_cast<const unsigned char*>(&t),static_cast<std::size_t>(sizeof(T)), header, idIncInterface, nodeInterfaces, evict);
}
inline std::vector<std::vector<uint32_t> > * BloomFilterBase::MakeCustomLookupBloomFilters(const std::string& key, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const
{
   return MakeCustomLookupBloomFilters(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), header, idIncInterface, nodeInterfaces, evict);
}
inline std::vector<std::vector<uint32_t> > * BloomFilterBase::MakeCustomLookupBloomFilters(const char* data, const std::size_t& length, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const
{
   return MakeCustomLookupBloomFilters(reinterpret_cast<const unsigned char*>(data),length, header, idIncInterface, nodeInterfaces, evict);
}


void BloomFilterBase::ClearFilter(uint32_t interface)
{
	bloomFilter->operator [](interface).reset();
}

void BloomFilterBase::SetFilter(uint32_t interface)
{
	bloomFilter->operator [](interface).set();
}

void BloomFilterBase::SetBloomFilterFlag(bool value)
{
	bloomFilterFlag = value;
}

bool BloomFilterBase::GetBloomFilterFlag()
{
	return bloomFilterFlag;
}

void
BloomFilterBase::SetForwardingStrategy(const Ptr<ForwardingStrategy> fwS)
{
	m_forwardingStrategy = fwS;
}

Ptr<ForwardingStrategy>
BloomFilterBase::GetForwardingStrategy() const
{
	return m_forwardingStrategy;
}

void
BloomFilterBase::SetL3Protocol(const Ptr<L3Protocol> l3Prot)
{
	m_l3prot = l3Prot;
}

Ptr<L3Protocol>
BloomFilterBase::GetL3Protocol() const
{
	return m_l3prot;
}



inline void BloomFilterBase::ComputeIndicesFilter(const uint32_t& hash, std::size_t& bit_index, std::size_t& bit) const
{
    bit_index = hash % bfParam->bfRawLengthBit;
    bit = bit_index % bits_per_char;
}


void BloomFilterBase::GenerateSeedsHash()
{
   // To obtain different hash functions, the same function is seeded with different seeds.

   srand(static_cast<uint32_t>(randomSeed));
   while (bfSeeds->size() < bfParam->bfNumHashFunctions)
   {
       uint32_t currentSeed = static_cast<uint32_t>(rand()) * static_cast<uint32_t>(rand());
       if (currentSeed == 0) continue;
       if (std::find(bfSeeds->begin(), bfSeeds->end(), currentSeed) == bfSeeds->end())
       {
          bfSeeds->push_back(currentSeed);
       }
   }
}


// ** Implementation of the Arash Partow hash function.

inline uint32_t BloomFilterBase::hash_ap(const unsigned char* begin, std::size_t remaining_length, uint32_t hash) const
{
    const unsigned char* itr = begin;
    unsigned int loop = 0;
    while (remaining_length >= 8)
    {
        const unsigned int& i1 = *(reinterpret_cast<const unsigned int*>(itr)); itr += sizeof(unsigned int);
        const unsigned int& i2 = *(reinterpret_cast<const unsigned int*>(itr)); itr += sizeof(unsigned int);
        hash ^= (hash <<  7) ^  i1 * (hash >> 3) ^ (~((hash << 11) + (i2 ^ (hash >> 5))));
        remaining_length -= 8;
    }
    while (remaining_length >= 4)
    {
       const unsigned int& i = *(reinterpret_cast<const unsigned int*>(itr));
       if (loop & 0x01)
           hash ^=    (hash <<  7) ^  i * (hash >> 3);
       else
           hash ^= (~((hash << 11) + (i ^ (hash >> 5))));
       ++loop;
       remaining_length -= 4;
       itr += sizeof(unsigned int);
    }
    while (remaining_length >= 2)
    {
       const unsigned short& i = *(reinterpret_cast<const unsigned short*>(itr));
       if (loop & 0x01)
           hash ^=    (hash <<  7) ^  i * (hash >> 3);
       else
           hash ^= (~((hash << 11) + (i ^ (hash >> 5))));
       ++loop;
       remaining_length -= 2;
       itr += sizeof(unsigned short);
    }
    if (remaining_length)
    {
       hash += ((*itr) ^ (hash * 0xA5A5A5A5)) + loop;
    }
    return hash;
}

// ** Implementation of the Murmur3 hash function
inline uint32_t BloomFilterBase::MurmurHash3 (const unsigned char * key, std::size_t len, uint32_t seed) const
{
  const uint8_t * data = (const uint8_t*)key;
  const int nblocks = len / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  //----------
  // body

  const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

  for(int i = -nblocks; i; i++)
  {
    //uint32_t k1 = getblock32(blocks,i);
	uint32_t k1 = blocks[i];

    k1 *= c1;
    //k1 = ROTL32(k1,15);
    k1 = k1 << 15 | k1 >> (32 - 15);
    k1 *= c2;
    
    h1 ^= k1;
    //h1 = ROTL32(h1,13); 
    h1 = h1 << 13 | h1 >> (32 - 13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail

  const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

  uint32_t k1 = 0;

  switch(len & 3)
  {
  case 3: k1 ^= tail[2] << 16;
  case 2: k1 ^= tail[1] << 8;
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = k1 << 15 | k1 >> (32 - 15); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;

  //h1 = fmix32(h1);
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;


  //*(uint32_t*)out = h1;
  return h1;
}


/*void BloomFilterBase::NotifyNewAggregate ()
{
   if (m_forwardingStrategy == 0)
   {
      m_forwardingStrategy = GetObject<ForwardingStrategy> ();
   }
   if (m_l3prot == 0)
   {
      m_l3prot = GetObject<L3Protocol> ();
   }

   Object::NotifyNewAggregate();
}*/




}  // ndn
}  // ns3



