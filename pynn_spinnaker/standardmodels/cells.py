# Import modules
import lazyarray as la
import logging
from pyNN.standardmodels import cells
from ..spinnaker import lazy_param_map
from ..spinnaker import regions

# Import functions
from copy import deepcopy
from functools import partial
from pyNN.standardmodels import build_translations

logger = logging.getLogger("PyNN")

# Function to use with lazy_param_map.choose to determine whether
# a Poisson source should be modelled using the fast or slow model
def _poisson_slow_model(values, sim_timestep_ms, **kwargs):
    # Convert rates into spikes per time step
    rate_vals = deepcopy(values)
    spikes_per_timestep = (rate_vals * sim_timestep_ms) / 1000.0

    # Based on this return mask specifying which spikes sources
    # should be simulated using the slow rather than fast model
    return la.larray(spikes_per_timestep <= 0.25)

# ----------------------------------------------------------------------------
# Neuron type translations
# ----------------------------------------------------------------------------
# Build translations from PyNN to SpiNNaker neuron model parameters
if_curr_neuron_translations = build_translations(
    ("tau_m",       "tau_m"),
    ("cm",          "r_membrane", "tau_m / cm", ""),
    ("v_rest",      "v_rest"),
    ("v_thresh",    "v_thresh"),
    ("v_reset",     "v_reset"),
    ("tau_refrac",  "tau_refrac"),
    ("i_offset",    "i_offset"),
)

if_cond_neuron_translations = build_translations(
    ("tau_m",       "tau_m"),
    ("cm",          "r_membrane", "tau_m / cm", ""),
    ("v_rest",      "v_rest"),
    ("e_rev_E",     "e_rev_e"),
    ("e_rev_I",     "e_rev_i"),
    ("v_thresh",    "v_thresh"),
    ("v_reset",     "v_reset"),
    ("tau_refrac",  "tau_refrac"),
    ("i_offset",    "i_offset"),
)

izhikevich_neuron_translations = build_translations(
    ("a",         "a"),
    ("b",         "b"),
    ("c",         "c"),
    ("d",         "d"),
    ("i_offset",  "i_offset"),
)

# ----------------------------------------------------------------------------
# Synapse type translations
# ----------------------------------------------------------------------------
# Build translations from PyNN to SpiNNaker synapse model parameters
exp_synapse_translations = build_translations(
    ("tau_syn_E",   "tau_syn_e"),
    ("tau_syn_I",   "tau_syn_i"),
)

# ----------------------------------------------------------------------------
# Neuron region maps
# ----------------------------------------------------------------------------
# Build maps of where and how parameters need to be written into neuron regions
if_curr_neuron_immutable_param_map = [
    ("v_thresh",    "i4", lazy_param_map.s1615),
    ("v_reset",     "i4", lazy_param_map.s1615),
    ("v_rest",      "i4", lazy_param_map.s1615),
    ("i_offset",    "i4", lazy_param_map.s1615),
    ("r_membrane",  "i4", lazy_param_map.s1615),
    ("tau_m",       "i4", lazy_param_map.s1615_exp_decay),
    ("tau_refrac",  "u4", lazy_param_map.integer_time_divide),
]

if_curr_neuron_mutable_param_map = [
    ("v", "i4", lazy_param_map.s1615),
    (0,   "i4"),
]

if_cond_neuron_immutable_param_map = [
    ("v_thresh",    "i4", lazy_param_map.s1615),
    ("v_reset",     "i4", lazy_param_map.s1615),
    ("v_rest",      "i4", lazy_param_map.s1615),
    ("e_rev_e",     "i4", lazy_param_map.s1615),
    ("e_rev_i",     "i4", lazy_param_map.s1615),
    ("i_offset",    "i4", lazy_param_map.s1615),
    ("r_membrane",  "i4", lazy_param_map.s1615),
    ("tau_m",       "i4", lazy_param_map.s1615_exp_decay),
    ("tau_refrac",  "u4", lazy_param_map.integer_time_divide),
]

if_cond_neuron_mutable_param_map = [
    ("v", "i4", lazy_param_map.s1615),
    (0,   "i4"),
]

