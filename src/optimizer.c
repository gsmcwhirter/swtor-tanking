#include <math.h>
#include <stdlib.h>

#include "randomkit.h"
#include "optimizer.h"
	
double
kineticWardAvgAbsorbBonus(dmgtypes_t *dtypes, double def_miss_pct, double resist_pct, double shield_pct, double time_per_swing)
{
	double shield_rate = shield_pct * ((1.0 - def_miss_pct) * dtypes->MRKE + (1.0 - resist_pct) * dtypes->FTKE);
	double swing_per_shield = 1.0 / shield_rate;
	double time_to_shield = swing_per_shield * time_per_swing;
	
	double t = 0.0;
	double bonus_in_time = 0.0;
	int stacks = 0;
	for (stacks = 0; stacks <= 8 && t < 20.0; stacks++){
		t += time_to_shield;
		bonus_in_time += (double)stacks * 0.01 * time_to_shield;
	}
	
	if (t < 20.0){
		bonus_in_time += 0.08 * (20.0 - t);
	}
	
	return bonus_in_time / 20.0;
}

double
relicBonusPct(const unsigned int base_rating, const unsigned int bonus_rating, unsigned int stat)
{
	double delta;
	
	switch (stat){
		case RELIC_STATTYPE_DEF:
			delta = defenseChance(base_rating + bonus_rating, 0) - defenseChance(base_rating, 0);
			break; 
		case RELIC_STATTYPE_SHIELD:
			delta = shieldChance(base_rating + bonus_rating, 0) - shieldChance(base_rating, 0);
			break;
		case RELIC_STATTYPE_ABSORB:
			delta = absorbChance(base_rating + bonus_rating, 0) - absorbChance(base_rating, 0);
			break;
		default:
			delta = 0.0;
			break;
	}
	
	return delta;
}

double
procRelicUptime(procrelic_t *relic, dmgtypes_t *dtypes, double def_miss_pct, double resist_pct, double shield_pct, double time_per_swing)
{
	double modifier;
	switch (relic->stat){
		case RELIC_STATTYPE_ABSORB:
			modifier = shield_pct;
			break;
		case RELIC_STATTYPE_DEF:
		case RELIC_STATTYPE_SHIELD:
		default:
			modifier = 1.0;
			break;
	}
	
	double mean_swings = 1.0 / (relic->rate * modifier * ((1.0 - def_miss_pct) * dtypes->MRKE + (1.0 - resist_pct) * (dtypes->FTKE + dtypes->FTIE)));
	return relic->duration_time / (mean_swings * time_per_swing + relic->cooldown_time);
}

double
clickRelicUptime(clickrelic_t *relic)
{
	return relic->duration_time / relic->cooldown_time;
}

double
defenseChance(const unsigned int rating, const double bonus)
{
	return bonus + 0.05 + 0.3 * (1.0 - pow(1.0 - 0.01/0.3, (double)rating / (55.0 * 1.2)));
}

double
shieldChance(const unsigned int rating, const double bonus)
{
	return bonus + 0.05 + 0.5 * (1.0 - pow(1.0 - 0.01/0.5, (double)rating / (55.0 * 0.78)));
}

double
absorbChance(const unsigned int rating, const double bonus)
{
	return bonus + 0.2 + 0.5 * (1.0 - pow(1.0 - 0.01/0.5, (double)rating / (55.0 * 0.65)));
}

double
dmgReductionKE(const unsigned int armor, const double bonus)
{
	return bonus + (double)armor / ((double)armor + 240.0 * 55.0 + 800.0);
}

double
dmgReductionIE(const double bonus)
{
	return bonus + 0.10;
}

