#pragma once

// Standard includes
#include <cstdint>

// Common includes
#include "../../common/log.h"

// Synapse processor includes
#include "../plasticity/post_events.h"

//-----------------------------------------------------------------------------
// SynapseProcessor::SynapseTypes::STDP
//-----------------------------------------------------------------------------
namespace SynapseProcessor
{
namespace SynapseTypes
{
template<typename C, unsigned int D, unsigned int I,
         typename TimingDependence, typename WeightDependence, typename SynapseStructure,
         unsigned int T>
class STDP
{
private:
  //-----------------------------------------------------------------------------
  // Typedefines
  //-----------------------------------------------------------------------------
  typedef typename SynapseStructure::PlasticSynapse PlasticSynapse;
  typedef typename TimingDependence::PreTrace PreTrace;
  typedef typename TimingDependence::PostTrace PostTrace;
  typedef Plasticity::PostEventHistory<PostTrace, T> PostEventHistory;

  //-----------------------------------------------------------------------------
  // Constants
  //-----------------------------------------------------------------------------
  static const unsigned int PreTraceWords = (sizeof(PreTrace) / 4) + (((sizeof(PreTrace) % 4) == 0) ? 0 : 1);
  static const uint32_t DelayMask = ((1 << D) - 1);
  static const uint32_t IndexMask = ((1 << I) - 1);

public:
  //-----------------------------------------------------------------------------
  // Constants
  //-----------------------------------------------------------------------------
  // One word for a synapse-count and 512 synapses
  static const unsigned int MaxRowWords = 516 + PreTraceWords;

  //-----------------------------------------------------------------------------
  // Public methods
  //-----------------------------------------------------------------------------
  template<typename F, typename E>
  bool ProcessRow(uint tick, uint32_t (&dmaBuffer)[MaxRowWords], bool flush,
                  F applyInputFunction, E addDelayRowFunction)
  {
    LOG_PRINT(LOG_LEVEL_TRACE, "\tProcessing STDP row with %u synapses",
              dmaBuffer[0]);

    // If this row has a delay extension, call function to add it
    if(dmaBuffer[1] != 0)
    {
      addDelayRowFunction(dmaBuffer[1] + tick, dmaBuffer[2]);
    }

    // Get last pre-synaptic event from event history and write back current time
    // **IMPORTANT TODO** seperate lastPreTick from last update
    // Therefore, flushes update later but not former allowing nearest neighbour to be trivially implemented
    const uint32_t lastPreTick = dmaBuffer[3];
    dmaBuffer[3] = tick;

    // Calculate new pre-trace
    const PreTrace lastPreTrace = *GetPreTrace(dmaBuffer);
    const PreTrace newPreTrace = m_TimingDependence.UpdatePreTrace(tick, lastPreTrace,
                                                                   lastPreTick, flush);

    // Extract first plastic and control words; and loop through synapses
    uint32_t count = dmaBuffer[0];
    PlasticSynapse *plasticWords = GetPlasticWords(dmaBuffer);
    const C *controlWords = GetControlWords(dmaBuffer, count);
    for(; count > 0; count--)
    {
      // Get the next control word from the synaptic_row
      // (should autoincrement pointer in single instruction)
      const uint32_t controlWord = *controlWords++;

      // Extract control word components
      const uint32_t delayDendritic = GetDelay(controlWord);
      const uint32_t delayAxonal = 0;
      const uint32_t postIndex = GetIndex(controlWord);

      // Create update state from next plastic word
      SynapseStructure updateState(*plasticWords);

      // Apply axonal delay to last presynaptic spike tick
      const uint32_t delayedLastPreTick = lastPreTick + delayAxonal;

      // Get the post-synaptic window of events to be processed
      const uint32_t windowBeginTick = (delayedLastPreTick >= delayDendritic) ?
        (delayedLastPreTick - delayDendritic) : 0;
      const uint32_t windowEndTick = tick + delayAxonal - delayDendritic;

      // Get post event history within this window
      auto postWindow = m_PostEventHistory[postIndex].GetWindow(windowBeginTick,
                                                                windowEndTick);

      // Create lambda functions to apply depression
      // and potentiation to the update state
      auto applyDepression =
        [&updateState, this](S2011 depression)
        {
          updateState.ApplyDepression(depression, m_WeightDependence);
        };
      auto applyPotentiation =
        [&updateState, this](S2011 applyPotentiation)
        {
          updateState.ApplyPotentiation(applyPotentiation, m_WeightDependence);
        };

      // Process events in post-synaptic window
      while (postWindow.GetNumEvents() > 0)
      {
        const uint32_t delayedPostTick = postWindow.GetNextTime() + delayDendritic;

        LOG_PRINT(LOG_LEVEL_TRACE, "\t\tApplying post-synaptic event at delayed tick:%u\n",
                  delayedPostTick);

        // Apply post-synaptic spike to state
        m_TimingDependence.ApplyPostSpike(applyDepression, applyPotentiation,
                                          delayedPostTick, postWindow.GetNextTrace(),
                                          delayedLastPreTick, lastPreTrace,
                                          postWindow.GetPrevTime(), postWindow.GetPrevTrace());

        // Go onto next event
        postWindow.Next(delayedPostTick);
      }

      // If this isn't a flush, apply spike to state
      if(!flush)
      {
          const uint32_t delayedPreTick = tick + delayAxonal;
          LOG_PRINT(LOG_LEVEL_TRACE, "\t\tApplying pre-synaptic event at tick:%u, last post tick:%u\n",
                    delayedPreTick, postWindow.GetPrevTime());

          // Apply pre-synaptic spike to state
          m_TimingDependence.ApplyPreSpike(applyDepression, applyPotentiation,
                                           delayedPreTick, newPreTrace,
                                           delayedLastPreTick, lastPreTrace,
                                           postWindow.GetPrevTime(), postWindow.GetPrevTrace());
      }


      // Calculate final state after all updates
      auto finalState = updateState.CalculateFinalState(m_WeightDependence);

      // If this isn't a flush, add weight to ring-buffer
      if(!flush)
      {
        applyInputFunction(delayDendritic + delayAxonal + tick,
          postIndex, finalState.GetWeight());

      }

      // Write back updated synaptic word to plastic region
      *plasticWords++ = finalState.GetPlasticSynapse();
    }

    return true;
  }

