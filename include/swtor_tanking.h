#ifndef SWTOR_TANKING_H
#define SWTOR_TANKING_H

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

#endif
