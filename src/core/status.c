#include "nad/core/status.h"
#include "nad/core/util.h"

const char *nad_status_to_str(nad_Status st) {
    switch (st) {
        case NAD_STATUS_OK:
            return NAD_STRINGIFY(NAD_STATUS_OK);
        case NAD_STATUS_INVALID_ARG:
            return NAD_STRINGIFY(NAD_STATUS_INVALID_ARG);
        case NAD_STATUS_OUT_OF_RANGE:
            return NAD_STRINGIFY(NAD_STATUS_OUT_OF_RANGE);
        case NAD_STATUS_OUT_OF_MEMORY:
            return NAD_STRINGIFY(NAD_STATUS_OUT_OF_MEMORY);
        case NAD_STATUS_UNSUPPORTED:
            return NAD_STRINGIFY(NAD_STATUS_UNSUPPORTED);
    }
    return "UNKNOWN_NAD_STATUS";
}