  void AddPostSynapticSpike(uint tick, unsigned int neuronID)
  {
    LOG_PRINT(LOG_LEVEL_TRACE, "Adding post-synaptic event to trace at tick:%u",
              tick);

    // Get neuron's post history
    auto &postHistory = m_PostEventHistory[neuronID];

    // Update last trace entry based on spike at tick
    // and add new trace and time to post history
    PostTrace trace = m_TimingDependence.UpdatePostTrace(
      tick, postHistory.GetLastTrace(), postHistory.GetLastTime());
    postHistory.Add(tick, trace);
  }

  unsigned int GetRowWords(unsigned int rowSynapses) const
  {
    // Three header word and a synapse
    return 4 + PreTraceWords + GetNumPlasticWords(rowSynapses) + GetNumControlWords(rowSynapses);
  }

  bool ReadSDRAMData(uint32_t *region, uint32_t flags)
  {
    LOG_PRINT(LOG_LEVEL_INFO, "SynapseTypes::STDP::ReadSDRAMData");

    // Read timing dependence data
    if(!m_TimingDependence.ReadSDRAMData(region, flags))
    {
      return false;
    }

    // Read weight dependence data
    if(!m_WeightDependence.ReadSDRAMData(region, flags))
    {
      return false;
    }

    return true;
  }

private:
  //-----------------------------------------------------------------------------
  // Private static methods
  //-----------------------------------------------------------------------------
  static uint32_t GetIndex(uint32_t word)
  {
    return (word & IndexMask);
  }

  static uint32_t GetDelay(uint32_t word)
  {
    return ((word >> I) & DelayMask);
  }

  static unsigned int GetNumPlasticWords(unsigned int numSynapses)
  {
    const unsigned int plasticBytes = numSynapses * sizeof(PlasticSynapse);
    return (plasticBytes / 4) + (((plasticBytes % 4) == 0) ? 0 : 1);
  }

  static unsigned int GetNumControlWords(unsigned int numSynapses)
  {
    const unsigned int controlBytes = numSynapses * sizeof(C);
    return (controlBytes / 4) + (((controlBytes % 4) == 0) ? 0 : 1);
  }

  static PreTrace *GetPreTrace(uint32_t (&dmaBuffer)[MaxRowWords])
  {
    return reinterpret_cast<PreTrace*>(&dmaBuffer[4]);
  }

  static PlasticSynapse *GetPlasticWords(uint32_t (&dmaBuffer)[MaxRowWords])
  {
    return reinterpret_cast<PlasticSynapse*>(&dmaBuffer[4 + PreTraceWords]);
  }

  static const C *GetControlWords(uint32_t (&dmaBuffer)[MaxRowWords], unsigned int numSynapses)
  {
    return reinterpret_cast<C*>(&dmaBuffer[4 + PreTraceWords + GetNumPlasticWords(numSynapses)]);
  }

  //-----------------------------------------------------------------------------
  // Members
  //-----------------------------------------------------------------------------
  TimingDependence m_TimingDependence;
  WeightDependence m_WeightDependence;

  PostEventHistory m_PostEventHistory[512];
};
} // SynapseTypes
} // SynapseProcessor