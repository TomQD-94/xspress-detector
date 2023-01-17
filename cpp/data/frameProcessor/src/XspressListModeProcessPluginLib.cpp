//
// Created by gnx91527 on 2022/01/05.
//

#include "XspressListModeProcessPlugin.h"
#include "ClassLoader.h"

namespace FrameProcessor
{
    /**
     * Registration of this process plugin through the ClassLoader.  This macro
     * registers the class without needing to worry about name mangling
     */
    REGISTER(FrameProcessorPlugin, XspressListModeProcessPlugin, "XspressListModeProcessPlugin");

} // namespace FrameReceiver

