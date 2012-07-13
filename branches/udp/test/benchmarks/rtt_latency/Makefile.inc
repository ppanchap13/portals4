# vim:ft=automake
RTT_TESTS =  \
	CT_LE_rtt_latency \
	CT_ME_rtt_latency \
	EQ_LE_rtt_latency \
	EQ_ME_rtt_latency

BENCHMARKS += $(RTT_TESTS)
check_PROGRAMS += $(RTT_TESTS)
TESTS += $(RTT_TESTS)

CT_LE_rtt_latency_SOURCES = benchmarks/rtt_latency/ct_hotpotato.c
CT_LE_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

CT_ME_rtt_latency_SOURCES = benchmarks/rtt_latency/ct_hotpotato.c
CT_ME_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1

EQ_LE_rtt_latency_SOURCES = benchmarks/rtt_latency/events_hotpotato.c
EQ_LE_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=0

EQ_ME_rtt_latency_SOURCES = benchmarks/rtt_latency/events_hotpotato.c
EQ_ME_rtt_latency_CPPFLAGS = $(AM_CPPFLAGS) -DINTERFACE=1