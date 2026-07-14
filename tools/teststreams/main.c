#include "xsAll.h"
#include "mc.xs.h"

void flushPromises(xsMachine* the)
{
	while (mxPendingJobs.value.reference->next) {
		fxRunPromiseJobs(the);
	}
}