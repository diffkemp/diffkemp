"""Module exports:
- lib - loaded SimpLL library (contains callable references to the C functions
  declared in `library/FFI.h`),
- ffi - interface for defining C types/functions and manipulating C data in
  Python.
"""

# Dynamically import the SimpLL C extension module.
_simpll = __import__("_simpll")
lib = _simpll.lib
ffi = _simpll.ffi
