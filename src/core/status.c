#include "terse/core/status.h"

#include "terse/core/util.h"

const char *trs_status_to_str(trs_Status st) {
    switch (st) {
        case TRS_STATUS_OK:
            return TRS_STRINGIFY(TRS_STATUS_OK);
        case TRS_STATUS_ERR_NO_MEM:
            return TRS_STRINGIFY(TRS_STATUS_ERR_NO_MEM);
    }
    return "UNKNOWN_TRS_STATUS";
}
