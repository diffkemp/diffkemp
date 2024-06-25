# Dynamically import the SimpLL C extension module.
# First try to import from the current build directory (to allow multiple
# parallel local builds).
# If that fails, import from the default location.
try:
    _simpll = __import__('_simpll')
except ImportError:
    _simpll = __import__('diffkemp.simpll._simpll')

lib = _simpll.lib
ffi = _simpll.ffi