# ----------------------------------------------------------------------------
# Synapse shaping region maps
# ----------------------------------------------------------------------------
exp_synapse_immutable_param_map = [
    ("tau_syn_e", "u4", lazy_param_map.u032_exp_decay),
    ("tau_syn_e", "i4", lazy_param_map.s1615_exp_init),
    ("tau_syn_i", "u4", lazy_param_map.u032_exp_decay),
    ("tau_syn_i", "i4", lazy_param_map.s1615_exp_init),
]

exp_synapse_curr_mutable_param_map = [
    ("isyn_exc", "i4", lazy_param_map.s1615),
    ("isyn_inh", "i4", lazy_param_map.s1615),
]

exp_synapse_cond_mutable_param_map = [
    ("gsyn_exc", "i4", lazy_param_map.s1615),
    ("gsyn_inh", "i4", lazy_param_map.s1615),
]

# ----------------------------------------------------------------------------
# Cell types
# ----------------------------------------------------------------------------
class IF_curr_exp(cells.IF_curr_exp):
    __doc__ = cells.IF_curr_exp.__doc__

    # How many of these neurons per core can
    # a SpiNNaker neuron processor handle
    max_neurons_per_core = 1024

    # JK: not necessary
    neuron_region_class = regions.Neuron
    
    directly_connectable = False

    translations = deepcopy(if_curr_neuron_translations)
    translations.update(exp_synapse_translations)

    neuron_immutable_param_map = if_curr_neuron_immutable_param_map
    neuron_mutable_param_map = if_curr_neuron_mutable_param_map

    synapse_immutable_param_map = exp_synapse_immutable_param_map
    synapse_mutable_param_map = exp_synapse_curr_mutable_param_map


class IF_cond_exp(cells.IF_cond_exp):
    __doc__ = cells.IF_cond_exp.__doc__

    # How many of these neurons per core can
    # a SpiNNaker neuron processor handle
    max_neurons_per_core = 1024

    neuron_region_class = regions.Neuron

    directly_connectable = False

    translations = deepcopy(if_cond_neuron_translations)
    translations.update(exp_synapse_translations)

    neuron_immutable_param_map = if_cond_neuron_immutable_param_map
    neuron_mutable_param_map = if_cond_neuron_mutable_param_map

    synapse_immutable_param_map = exp_synapse_immutable_param_map
    synapse_mutable_param_map = exp_synapse_cond_mutable_param_map

'''
class Izhikevich(cells.Izhikevich):
    __doc__ = cells.Izhikevich.__doc__

    translations = deepcopy(izhikevich_neuron_translations)
    translations.update(current_synapse_translations)
'''


class SpikeSourcePoisson(cells.SpikeSourcePoisson):
    __doc__ = cells.SpikeSourcePoisson.__doc__

    # How many of these neurons per core can
    # a SpiNNaker neuron processor handle
    max_neurons_per_core = 256
    max_current_inputs_per_core = 2048

    directly_connectable = True
    neuron_region_class = regions.SpikeSourcePoisson
    current_input_region_class = regions.SpikeSourcePoisson

    translations = build_translations(
        ("start",    "start_time"),
        ("rate",     "rate"),
        ("duration", "end_time",  "start + duration", "end_time - start_time"),
    )

    immutable_param_map = [
        ("rate", "u4", _poisson_slow_model),
        ("start_time", "u4", lazy_param_map.integer_time_divide),
        ("end_time", "u4", lazy_param_map.integer_time_divide),
        ("rate", "u4", partial(lazy_param_map.choose,
                               mask_function=_poisson_slow_model,
                               a_function=lazy_param_map.s1615_rate_isi,
                               b_function=lazy_param_map.u032_rate_exp_minus_lambda)),
    ]


class SpikeSourceArray(cells.SpikeSourceArray):
    __doc__ = cells.SpikeSourceArray.__doc__

    # How many of these neurons per core can
    # a SpiNNaker neuron processor handle
    max_neurons_per_core = 256
    max_current_inputs_per_core = 512

    directly_connectable = True
    neuron_region_class = regions.SpikeSourceArray
    current_input_region_class = regions.SpikeSourceArray

    translations = build_translations(
        ("spike_times", "spike_times"),
    )
