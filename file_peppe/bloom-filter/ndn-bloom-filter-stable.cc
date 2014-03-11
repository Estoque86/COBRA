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

#include "ndn-bloom-filter-stable.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("ndn.bf.Stable");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (BloomFilterStable);

TypeId
BloomFilterStable::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::BloomFilterStable") // cheating ns3 object system
    .SetParent<BloomFilterBase> ()
    .SetGroupName ("Ndn")
    .AddConstructor<BloomFilterStable> ()

  ;
  return tid;
}


BloomFilterStable::BloomFilterStable()
{
}

BloomFilterStable::~BloomFilterStable()
{
}


// INIT BLOOM FILTER
void BloomFilterStable::InitBloomFilterOptimal (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces)
{
	NS_LOG_UNCOND("*** BF STABLE *** Number of Interfaces INIT:\t" << numberOfInterfaces);

	bfParam->bfEstimCatalogCardinality = M;
	bfParam->bfDesiredFalsePosProb = pfp;
	bfParam->bfCellWidth = cW;

	ExtractOptimum(M, pfp);

    GenerateSeedsHash();

    for (uint32_t i = 0; i < interfaces; ++i)
    {
    	boost::dynamic_bitset<> bf;
    	bf.resize(bfParam->bfLengthBit,false);
    	bloomFilter->push_back(bf);

    	bfInsertedElements->push_back((uint32_t)0);
    	bfDeletedElements->push_back((uint32_t)0);
    }
    
}

void BloomFilterStable::InitBloomFilterCustom (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces, uint64_t m, uint32_t k)
{
	bfParam->bfEstimCatalogCardinality = M;
	bfParam->bfDesiredFalsePosProb = pfp;
	bfParam->bfCellWidth = cW;

	bfParam->bfNumHashFunctions = k;

	uint32_t max = pow(2,cW)-1;

	double customLength = m / cW;                     // The desired length of the filter (m) is expressed in [bit]
	bfParam->bfRawLengthBit = static_cast<uint64_t>(customLength);
	bfParam->bfRawLengthBit += (((bfParam->bfRawLengthBit % bits_per_char) != 0) ? (bits_per_char - (bfParam->bfRawLengthBit % bits_per_char)) : 0);

	NS_LOG_UNCOND("FILTER LENGTH [bit]:\t" << bfParam->bfRawLengthBit << "\nHASH FUNCTIONS:\t" << k);

	bfParam->bfLengthBit = cW * bfParam->bfRawLengthBit;

	NS_LOG_UNCOND("FILTER LENGTH WITH CELL WIDTH[bit]:\t" << bfParam->bfLengthBit);

	bfParam->bfRawLengthByte = bfParam->bfRawLengthBit * bits_per_char;
	bfParam->bfLengthByte = bfParam->bfLengthBit * bits_per_char;

    GenerateSeedsHash();

    for (uint32_t i = 0; i < interfaces; ++i)
    {
    	boost::dynamic_bitset<> bf;
    	bf.resize(bfParam->bfLengthBit,false);
    	bloomFilter->push_back(bf);

    	bfInsertedElements->push_back((uint32_t)0);
    	bfDeletedElements->push_back((uint32_t)0);
    }

	bfStableP = floor( 1/(((1/(pow((1-pow((double)bfParam->bfDesiredFalsePosProb,1/(double)bfParam->bfNumHashFunctions)),(1/(double)max))))-1)*((1/(double)bfParam->bfNumHashFunctions))));

NS_LOG_UNCOND("*** BF STABLE *** DECREM POS:\t" << bfStableP);
}


// EXTRACT OPTIMUM

