import sys
from diffkemp.utils import get_simpll_build_dir

# Dynamically import the SimpLL C extension module.
# This way, we can prettily import the module objects into other files.
sys.path.append(get_simpll_build_dir())
import _simpll  # noqa E402

lib = _simpll.lib
ffi = _simpll.ffi
