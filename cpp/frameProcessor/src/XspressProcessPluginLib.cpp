//
// Created by myr45768 on 2019/10/16.
//

#include "XspressProcessPlugin.h"
#include "ClassLoader.h"

namespace FrameProcessor
{
    /**
     * Registration of this process plugin through the ClassLoader.  This macro
     * registers the class without needing to worry about name mangling
     */
    REGISTER(FrameProcessorPlugin, XspressProcessPlugin, "XspressProcessPlugin");

} // namespace FrameReceiver



