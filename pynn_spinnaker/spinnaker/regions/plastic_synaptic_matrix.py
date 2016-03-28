# Import modules
import logging
import math
import numpy as np

# Import classes
from synaptic_matrix import SynapticMatrix

logger = logging.getLogger("pynn_spinnaker")


# ------------------------------------------------------------------------------
# PlasticSynapticMatrix
# ------------------------------------------------------------------------------
class PlasticSynapticMatrix(SynapticMatrix):
    # How many bits should fixed point weights be converted into
    FixedPointWeightBits = 16

    def __init__(self, synapse_type):
        # Superclass
        super(PlasticSynapticMatrix, self).__init__(synapse_type)

        # Round up number of bytes required by pre-trace to words
        self.pre_trace_words = int(math.ceil(
            float(synapse_type.pre_trace_bytes) / 4.0))

    # --------------------------------------------------------------------------
    # Private methods
    # --------------------------------------------------------------------------
    def _get_num_row_words(self, num_synapses):
        # Both control and plastic words are stored as seperate
        # arrays of 16-bit elements so numbers of words
        # should be rounded up to keep them word aligned
        num_array_words = int(math.ceil(float(num_synapses) / 2.0))

        # Complete row consists of standard header, time of last pre-synaptic
        # spike, pre-synaptic trace and arrays of control words and plastic weights
        return self.NumHeaderWords + 1 + self.pre_trace_words +\
            (2 * num_array_words)

    def _get_num_ext_words(self, num_sub_rows, sub_row_lengths,
                           sub_row_sections):
        # Round up each extension sub-row's length
        # to keep word aligned and take sum
        num_array_words = np.sum(np.ceil(sub_row_lengths[1:] / 2.0), dtype=int)

        # Number of synapses in all but 1st delay
        # slot and header for each extension row to total
        return (2 * num_array_words) +\
            ((self.NumHeaderWords + 1 + self.pre_trace_words) * (num_sub_rows - 1))

    def _write_spinnaker_synapses(self, dtcm_delay, weight_fixed, indices,
                                destination):
        # Zero time of last pre-synaptic spike and pre-synaptic trace
        num_pre_state_words = 1 + self.pre_trace_words
        destination[0: num_pre_state_words] = 0

        # Re-calculate size of control and plastic arrays in words
        num_array_words = int(math.ceil(float(len(indices)) / 2.0))

        # Based on this get index of where
        control_start_idx = num_pre_state_words + num_array_words

        # Create 16-bit view of section of plastic weight
        # section of destination and copy them in
        weight_view = destination[num_pre_state_words: control_start_idx]
        weight_view = weight_view.view(dtype=np.uint16)[:len(weight_fixed)]
        weight_view[:] = weight_fixed

        # Create 16-bit view of control word
        # section of destination and copy them in
        control_view = destination[control_start_idx:]
        control_view = control_view.view(dtype=np.uint16)[:len(indices)]
        control_view[:] = (indices
                           | (dtcm_delay << self.IndexBits)).astype(np.uint16)

