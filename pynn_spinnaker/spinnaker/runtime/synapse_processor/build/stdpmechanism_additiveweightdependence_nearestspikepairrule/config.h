#pragma once

// Rig CPP common includes
#include "rig_cpp_common/circular_buffer.h"
#include "../../../common/row_offset_length.h"
namespace SynapseProcessor
{
  typedef Common::CircularBuffer<uint32_t, 256> SpikeInputBuffer;
  typedef Common::CircularBuffer<RowOffsetLength<10>, 256> DelayRowBuffer;
}

// Synapse processor includes
#include "../../../common/key_lookup_binary_search.h"
namespace SynapseProcessor
{
  typedef Common::KeyLookupBinarySearch<10> KeyLookup;
}

// Additive weight dependence with 16-bit unsigned weights
#include "../../plasticity/weight_dependences/additive.h"
namespace SynapseProcessor
{
  typedef Plasticity::WeightDependences::Additive<uint16_t> WeightDependence;
}

// Plastic synapses contain just a a weight
#include "../../plasticity/synapse_structures/weight.h"
namespace SynapseProcessor
{
  typedef Plasticity::SynapseStructures::Weight<WeightDependence> SynapseStructure;
}

// Pair-based STDP rule with 256 entry lookup tables for potentiation and depression function
#include "../../plasticity/timing_dependences/nearest_pair.h"
namespace SynapseProcessor
{
  typedef Plasticity::TimingDependences::NearestPair<256, 0, 256, 0> TimingDependence;
}

// STDP synapses using 16-bit control words with 3 delay bits and 10 index bits;
// previously configured timing dependence, weight dependence and synapse structure;
// and a post-synaptic event history with 10 entries
#include "../../synapse_types/stdp_nearest.h"
namespace SynapseProcessor
{
  typedef SynapseTypes::STDPNearest<uint16_t, 3, 10,
                                    TimingDependence, WeightDependence, SynapseStructure, 10> SynapseType;
}

// Ring buffer with 32-bit unsigned entries, large enough for 512 neurons
#include "../../ring_buffer.h"
namespace SynapseProcessor
{
  typedef RingBufferBase<uint32_t, 3, 9> RingBuffer;
}

#include "../../delay_buffer.h"
namespace SynapseProcessor
{
  typedef DelayBufferBase<10> DelayBuffer;
}
