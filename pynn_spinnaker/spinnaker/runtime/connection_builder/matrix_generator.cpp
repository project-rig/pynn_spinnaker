#include "matrix_generator.h"

// Connection builder includes
#include "connector_generator.h"
#include "param_generator.h"

//-----------------------------------------------------------------------------
// ConnectionBuilder::MatrixGenerator::Static
//-----------------------------------------------------------------------------
void ConnectionBuilder::MatrixGenerator::Base::TraceUInt(uint32_t (&values)[1024],
                                                         unsigned int number) const
{
#if LOG_LEVEL <= LOG_LEVEL_TRACE
  for(unsigned int i = 0; i < number; i++)
  {
    io_printf(IO_BUF, "%u,", values[i]);
  }
#endif
}
//-----------------------------------------------------------------------------
void ConnectionBuilder::MatrixGenerator::Base::TraceInt(int32_t (&values)[1024],
                                                        unsigned int number) const
{
#if LOG_LEVEL <= LOG_LEVEL_TRACE
  for(unsigned int i = 0; i < number; i++)
  {
    io_printf(IO_BUF, "%u,", values[i]);
  }
#endif
}

//-----------------------------------------------------------------------------
// ConnectionBuilder::MatrixGenerator::Static
//-----------------------------------------------------------------------------
void ConnectionBuilder::MatrixGenerator::Static::Generate(uint32_t *matrixAddress,
  unsigned int maxRowSynapses, unsigned int weightFixedPoint, unsigned int numPostNeurons,
  const ConnectorGenerator::Base *connectorGenerator,
  const ParamGenerator::Base *delayGenerator,
  const ParamGenerator::Base *weightGenerator,
  MarsKiss64 &rng) const
{
  // Loop through rows
  for(uint32_t i = 0; i < m_NumRows; i++)
  {
    LOG_PRINT(LOG_LEVEL_TRACE, "\tRow %u", i);

    // Generate row indices
    uint32_t indices[1024];
    LOG_PRINT(LOG_LEVEL_TRACE, "\t\tGenerating indices");
    const unsigned int numIndices = connectorGenerator->Generate(i, maxRowSynapses,
                                                                 numPostNeurons,
                                                                 rng, indices);
    TraceUInt(indices, numIndices);

    // Generate delays and weights for each index
    int32_t delays[1024];
    int32_t weights[1024];
    LOG_PRINT(LOG_LEVEL_TRACE, "\t\tGenerating delays");
    delayGenerator->Generate(numIndices, 0, rng, delays);
    TraceInt(delays, numIndices);

    LOG_PRINT(LOG_LEVEL_TRACE, "\t\tGenerating weights");
    weightGenerator->Generate(numIndices, weightFixedPoint, rng, weights);
    TraceInt(weights, numIndices);

    // Write row length
    *matrixAddress++ = numIndices;

    // **TODO** support delay extension
    *matrixAddress++ = 0;
    *matrixAddress++ = 0;

    // Loop through synapses and write synaptic words
    for(unsigned int j = 0; j < numIndices; j++)
    {
      *matrixAddress++ = (indices[j] & IndexMask) |
        (((uint32_t)delays[j] & DelayMask) << IndexBits) |
        (weights[j] << (DelayBits + IndexBits));
    }

    // Skip end of row padding
    *matrixAddress += (maxRowSynapses - numIndices);
  }
}
