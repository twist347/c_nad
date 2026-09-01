#include "nad/core/status.h"

#include "nad/core/util.h"

const char *nad_status_to_str(nad_Status st) {
    switch (st) {
        case NAD_STATUS_OK:
            return NAD_STRINGIFY(NAD_STATUS_OK);
        case NAD_STATUS_ERR_NO_MEM:
            return NAD_STRINGIFY(NAD_STATUS_ERR_NO_MEM);
    }
    return "UNKNOWN_NAD_STATUS";
}
