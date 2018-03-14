import os
import sys
import tarfile
from distutils.version import StrictVersion
from progressbar import ProgressBar, Percentage, Bar
from urllib import urlretrieve

kernel_base_path = "kernel"

# Progress bar for downloading
pbar = None
def show_progress(count, block_size, total_size):
    global pbar
    if pbar is None:
        pbar = ProgressBar(maxval=total_size, widgets=[Percentage(), Bar()])
        pbar.start()

    downloaded = count * block_size
    if downloaded < total_size:
        pbar.update(downloaded)
    else:
        pbar.finish()
        pbar = None


class KernelModuleCompiler:
    def __init__(self, kernel_version):
        self.kernel_version = kernel_version


    def get_kernel_source(self):
        kernel_path = os.path.join(kernel_base_path,
                                   "linux-%s" % self.kernel_version)
        if not os.path.isfile(kernel_path):
            url = "https://www.kernel.org/pub/linux/kernel/"

            # version directory
            if StrictVersion(self.kernel_version) < StrictVersion("3.0"):
                url += "v%s/" % self.kernel_version[:3]
            else:
                url += "v%s.x/" % self.kernel_version[:1]

            tarname = "linux-%s.tar.gz" % self.kernel_version
            url += tarname

            # Download the tarball with kernel sources
            print "Downloading kernel version %s..." % self.kernel_version
            urlretrieve(url, os.path.join(kernel_base_path, tarname),
                        show_progress)

            # Extract kernel sources
            print "Extracting..."
            os.chdir(kernel_base_path)
            tar = tarfile.open(tarname, "r:gz")
            tar.extractall()
            tar.close

            os.remove(tarname)
            print "Done."
            print("Kernel sources for version %s are in directory %s" %
                  (self.kernel_version, kernel_path))


