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



#ifndef NDN_BLOOM_FILTER_SIMPLE_H
#define NDN_BLOOM_FILTER_SIMPLE_H

#include "ns3/ndn-bloom-filter-base.h"

namespace ns3 {
namespace ndn {


class BloomFilterSimple :
         public BloomFilterBase
{
private:
		typedef BloomFilterBase super;
public:

   static TypeId GetTypeId ();

   BloomFilterSimple ();
   virtual ~BloomFilterSimple ();

   /**
   *   Initialization functions for the Stable and Simple Bloom Filters (Basic or Counting).
   *
   *   The initialization can be either "optimal" or "custom".
   *
   *   M 		   =  Estimated Content Catalog Cardinality.
   *   pfp         =  Desired probability of False Positive.
   *   cW		   =  Cell Width.
   *   interfaces  =  Number of interfaces of the node.
   *   m           =  Desired Length of the filter (in case of custom initialization).
   *   k           =  Desired Number of Hash functions to be used (in case of custom initialization).
   *
   */

   // ** [MT] ** Function used to insert an element in the SBF. At every insertion, 'p' random cells are decremented by one.

 //////////
protected:

   virtual void InitBloomFilterOptimal (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces);

   virtual void InitBloomFilterCustom (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces, uint64_t m, uint32_t k);

   virtual void ExtractOptimum(uint64_t M, double pfp);

   virtual void InsertFootprint(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface);

   virtual void InsertFootprint(const std::string& key, const std::size_t interface);

};


}  //ndn
}  //ns3

#endif


