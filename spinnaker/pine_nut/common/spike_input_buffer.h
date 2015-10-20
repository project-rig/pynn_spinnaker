#pragma once

//-----------------------------------------------------------------------------
// Common::SpikeInputBufferBase
//-----------------------------------------------------------------------------
namespace Common
{
template<unsigned int Size>
class SpikeInputBufferBase
{
public:
  SpikeInputBufferBase() : m_Input(Size - 1), m_Output(0), m_NumOverflows(0), m_NumUnderflows(0)
  {
  }

  //-----------------------------------------------------------------------------
  // Public API
  //-----------------------------------------------------------------------------
  unsigned int GetUnallocated() const
  {
    return ((m_Input - m_Output) % Size);
  }

  unsigned int GetAllocated() const
  {
    return ((m_Output - m_Input - 1) % Size);
  }

  bool NonEmpty() const
  {
    return (GetAllocated() > 0);
  }

  bool NonFull() const
  {
    return (GetUnallocated() > 0);
  }

  bool AddSpike(uint32_t key)
  {
    bool success = NonFull();
    if (success)
    {
      m_Buffer[m_Input] = key;
      m_Input = (m_Input - 1) % Size;
    }
    else
    {
      m_NumOverflows++;
    }

    return success;
  }

  bool GetNextSpike(uint32_t *e)
  {
    bool success = NonEmpty();
    if (success)
    {
      *e = m_Buffer[m_Output];
      m_Output = (m_Output - 1) % Size;
    }
    else
    {
      m_NumUnderflows++;
    }

    return success;
  }

private:
  //-----------------------------------------------------------------------------
  // Members
  //-----------------------------------------------------------------------------
  uint32_t m_Buffer[Size];
  unsigned int m_Input;
  unsigned int m_Output;

  unsigned int m_NumOverflows;
  unsigned int m_NumUnderflows;

};
} // Common