void BloomFilterStable::ExtractOptimum(uint64_t M, double pfp)
{
	NS_LOG_UNCOND("*** BF STABLE *** Extract Optimum");
	double optimalLength = round(-(M*std::log(pfp)/(std::pow(std::log(2),2))));
	double optimalNumHash = (optimalLength/M)*std::log(2);
	uint32_t max = pow(2,bfParam->bfCellWidth)-1;

	bfParam->bfRawLengthBit = static_cast<uint32_t>(optimalLength);
	bfParam->bfNumHashFunctions = static_cast<uint32_t>(optimalNumHash);

	uint64_t len_bit_no_count = bfParam->bfRawLengthBit;
	uint32_t hash = bfParam->bfNumHashFunctions;
	NS_LOG_UNCOND("*** BF STABLE *** LUNGHEZZA FILTRO [bit]:\t" << len_bit_no_count << "\nHASH FUNCTIONS:\t" << hash);

	// It is simpler to manage a filter with a length equal to a multiplier of 8.
	bfParam->bfRawLengthBit += (((bfParam->bfRawLengthBit % bits_per_char) != 0) ? (bits_per_char - (bfParam->bfLengthBit % bits_per_char)) : 0);

	bfParam->bfLengthBit = bfParam->bfCellWidth * bfParam->bfRawLengthBit;
	uint64_t len_bit_count = bfParam->bfLengthBit;
	NS_LOG_UNCOND("*** BF STABLE *** LUNGHEZZA FILTRO COUNT[bit]:\t" << len_bit_count);

	bfParam->bfRawLengthByte = bfParam->bfRawLengthBit * bits_per_char;
	bfParam->bfLengthByte = bfParam->bfLengthBit * bits_per_char;

	bfStableP = floor( 1/(((1/(pow((1-pow((double)bfParam->bfDesiredFalsePosProb,1/(double)bfParam->bfNumHashFunctions)),(1/(double)max))))-1)*((1/(double)bfParam->bfNumHashFunctions))));
	NS_LOG_UNCOND("*** BF STABLE *** DECREM POS:\t" << bfStableP);
}


// INSERT FOOTPRINT
void BloomFilterStable::InsertFootprint(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface)
{
    std::size_t bit_index = 0;
    std::size_t bit = 0;

    // ** Determine the 'p' random cells to be decremented.
    for (size_t i = 0; i < bfStableP; ++i)
    {
  	  std::size_t decCell = (uint64_t)((std::rand()%bfParam->bfRawLengthBit));
   	  DecrementFilterCell(decCell, interface);
    }

    // ** The "k" different hash functions are obtained by changing the seed of the same hash function.

    for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
    {
    	// ** The indexes are evaluated using the Arash Partow hash function.
    	//ComputeIndicesStableFib(hash_ap(key_begin,length,bfStableFibSeeds[i]),bit_index,bit);
    	ComputeIndicesFilter(MurmurHash3(key_begin,length,bfSeeds->operator [](i)),bit_index,bit);

    	// ** The evaluated 'bit_index' is passed to the function 'set_max', which determines the position of the specified cell in the SBF
    	// and sets to 'Max' the respective counter.
    	SetMaxFilterCell(bit_index, interface);

    }
    ++bfInsertedElements->operator [](interface);
}


void BloomFilterStable::InsertFootprint(const std::string& key, const std::size_t interface)
{
	BloomFilterStable::InsertFootprint(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), interface);
}

// ** [MT] ** Insert elements without decrementing the 'p' random cells.
inline void BloomFilterStable::InsertFootprintStableNoDecrement(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface)
{
   std::size_t bitIndex = 0;
   std::size_t bit = 0;

   // ** The "k" different hash functions are obtained by changing the seed of the same hash function.

   for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
   {
	   //ComputeIndicesStableFib(hash_ap(key_begin,length,bfStableFibSeeds[i]),bitIndex,bit);
	   ComputeIndicesFilter(MurmurHash3(key_begin,length,bfSeeds->operator [](i)),bitIndex,bit);
       SetMaxFilterCell(bitIndex, interface);

   }
   ++bfInsertedElements->operator [](interface);
}

template<typename T>
inline void BloomFilterStable::InsertFootprintStableNoDecrement(const T& t, const std::size_t interface)
{
   BloomFilterStable::InsertFootprintStableNoDecrement(reinterpret_cast<const unsigned char*>(&t),sizeof(T), interface);
}

inline void BloomFilterStable::InsertFootprintStableNoDecrement(const std::string& key, const std::size_t interface)
{
   BloomFilterStable::InsertFootprintStableNoDecrement(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), interface);
}

template<typename InputIterator>
inline void BloomFilterStable::InsertFootprintStableNoDecrement(const InputIterator begin, const InputIterator end, const std::size_t interface)
{
   InputIterator itr = begin;
   while (end != itr)
   {
      BloomFilterStable::InsertFootprintStableNoDecrement(*(itr++), interface);
   }
}



}  // ndn
}  // ns3



