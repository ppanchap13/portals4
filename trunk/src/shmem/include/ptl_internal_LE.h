#ifndef PTL_INTERNAL_LE_H
#define PTL_INTERNAL_LE_H

#include "ptl_internal_PT.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_ints.h"

void INTERNAL PtlInternalLENISetup(const uint_fast8_t ni,
                                   const ptl_size_t   limit);
void INTERNAL      PtlInternalLENITeardown(const uint_fast8_t ni);
ptl_pid_t INTERNAL PtlInternalLEDeliver(ptl_table_entry_t *restrict     t,
                                        ptl_internal_header_t *restrict h);

#endif /* ifndef PTL_INTERNAL_LE_H */
/* vim:set expandtab: */