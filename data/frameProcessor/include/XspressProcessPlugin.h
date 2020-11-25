
#ifndef SRC_XSPRESSPLUGIN_H
#define SRC_XSPRESSPLUGIN_H

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>

using namespace log4cxx;
using namespace log4cxx::helpers;

#include "FrameProcessorPlugin.h"
#include "XspressDefinitions.h"

namespace FrameProcessor {

    class XspressProcessPlugin : public FrameProcessorPlugin {
    public:
        XspressProcessPlugin();

        virtual ~XspressProcessPlugin();

        void configure(OdinData::IpcMessage &config, OdinData::IpcMessage &reply);

        // version related functions
        int get_version_major();

        int get_version_minor();

        int get_version_patch();

        std::string get_version_short();

        std::string get_version_long();

    private:
        // Plugin interface
        void process_frame(boost::shared_ptr <Frame> frame);

        void deadtimeCorrectScalers(uint32_t numChans, uint32_t *iSCA, double *dSCA, double *factors, double *corrected,
                                    double clockPeriod);

        void calculateDeadtimeCorrection(uint32_t numChans, double clockPeriod, double deadtimeEnergy, uint32_t *sca,
                                         double *factors, double *corrected);

        bool dtcEnabled;
        int *dtcFlags;
        double *dtcParams;

        uint32_t DataEnd;
        size_t mcaDataSize;
        size_t scalerDataSize;

        static const std::string CONFIG_DTC_FLAGS;
        static const std::string CONFIG_DTC_PARAMS;

        /** Pointer to logger */
        LoggerPtr logger_;
    };

}


#endif //SRC_XSPRESSPLUGIN_H
