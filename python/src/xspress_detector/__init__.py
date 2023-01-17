from xspress_detector._version import get_versions
from xspress_detector.control.adapter import XspressAdapter
from xspress_detector.data.xspress_meta_writer import XspressMetaWriter

__version__ = get_versions()["version"]
del get_versions

__all__ = ["XspressAdapter", "XspressMetaWriter", "__version__"]
