#ifndef SWTOR_TANKING_H
#define SWTOR_TANKING_H

extern "C" {
struct ClassData {
	double defenseAdd;
	double defenseBonus;
	double shieldAdd;
	double shieldBonus;
	double absorbAdd;
	double absorbBonus;
	double drAddKE;
	double drAddIE;
	double drBonus;
	double resistPct; 
};

typedef struct ClassData classdata_t;

struct StatDistribution {
	unsigned int defRating;
	unsigned int shieldRating;
	unsigned int absorbRating;
};

typedef struct StatDistribution statdist_t;

struct DamageTypes {
	double KE;
	double IE;
};

typedef struct DamageTypes dmgtypes_t;

struct OptimizerResult {
	statdist_t *stats;
	double mitigation;
};

typedef struct OptimizerResult oresult_t;

struct ShieldBounds {
	double low;
	double high;
};

typedef struct ShieldBounds shieldbounds_t;

double defenseChance(const unsigned int rating, const double bonus);
double shieldChance(const unsigned int rating, const double bonus);
double absorbChance(const unsigned int rating, const double bonus);
double dmgReductionKE(const unsigned int armor, const double bonus);
double dmgReductionIE(const double bonus);
double mitigation(dmgtypes_t *dtypes, classdata_t *cdata, statdist_t *stats, unsigned int armor, unsigned int stimDefense);
statdist_t * randomStats(shieldbounds_t * sbounds, const unsigned int budget, rk_state *rand_state_ptr);
oresult_t * optimalStats(dmgtypes_t *dtypes, shieldbounds_t * sbounds, classdata_t *cdata, unsigned int statBudget, unsigned int armor, unsigned int stimBonus, unsigned int numSamples, rk_state *rand_state_ptr);

}

#endif
