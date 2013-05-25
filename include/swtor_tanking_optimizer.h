#ifndef SWTOR_TANKING_H
#define SWTOR_TANKING_H

using namespace v8;
using namespace node;

struct OptimizerTask {
	uv_work_t req;
	dmgtypes_t *dtypes;
	shieldbounds_t *sbounds;
	classdata_t *cdata;
	statdist_t *startingStats;
	unsigned int armor;
	unsigned int numRelics;
	relic_t **relics;
	unsigned int *relictypes;
	unsigned int stimBonus;
	unsigned int numSamples;
	double time_per_swing;
	rk_state *rand_state;
	oresult_t *output;
	Persistent<Function> callback;
};

typedef struct OptimizerTask opttask_t;

#endif
