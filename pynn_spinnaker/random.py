# Import modules
import lazyarray as la
from spinnaker import lazy_param_map

# Import classes
from pyNN.random import NativeRNG

# Import functions
from six import iteritems

# ----------------------------------------------------------------------------
# NativeRNG
# ----------------------------------------------------------------------------
# Signals that the random numbers will be supplied by RNG running on SpiNNaker
class NativeRNG(NativeRNG):
    # Maps specifying how each distribution type's
    # parameters will be written to SpiNNaker
    _dist_param_maps = {
        "uniform":      [("low",    "i4", lazy_param_map.s32_fixed_point),
                         ("high",   "i4", lazy_param_map.s32_fixed_point)],
        "uniform_int":  [("low",    "i4", lazy_param_map.s32_fixed_point),
                         ("high",   "i4", lazy_param_map.s32_fixed_point)],
    }

    # Functions to estimate the maximum value a distribution will result in
    # **THINK** should this be moved out of NativeRNG
    # for more general estimation of max delays etc
    _dist_estimate_max_value = {
        "uniform":      lambda parameters: parameters["high"],
        "uniform_int":  lambda parameters: parameters["high"]
    }

    # ------------------------------------------------------------------------
    # AbstractRNG methods
    # ------------------------------------------------------------------------
    def next(self, n=None, distribution=None, parameters=None, mask_local=None):
        raise NotImplementedError("Parameters chosen using SpiNNaker native"
                                  "RNG can only be evaluated on SpiNNaker")

    # ------------------------------------------------------------------------
    # Internal SpiNNaker methods
    # ------------------------------------------------------------------------
    def _estimate_dist_max_value(self, distribution, parameters):
         # Check translation and parameter map exists for this distribution
        if (distribution not in self._dist_estimate_max_value):
            raise NotImplementedError("SpiNNaker native RNG does not support"
                                      "%s distributions" % distribution)
        else:
            return self._dist_estimate_max_value[distribution](parameters)

    def _get_dist_param_map(self, distribution):
        # Check translation and parameter map exists for this distribution
        if (distribution not in self._dist_param_maps):
            raise NotImplementedError("SpiNNaker native RNG does not support"
                                      "%s distributions" % distribution)
        else:
            return self._dist_param_maps[distribution]

    def _get_dist_size(self, distribution):
        # Return size of parameter map
        return lazy_param_map.size(self._get_dist_param_map(distribution), 1)

    def _write_dist(self, fp, distribution, parameters, fixed_point):
        # Wrap parameters in lazy arrays
        parameters = {name: la.larray(value)
                      for name, value in iteritems(parameters)}

        # Evaluate parameters and write to file
        data = lazy_param_map.apply(
            parameters, self._get_dist_param_map(distribution),
            1, fixed_point=fixed_point)
        fp.write(data.tostring())