double
mitigation(dmgtypes_t *dtypes, classdata_t *cdata, statdist_t *stats, unsigned int armor, unsigned int stimDefense, int num_relics, relic_t **relics, unsigned int *relictypes, double time_per_swing)
{
	double d, s, a, r, drke, drie;
	
	double dbonus = 0.0, sbonus = 0.0, abonus = 0.0;
	int i;
	
	d = defenseChance(stats->defRating + stimDefense, cdata->defenseAdd + cdata->defenseBonus);
	s = shieldChance(stats->shieldRating, cdata->shieldAdd + cdata->shieldBonus);
	r = cdata->resistPct;
	
	int has_proc_relic = 0;
	int has_click_relic = 0;
	
	for (i = 0; i < num_relics; i++){
		relic_t * relic = *(relics + i);
		switch (*(relictypes + i)){
			case RELIC_TYPE_PROC:
				if (!has_proc_relic || (relic->prelic)->can_stack){
					if (!(relic->prelic)->can_stack){
						has_proc_relic = 1;	
					}
					
					if ((relic->prelic)->stat == RELIC_STATTYPE_DEF){
						dbonus = dbonus + relicBonusPct(stats->defRating + stimDefense, (relic->prelic)->bonus_rating, (relic->prelic)->stat) * procRelicUptime(relic->prelic, dtypes, d + (0.5 * 0.1), r, s, time_per_swing);	
					}
					else if ((relic->prelic)->stat == RELIC_STATTYPE_SHIELD){
						sbonus = sbonus + relicBonusPct(stats->shieldRating, (relic->prelic)->bonus_rating, (relic->prelic)->stat) * procRelicUptime(relic->prelic, dtypes, d + (0.5 * 0.1), r, s, time_per_swing);
					}
					else if ((relic->prelic)->stat == RELIC_STATTYPE_ABSORB){
						abonus = abonus + relicBonusPct(stats->absorbRating, (relic->prelic)->bonus_rating, (relic->prelic)->stat) * procRelicUptime(relic->prelic, dtypes, d + (0.5 * 0.1), r, s, time_per_swing);
					}
				}
				break;
			case RELIC_TYPE_CLICK:
				if (!has_click_relic){
					has_click_relic = 1;
					
					//stat 1
					if ((relic->crelic)->stat1 == RELIC_STATTYPE_DEF){
						dbonus = dbonus + relicBonusPct(stats->defRating + stimDefense, (relic->crelic)->bonus_rating1, (relic->crelic)->stat1) * clickRelicUptime(relic->crelic);	
					}
					else if ((relic->crelic)->stat1 == RELIC_STATTYPE_SHIELD){
						sbonus = sbonus + relicBonusPct(stats->shieldRating, (relic->crelic)->bonus_rating1, (relic->crelic)->stat1) * clickRelicUptime(relic->crelic);
					}
					else if ((relic->crelic)->stat1 == RELIC_STATTYPE_ABSORB){
						abonus = abonus + relicBonusPct(stats->absorbRating, (relic->crelic)->bonus_rating1, (relic->crelic)->stat1) * clickRelicUptime(relic->crelic);
					}
					
					//stat 2
					if ((relic->crelic)->stat2 == RELIC_STATTYPE_DEF){
						dbonus = dbonus + relicBonusPct(stats->defRating + stimDefense, (relic->crelic)->bonus_rating2, (relic->crelic)->stat2) * clickRelicUptime(relic->crelic);	
					}
					else if ((relic->crelic)->stat2 == RELIC_STATTYPE_SHIELD){
						sbonus = sbonus + relicBonusPct(stats->shieldRating, (relic->crelic)->bonus_rating2, (relic->crelic)->stat2) * clickRelicUptime(relic->crelic);
					}
					else if ((relic->crelic)->stat2 == RELIC_STATTYPE_ABSORB){
						abonus = abonus + relicBonusPct(stats->absorbRating, (relic->crelic)->bonus_rating2, (relic->crelic)->stat2) * clickRelicUptime(relic->crelic);
					}
					
				}
				break;
			default:
				break;
		}
	}
	
	d = defenseChance(stats->defRating + stimDefense, cdata->defenseAdd + cdata->defenseBonus + dbonus);
	//r is above
	s = shieldChance(stats->shieldRating, cdata->shieldAdd + cdata->shieldBonus + sbonus);
	
	double kwbonus = 0.0;
	if (cdata->useKW){
		kwbonus = kineticWardAvgAbsorbBonus(dtypes, d, r, s, time_per_swing);
	}
	
	a = absorbChance(stats->absorbRating, cdata->absorbAdd + cdata->absorbBonus + abonus + kwbonus);
	drke = dmgReductionKE(armor, cdata->drAddKE + cdata->drBonus);
	drie = dmgReductionIE(cdata->drAddIE + cdata->drBonus);
	
	return   dtypes->MRKE *
				(  
					d + (0.5 * 0.1) + //defended or missed, assuming 50% of all KE attacks at 90% accuracy base
				 	(1.0 - d - (0.5 * 0.1)) * ( //hit
				 		s * ( //shielded
				 			1.0 - (1.0-a) * (1.0-drke) //absorbed and reduced
				 		) +
				 		(1.0-s) * drke //not shielded, but reduced
				 	)
				)
		   + dtypes->FTKE *
		   		(
		   			r + //resisted
		   			(1.0-r) * (
		   				s * ( //shielded
		   					1.0 - (1.0-a) * (1.0-drke) //absorbed and reduced
		   				) +
		   				(1.0-s) * drke //not shielded, but reduced
		   			)
		   		)
		   	+ dtypes->FTIE *
		   		(
		   			r + //resisted
		   			(1.0-r) * drie //can't shield IE damage
		   		);
		   		
}

statdist_t *
randomStats(shieldbounds_t * sbounds, const unsigned int budget, rk_state *rand_state_ptr)
{
	statdist_t *stats = (statdist_t *)malloc(sizeof(statdist_t));
	
	double shieldDraw = rk_double(rand_state_ptr);
	stats->shieldRating = (unsigned int)floor((double)budget * (sbounds->low + shieldDraw * (sbounds->high - sbounds->low)));
	double defDraw = rk_double(rand_state_ptr);
	stats->defRating = (unsigned int)floor(defDraw * (double)(budget - stats->shieldRating));
	stats->absorbRating = budget - stats->shieldRating - stats->defRating;
	
	return stats;
}

oresult_t *
optimalStats(dmgtypes_t *dtypes, shieldbounds_t * sbounds, classdata_t *cdata, unsigned int statBudget, unsigned int armor, int num_relics, relic_t **relics, unsigned int *relictypes, unsigned int stimBonus, double time_per_swing, unsigned int numSamples, rk_state *rand_state_ptr)
{
	oresult_t *bestResult = (oresult_t *)malloc(sizeof(oresult_t));
	
	statdist_t *currentStats;
	double currentMitigation;
	
	currentStats = randomStats(sbounds, statBudget, rand_state_ptr);
	currentMitigation = mitigation(dtypes, cdata, currentStats, armor, stimBonus, num_relics, relics, relictypes, time_per_swing);
	
	bestResult->stats = currentStats;
	bestResult->mitigation = currentMitigation;
	
	unsigned int i;
	for (i = 0; i < numSamples; i++){
		currentStats = randomStats(sbounds, statBudget, rand_state_ptr);
		currentMitigation = mitigation(dtypes, cdata, currentStats, armor, stimBonus, num_relics, relics, relictypes, time_per_swing);
		
		if (currentMitigation > bestResult->mitigation){
			free(bestResult->stats);
			bestResult->stats = currentStats;
			bestResult->mitigation = currentMitigation;
		}
		else {
			free(currentStats);
		}
	}
	
	return bestResult;
}
