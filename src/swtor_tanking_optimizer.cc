#include <node.h>
#include <v8.h>
#include <math.h>
#include <stdlib.h>

#include "randomkit.h"
#include "swtor_tanking_optimizer.h"

using namespace node;
using namespace v8;

extern "C" {

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
	procRelicUptime(procrelic_t relic, dmgtypes_t *dtypes, double def_miss_pct, double resist_pct, double shield_pct, double time_per_swing)
	{
		double modifier;
		switch (relic.stat){
			case RELIC_STATTYPE_ABSORB:
				modifier = shield_pct;
				break;
			case RELIC_STATTYPE_DEF:
			case RELIC_STATTYPE_SHIELD:
			default:
				modifier = 1.0;
				break;
		}
		
		double mean_swings = 1.0 / (relic.rate * modifier * ((1.0 - def_miss_pct) * dtypes->KE + (1.0 - resist_pct) * dtypes->IE));
		return relic.duration_time / (mean_swings * time_per_swing + relic.cooldown_time);
	}
	
	double
	clickRelicUptime(clickrelic_t relic)
	{
		return relic.duration_time / relic.cooldown_time;
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
	mitigation(dmgtypes_t *dtypes, classdata_t *cdata, statdist_t *stats, unsigned int armor, unsigned int stimDefense, unsigned int num_relics, relic_t **relics, unsigned int *relictypes)
	{
		double d, s, a, r, drke, drie;
		
		double dbonus = 0.0, sbonus = 0.0, abonus = 0.0;
		unsigned int i;
		
		d = defenseChance(stats->defRating + stimDefense, cdata->defenseAdd + cdata->defenseBonus);
		s = shieldChance(stats->shieldRating, cdata->shieldAdd + cdata->shieldBonus);
		r = cdata->resistPct;
		
		int has_proc_relic = 0;
		int has_click_relic = 0;
		
		for (i = 0; i < num_relics; i++){
			relic_t * relic = *(relics + i);
			switch (*(relictypes + i)){
				case RELIC_TYPE_PROC:
					if (!has_proc_relic || (relic->prelic).can_stack){
						has_proc_relic = 1;
						if ((relic->prelic).stat == RELIC_STATTYPE_DEF){
							dbonus = dbonus + relicBonusPct(stats->defRating + stimDefense, (relic->prelic).bonus_rating, (relic->prelic).stat) * procRelicUptime(relic->prelic, dtypes, d + (0.5 * 0.1), r, s, 1.2);	
						}
						else if ((relic->prelic).stat == RELIC_STATTYPE_SHIELD){
							sbonus = sbonus + relicBonusPct(stats->shieldRating, (relic->prelic).bonus_rating, (relic->prelic).stat) * procRelicUptime(relic->prelic, dtypes, d + (0.5 * 0.1), r, s, 1.2);
						}
						else if ((relic->prelic).stat == RELIC_STATTYPE_ABSORB){
							abonus = abonus + relicBonusPct(stats->absorbRating, (relic->prelic).bonus_rating, (relic->prelic).stat) * procRelicUptime(relic->prelic, dtypes, d + (0.5 * 0.1), r, s, 1.2);
						}
					}
					break;
				case RELIC_TYPE_CLICK:
					if (!has_click_relic){
						has_click_relic = 1;
						
						//stat 1
						if ((relic->crelic).stat1 == RELIC_STATTYPE_DEF){
							dbonus = dbonus + relicBonusPct(stats->defRating + stimDefense, (relic->crelic).bonus_rating1, (relic->crelic).stat1) * clickRelicUptime(relic->crelic);	
						}
						else if ((relic->crelic).stat1 == RELIC_STATTYPE_SHIELD){
							sbonus = sbonus + relicBonusPct(stats->shieldRating, (relic->crelic).bonus_rating1, (relic->crelic).stat1) * clickRelicUptime(relic->crelic);
						}
						else if ((relic->crelic).stat1 == RELIC_STATTYPE_ABSORB){
							abonus = abonus + relicBonusPct(stats->absorbRating, (relic->crelic).bonus_rating1, (relic->crelic).stat1) * clickRelicUptime(relic->crelic);
						}
						
						//stat 2
						if ((relic->crelic).stat2 == RELIC_STATTYPE_DEF){
							dbonus = dbonus + relicBonusPct(stats->defRating + stimDefense, (relic->crelic).bonus_rating2, (relic->crelic).stat2) * clickRelicUptime(relic->crelic);	
						}
						else if ((relic->crelic).stat2 == RELIC_STATTYPE_SHIELD){
							sbonus = sbonus + relicBonusPct(stats->shieldRating, (relic->crelic).bonus_rating2, (relic->crelic).stat2) * clickRelicUptime(relic->crelic);
						}
						else if ((relic->crelic).stat2 == RELIC_STATTYPE_ABSORB){
							abonus = abonus + relicBonusPct(stats->absorbRating, (relic->crelic).bonus_rating2, (relic->crelic).stat2) * clickRelicUptime(relic->crelic);
						}
						
					}
					break;
				default:
					break;
			}
		}
		
		d = defenseChance(stats->defRating + stimDefense, cdata->defenseAdd + cdata->defenseBonus + dbonus);
		s = shieldChance(stats->shieldRating, cdata->shieldAdd + cdata->shieldBonus + sbonus);
		a = absorbChance(stats->absorbRating, cdata->absorbAdd + cdata->absorbBonus + abonus);
		r = cdata->resistPct;
		drke = dmgReductionKE(armor, cdata->drAddKE + cdata->drBonus);
		drie = dmgReductionIE(cdata->drAddIE + cdata->drBonus);
		
		return   dtypes->KE *
					(  
						d + (0.5 * 0.1) + //defended or missed, assuming 50% of all KE attacks at 90% accuracy base
					 	(1.0 - d - (0.5 * 0.1)) * ( //hit
					 		s * ( //shielded
					 			1.0 - (1.0-a) * (1.0-drke) //absorbed and reduced
					 		) +
					 		(1.0-s) * drke //not shielded, but reduced
					 	)
					)
			   + dtypes->IE *
			   		(
			   			r + //resisted
			   			(1.0-r) * (
			   				s * ( //shielded
			   					1.0 - (1.0-a) * (1.0-drie) //absorbed and reduced
			   				) +
			   				(1.0-s) * drie //not shielded, but reduced
			   			)
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
	optimalStats(dmgtypes_t *dtypes, shieldbounds_t * sbounds, classdata_t *cdata, unsigned int statBudget, unsigned int armor, unsigned int stimBonus, unsigned int numSamples, rk_state *rand_state_ptr)
	{
		oresult_t *bestResult = (oresult_t *)malloc(sizeof(oresult_t));
		
		statdist_t *currentStats;
		double currentMitigation;
		
		currentStats = randomStats(sbounds, statBudget, rand_state_ptr);
		currentMitigation = mitigation(dtypes, cdata, currentStats, armor, stimBonus, 0, NULL, NULL);
		
		bestResult->stats = currentStats;
		bestResult->mitigation = currentMitigation;
		
		unsigned int i;
		for (i = 0; i < numSamples; i++){
			currentStats = randomStats(sbounds, statBudget, rand_state_ptr);
			currentMitigation = mitigation(dtypes, cdata, currentStats, armor, stimBonus, 0, NULL, NULL);
			
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
}

Handle<Value> 
Optimizer(const Arguments& args)
{
	HandleScope scope;
		
	//Handle 5-7 arguments: dmgTypes/shieldBounds, classData, statBudget, armor, [stimBonus, [numSamples]], callback
	if (args.Length() < 5 || args.Length() > 7){
		ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    	return scope.Close(Undefined());
	}
	
	//Set up callback
	Local<Function> cb = Local<Function>::Cast(args[args.Length() - 1]);
	const unsigned int cbargc = 2;
	Local<Value> cbargv[cbargc] = { Local<Value>::New(Undefined()), Local<Value>::New(Undefined()) };
	
	//Check parameter types for the main arguments
	if (!args[0]->IsObject() || !args[1]->IsObject() || !args[2]->IsNumber() || !args[3]->IsNumber() || (args.Length() > 5 && !args[4]->IsNumber()) || (args.Length() > 6 && !args[5]->IsNumber())){
		cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong arguments")));
		cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
		return scope.Close(Undefined());
	}
	
	unsigned int i;
	//Check the damage type values (KE and IE) and shield bounds
	const unsigned int dtCt = 4;
	Local<String> dmgTypesProps[dtCt] = {
		String::NewSymbol("dmgKE"), String::NewSymbol("dmgIE"),
		String::NewSymbol("shieldLow"), String::NewSymbol("shieldHigh")
	};
	
	for (i = 0; i < dtCt; i++){
		if (!((args[0]->ToObject())->Get(dmgTypesProps[i]))->IsNumber()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong dmgType values")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
			return scope.Close(Undefined());
		}
	}
	
	//Check the keys and values for classData (defenseAdd, defenseBonus, shieldAdd, shieldBonus, absorbAdd, absorbBonus, drAddKE, drAddIE, drBonus, resistPct)
	const unsigned int propCt = 10;
	Local<String> classDataProps[propCt] = {
		String::NewSymbol("defenseAdd"), String::NewSymbol("defenseBonus"),
		String::NewSymbol("shieldAdd"), String::NewSymbol("shieldBonus"),
		String::NewSymbol("absorbAdd"), String::NewSymbol("absorbBonus"),
		String::NewSymbol("drAddKE"), String::NewSymbol("drAddIE"),
		String::NewSymbol("drBonus"), String::NewSymbol("resistPct")
	};
	
	for (i = 0; i < propCt; i++){
		if (!((args[1]->ToObject())->Get(classDataProps[i]))->IsNumber()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong classData values")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
			return scope.Close(Undefined());
		}
	}
	
	//Seed the random number generator
	rk_state rand_state;
	rk_randomseed(&rand_state);
	
	//Fill in damage type percentages
	dmgtypes_t dtypes;
	dtypes.KE = (((args[0]->ToObject())->Get(dmgTypesProps[0]))->ToNumber())->Value();
	dtypes.IE = (((args[0]->ToObject())->Get(dmgTypesProps[1]))->ToNumber())->Value();
	
	shieldbounds_t sbounds;
	sbounds.low = (((args[0]->ToObject())->Get(dmgTypesProps[2]))->ToNumber())->Value();
	sbounds.high = (((args[0]->ToObject())->Get(dmgTypesProps[3]))->ToNumber())->Value();
	
	//Fill with class data to be passed in: defenseAdd, defenseBonus, shieldAdd, shieldBonus, absorbAdd, absorbBonus, drAddKE, drAddIE, drBonus, resistPct 
	classdata_t cdata;
	cdata.defenseAdd = (((args[1]->ToObject())->Get(classDataProps[0]))->ToNumber())->Value();
	cdata.defenseBonus = (((args[1]->ToObject())->Get(classDataProps[1]))->ToNumber())->Value();
	cdata.shieldAdd = (((args[1]->ToObject())->Get(classDataProps[2]))->ToNumber())->Value();
	cdata.shieldBonus = (((args[1]->ToObject())->Get(classDataProps[3]))->ToNumber())->Value();
	cdata.absorbAdd = (((args[1]->ToObject())->Get(classDataProps[4]))->ToNumber())->Value();
	cdata.absorbBonus = (((args[1]->ToObject())->Get(classDataProps[5]))->ToNumber())->Value();
	cdata.drAddKE = (((args[1]->ToObject())->Get(classDataProps[6]))->ToNumber())->Value();
	cdata.drAddIE = (((args[1]->ToObject())->Get(classDataProps[7]))->ToNumber())->Value();
	cdata.drBonus = (((args[1]->ToObject())->Get(classDataProps[8]))->ToNumber())->Value();
	cdata.resistPct = (((args[1]->ToObject())->Get(classDataProps[9]))->ToNumber())->Value();
	
	unsigned int statBudget = (args[2]->ToNumber())->Value();
	unsigned int armor = (args[3]->ToNumber())->Value();
	
	unsigned int stimBonus = 0;
	if (args.Length() > 5){
		stimBonus = (args[4]->ToNumber())->Value();
	}
	
	unsigned int numSamples = 1000000;
	if (args.Length() > 6){
		numSamples = (args[5]->ToNumber())->Value();
	}
	
	oresult_t *result = optimalStats(&dtypes, &sbounds, &cdata, statBudget, armor, stimBonus, numSamples, &rand_state);
	
	Local<Object> retObj = Object::New();
	retObj->Set(String::NewSymbol("defRating"), Number::New((result->stats)->defRating));
	retObj->Set(String::NewSymbol("shieldRating"), Number::New((result->stats)->shieldRating));
	retObj->Set(String::NewSymbol("absorbRating"), Number::New((result->stats)->absorbRating));
	retObj->Set(String::NewSymbol("defPctNBNS"), Number::New(defenseChance((result->stats)->defRating, cdata.defenseAdd)));
	retObj->Set(String::NewSymbol("defPctNB"), Number::New(defenseChance((result->stats)->defRating + stimBonus, cdata.defenseAdd)));
	retObj->Set(String::NewSymbol("defPct"), Number::New(defenseChance((result->stats)->defRating + stimBonus, cdata.defenseAdd + cdata.defenseBonus)));
	retObj->Set(String::NewSymbol("shieldPctNB"), Number::New(shieldChance((result->stats)->shieldRating, cdata.shieldAdd)));
	retObj->Set(String::NewSymbol("shieldPct"), Number::New(shieldChance((result->stats)->shieldRating, cdata.shieldAdd + cdata.shieldBonus)));
	retObj->Set(String::NewSymbol("absorbPctNB"), Number::New(absorbChance((result->stats)->absorbRating, cdata.absorbAdd)));
	retObj->Set(String::NewSymbol("absorbPct"), Number::New(absorbChance((result->stats)->absorbRating, cdata.absorbAdd + cdata.absorbBonus)));
	retObj->Set(String::NewSymbol("drKENB"), Number::New(dmgReductionKE(armor, cdata.drAddKE)));
	retObj->Set(String::NewSymbol("drKE"), Number::New(dmgReductionKE(armor, cdata.drAddKE + cdata.drBonus)));
	retObj->Set(String::NewSymbol("drIENB"), Number::New(dmgReductionIE(cdata.drAddIE)));
	retObj->Set(String::NewSymbol("drIE"), Number::New(dmgReductionIE(cdata.drAddIE + cdata.drBonus)));
	retObj->Set(String::NewSymbol("mitigation"), Number::New(result->mitigation));
	
	free(result->stats);
	free(result);
	
	cbargv[1] = Local<Value>::New(retObj);
	cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
	return scope.Close(Undefined());
}

void 
init(Handle<Object> target)
{
	NODE_SET_METHOD(target, "optimize", Optimizer);
}

// Here, "swtor_tanking_optimizer" has to be the actual name of the module.
NODE_MODULE(swtor_tanking_optimizer, init);
