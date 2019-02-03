//============================================================
//
//	ticker.c - MSDOS timing code
//
//============================================================

#include "osdepend.h"
#include "driver.h"
#include "mamalleg.h"
#include <time.h>



//============================================================
//	PROTOTYPES
//============================================================

static cycles_t rdtsc_cycle_counter(void);
static cycles_t uclock_cycle_counter(void);
static cycles_t nop_cycle_counter(void);



//============================================================
//	GLOBAL VARIABLES
//============================================================

// global cycle_counter function and divider
cycles_t		(*cycle_counter)(void) = nop_cycle_counter;
cycles_t		(*ticks_counter)(void) = nop_cycle_counter;
cycles_t		cycles_per_sec;



//============================================================
//	has_rdtsc
//============================================================

static int has_rdtsc(void)
{
	int result;

	__asm__ (
		"movl $1,%%eax     ; "
		"xorl %%ebx,%%ebx  ; "
		"xorl %%ecx,%%ecx  ; "
		"xorl %%edx,%%edx  ; "
		"cpuid             ; "
		"testl $0x10,%%edx ; "
		"setne %%al        ; "
		"andl $1,%%eax     ; "
	:  "=&a" (result)   /* the result has to go in eax */
	:       /* no inputs */
	:  "%ebx", "%ecx", "%edx" /* clobbers ebx ecx edx */
	);
	return result;
}


//============================================================
//	init_ticker
//============================================================

void init_ticker(void)
{
	uclock_t a,b;
	cycles_t start,end;

	if (cpu_cpuid && has_rdtsc())
	{
		// if the RDTSC instruction is available use it because
		// it is more precise and has less overhead than uclock()
		cycle_counter = rdtsc_cycle_counter;
		ticks_counter = rdtsc_cycle_counter;
		logerror("using RDTSC for timing ... ");

		a = uclock();
		/* wait some time to let everything stabilize */
		do
		{
			b = uclock();
			// get the starting cycle count
			start = (*cycle_counter)();
		} while (b - a < UCLOCKS_PER_SEC/2);

		// now wait for 1/2 second
		do
		{
			a = uclock();
			// get the ending cycle count
			end = (*cycle_counter)();
		} while (a - b < UCLOCKS_PER_SEC/2);

		// compute ticks_per_sec
		cycles_per_sec = (end - start) * 2;
	}
	else
	{
		cycle_counter = uclock_cycle_counter;
		ticks_counter = nop_cycle_counter;
		logerror("using uclock for timing ... ");

		cycles_per_sec = UCLOCKS_PER_SEC;
	}
	// log the results
	logerror("cycles/second = %u\n", (int)cycles_per_sec);
}



//============================================================
//	rdtsc_cycle_counter
//============================================================

cycles_t rdtsc_cycle_counter( void )
{
	INT64 result;

	// use RDTSC
	__asm__ __volatile__ (
		"rdtsc"
		: "=A" (result)
	);

	return result;
}



//============================================================
//	uclock_cycle_counter
//============================================================

static cycles_t uclock_cycle_counter( void )
{
	/* this assumes that uclock_t is 64-bit (which it is) */
	return uclock();
}



//============================================================
//	nop_cycle_counter
//============================================================

static cycles_t nop_cycle_counter(void)
{
	return 0;
}



//============================================================
//	osd_cycles
//============================================================

cycles_t osd_cycles(void)
{
	return (*cycle_counter)();
}



//============================================================
//	osd_cycles_per_second
//============================================================

cycles_t osd_cycles_per_second(void)
{
	return cycles_per_sec;
}



//============================================================
//	osd_profiling_ticks
//============================================================

cycles_t osd_profiling_ticks(void)
{
	return (*ticks_counter)();
}
