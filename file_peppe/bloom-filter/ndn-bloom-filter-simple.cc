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

#include "ndn-bloom-filter-simple.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("ndn.bf.Simple");

namespace ns3 {
namespace ndn {


NS_OBJECT_ENSURE_REGISTERED (BloomFilterSimple);

TypeId
BloomFilterSimple::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::BloomFilterSimple") // cheating ns3 object system
    .SetParent<BloomFilterBase> ()
    .SetGroupName ("Ndn")
    .AddConstructor<BloomFilterSimple> ()

    // VERIFICARE ATTRIBUTI
  ;
  return tid;
}


BloomFilterSimple::BloomFilterSimple()
{
}

BloomFilterSimple::~BloomFilterSimple()
{
}

// INIT BLOOM FILTER
void BloomFilterSimple::InitBloomFilterOptimal (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces)
{
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

void BloomFilterSimple::InitBloomFilterCustom (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces, uint64_t m, uint32_t k)
{
	bfParam->bfEstimCatalogCardinality = M;
	bfParam->bfDesiredFalsePosProb = pfp;
	bfParam->bfCellWidth = cW;

	bfParam->bfNumHashFunctions = k;

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
}

// EXTRACT OPTIMUM

void BloomFilterSimple::ExtractOptimum(uint64_t M, double pfp)
{
	double optimalLength = -(M*std::log(pfp)/(std::pow(std::log(2),2)));
	double optimalNumHash = (optimalLength/M)*std::log(2);

	bfParam->bfRawLengthBit = static_cast<uint32_t>(optimalLength);
	bfParam->bfNumHashFunctions = static_cast<uint32_t>(optimalNumHash);

	uint64_t len_bit_no_count = bfParam->bfRawLengthBit;
	uint32_t hash = bfParam->bfNumHashFunctions;
	NS_LOG_UNCOND("FILTER LENGTH [bit]:\t" << len_bit_no_count << "\nHASH FUNCTIONS:\t" << hash);

	// It is simpler to manage a filter with a length equal to a multiplier of 8.
	bfParam->bfRawLengthBit += (((bfParam->bfRawLengthBit % bits_per_char) != 0) ? (bits_per_char - (bfParam->bfRawLengthBit % bits_per_char)) : 0);

	bfParam->bfLengthBit = bfParam->bfCellWidth * bfParam->bfRawLengthBit;
	uint64_t len_bit_count = bfParam->bfLengthBit;
	NS_LOG_UNCOND("FILTER LENGTH WITH CELL WIDTH[bit]:\t" << len_bit_count);

	bfParam->bfRawLengthByte = bfParam->bfRawLengthBit * bits_per_char;
	bfParam->bfLengthByte = bfParam->bfLengthBit * bits_per_char;
}


// INSERT FOOTPRINT
void BloomFilterSimple::InsertFootprint(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface)
{
    std::size_t bit_index = 0;
    std::size_t bit = 0;

    for (std::size_t i = 0; i < bfParam->bfNumHashFunctions; ++i)
    {
       ComputeIndicesFilter(MurmurHash3(key_begin,length,bfSeeds->operator [](i)),bit_index,bit);

       IncrementFilterCell(bit_index, interface);
    }
    ++bfInsertedElements->operator [](interface);
}


void BloomFilterSimple::InsertFootprint(const std::string& key, const std::size_t interface)
{
	BloomFilterSimple::InsertFootprint(reinterpret_cast<const unsigned char*>(key.c_str()),key.size(), interface);
}



}  // ndn
}  // ns3



