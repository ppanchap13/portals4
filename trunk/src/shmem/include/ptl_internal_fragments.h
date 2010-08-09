#ifndef PTL_INTERNAL_FRAGMENTS_H
#define PTL_INTERNAL_FRAGMENTS_H

extern size_t SMALL_FRAG_SIZE;
extern size_t SMALL_FRAG_PAYLOAD;
extern size_t SMALL_FRAG_COUNT;
extern size_t LARGE_FRAG_SIZE;
extern size_t LARGE_FRAG_PAYLOAD;
extern size_t LARGE_FRAG_COUNT;

void PtlInternalFragmentSetup(
    volatile char *buf);
void PtlInternalFragmentTeardown(
    void);
void *PtlInternalFragmentFetch(
    size_t payload_size);
void PtlInternalFragmentToss(
    void *frag,
    ptl_pid_t dest);
void PtlInternalFragmentAck(
    void *frag,
    ptl_pid_t dest);
void *PtlInternalFragmentReceive(
    void);
void *PtlInternalFragmentAckReceive(
    void);
void PtlInternalFragmentFree(
    void *data);

uint64_t PtlInternalFragmentSize(
    void *frag);

#endif
