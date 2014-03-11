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



#ifndef NDN_BLOOM_FILTER_BASE_H
#define NDN_BLOOM_FILTER_BASE_H

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <bitset>
#include <stdlib.h>
#include <stdint.h>
#include <ctime>
#include <boost/dynamic_bitset.hpp>

#include "ns3/simple-ref-count.h"
#include "ns3/node.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/core-module.h"
#include "ns3/log.h"

namespace ns3 {
namespace ndn {


static const uint32_t bits_per_char = 8;    // 8 bits in 1 char(unsigned)

class BloomFilterBase :
	public Object
{
public:

   static TypeId GetTypeId ();

   static std::string GetLogName ();

   /** [MT] ** It represents the FIB Entry that is returned after having looked up the SBFs of the interfaces of the node. It contains a vector
    *          with the interfaces ordered according to the number of name components which provided a match. The respective metrics
    *          are in the vector 'metrics'.
    */

   struct bestFibEntry
   {
     Ptr<const ns3::ndn::Name> prefix;
     std::vector< Ptr<Face> >  orderedFaces;
     std::vector< uint32_t >   metrics;
     bool found;
   };

   struct bfParameters
   {
	   bfParameters (uint64_t _bfRawLengthBit, uint64_t _bfLengthBit, uint64_t _bfRawLengthByte,
			   uint64_t _bfLengthByte, uint32_t _bfCellWidth, uint64_t _bfEstimCatalogCardinality,
			   double   _bfDesiredFalsePosProb, uint32_t _bfNumHashFunctions):
				   bfRawLengthBit(_bfRawLengthBit), bfLengthBit(_bfLengthBit), bfRawLengthByte(_bfRawLengthByte),
				   bfLengthByte(_bfLengthByte), bfCellWidth(_bfCellWidth), bfEstimCatalogCardinality(_bfEstimCatalogCardinality),
				   bfDesiredFalsePosProb(_bfDesiredFalsePosProb), bfNumHashFunctions(_bfNumHashFunctions) {}

	   uint64_t bfRawLengthBit;                    // Length of the filter [bit] without considering the cell width;
	   uint64_t bfLengthBit;                       // Length of the filter [bit]; it includes the cell width.
	   uint64_t bfRawLengthByte;			 	   // Length of the filter [byte] without considering the cell width;
	   uint64_t bfLengthByte;					   // Length of the filter [byte]; it includes the cell width.
	   uint32_t bfCellWidth;					   // Cell Width of the filter.
	   uint64_t bfEstimCatalogCardinality;		   // Estimated Content Catalog Cardinality.
	   double   bfDesiredFalsePosProb;			   // Desired False Positive Probability.
	   uint32_t bfNumHashFunctions;                // Number of Hash Functions used to obtain a footprint.
   };

   bfParameters* bfParam;

   uint32_t     randomSeed;
   uint32_t 	numberOfInterfaces;			 // Number of interfaces of the node.

   std::string scope;
   std::string initMethod;
   uint64_t insertedContentCatalog;
   double insertedPfp;
   uint32_t insertedCellWidth;
   uint64_t insertedCustomFilterLength;
   uint32_t insertedCustomHashes;


   BloomFilterBase();

   virtual ~BloomFilterBase();

   void MakeBfInitialization();

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

   std::size_t GetBloomFilterSize(uint32_t interface) {return bloomFilter->operator [](interface).size(); }


   // ** [MT] ** Function used to insert an element in the SBF. At every insertion, 'p' random cells are decremented by one.

   template<typename T>
   void InsertFootprint(const T& t, const std::size_t interface);

   template<typename InputIterator>
   void InsertFootprint(const InputIterator begin, const InputIterator end, const std::size_t interface);


   void ResetFootprintCells(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface);

   template<typename T>
   void ResetFootprintCells(const T& t, const std::size_t interface);

   void ResetFootprintCells(const std::string& key, const std::size_t interface);

   template<typename InputIterator>
   void ResetFootprintCells(const InputIterator begin, const InputIterator end, const std::size_t interface);

   ////

   void DecrementFootprintCells(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface);

   template<typename T>
   void DecrementFootprintCells(const T& t, const std::size_t interface);

   void DecrementFootprintCells(const std::string& key, const std::size_t interface);

   template<typename InputIterator>
   void DecrementFootprintCells(const InputIterator begin, const InputIterator end, const std::size_t interface);

   ////

   bool IncrementFilterCell (std::size_t cell, std::size_t interface);

   bool DecrementFilterCell (std::size_t cell, std::size_t interface);

   bool SetMaxFilterCell (std::size_t cell, std::size_t interface);

   bool SetZeroFilterCell (std::size_t cell, std::size_t interface);

   ////

   bool LookupFilter(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface);

   template<typename T>
   bool LookupFilter(const T& t, const std::size_t interface);

   bool LookupFilter(const std::string& key, const std::size_t interface);

   template<typename InputIterator>
   bool LookupFilter(const InputIterator begin, const InputIterator end, const std::size_t interface);

   ////

   uint32_t CountCell(std::size_t cell, std::size_t interface) const;

   ////

   bestFibEntry& LookupOrderedBloomFilters (Ptr<const Interest> header, const Ptr<Face> &incomingFace) const;

   // ********************************************************************
   std::vector<std::vector<uint32_t> > * MakeCustomLookupBloomFilters(const unsigned char* key_begin, const std::size_t length, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const;

   template<typename T>
   std::vector<std::vector<uint32_t> > * MakeCustomLookupBloomFilters(const T& t, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const;
   std::vector<std::vector<uint32_t> > * MakeCustomLookupBloomFilters(const std::string& key, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const;
   std::vector<std::vector<uint32_t> > * MakeCustomLookupBloomFilters(const char* data, const std::size_t& length, Ptr<const Interest> header, uint32_t idIncInterface, uint32_t nodeInterfaces, bool evict) const;


   void SetFilter(uint32_t interface);

   void ClearFilter(uint32_t interface);

   bool GetBloomFilterFlag();

   void SetBloomFilterFlag(bool value);

   virtual void InsertFootprint(const unsigned char* key_begin, const std::size_t& length, const std::size_t interface) = 0;

   virtual void InsertFootprint(const std::string& key, const std::size_t interface) = 0;

   void
   SetForwardingStrategy(const Ptr<ForwardingStrategy> fwS);

   Ptr<ForwardingStrategy>
   GetForwardingStrategy() const;

   void
   SetL3Protocol(const Ptr<L3Protocol> l3P);

   Ptr<L3Protocol>
   GetL3Protocol() const;


protected:

//   virtual void NotifyNewAggregate ();

   virtual void InitBloomFilterOptimal (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces) = 0;

   virtual void InitBloomFilterCustom (uint64_t M, double pfp, uint32_t cW, uint32_t interfaces, uint64_t m, uint32_t k) = 0;

   virtual void ExtractOptimum(uint64_t M, double pfp) = 0;


   void ComputeIndicesFilter(const uint32_t& hash, std::size_t& bit_index, std::size_t& bit) const;

   void GenerateSeedsHash();

   uint32_t hash_ap(const unsigned char* begin, std::size_t remaining_length, uint32_t hash) const;
   uint32_t MurmurHash3 (const unsigned char * key, std::size_t len, uint32_t seed) const;

   std::vector<boost::dynamic_bitset<> >*  bloomFilter;       			   // There are as many SBFs as the number of interfaces.

   std::vector<uint32_t>* 				bfSeeds;

   std::vector<uint32_t>*	    bfInsertedElements;

   std::vector<uint32_t>*	    bfDeletedElements;

   bool bloomFilterFlag;


private:
   Ptr<ForwardingStrategy> m_forwardingStrategy;
   Ptr<L3Protocol> m_l3prot;
};


}  //ndn
}  //ns3

#endif


