#include <node.h>
#include <v8.h>
#include <math.h>
#include <stdlib.h>
#include <new>

#include <unistd.h>
#include <uv.h>

#include "randomkit.h"
#include "swtor_tanking_optimizer.h"

using namespace node;
using namespace v8;

extern "C" {

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
}

void 
DoCalculations(uv_work_t *r)
{
	opttask_t *task = reinterpret_cast<opttask_t *>(r->data);

	task->output = optimalStats(task->dtypes, task->sbounds, task->cdata, (task->startingStats)->defRating + (task->startingStats)->shieldRating + (task->startingStats)->absorbRating, task->armor, task->numRelics, task->relics, task->relictypes, task->stimBonus, task->time_per_swing, task->numSamples, task->rand_state);
}

void
AfterCalculations(uv_work_t *r)
{
	HandleScope scope;
	
	opttask_t *task = reinterpret_cast<opttask_t *>(r->data);
	oresult_t *result = task->output;
	
	const unsigned int cbargc = 2;
	Handle<Value> cbargv[cbargc] = { Null(), Null() };
	
	TryCatch try_catch;
	
	Local<Object> retObj = Object::New();
	Local<Object> beforeData = Object::New();
	Local<Object> afterData = Object::New();
	
	afterData->Set(String::NewSymbol("defRating"), Number::New((result->stats)->defRating));
	afterData->Set(String::NewSymbol("shieldRating"), Number::New((result->stats)->shieldRating));
	afterData->Set(String::NewSymbol("absorbRating"), Number::New((result->stats)->absorbRating));
	afterData->Set(String::NewSymbol("defPctNBNS"), Number::New(defenseChance((result->stats)->defRating, (task->cdata)->defenseAdd)));
	afterData->Set(String::NewSymbol("defPctNB"), Number::New(defenseChance((result->stats)->defRating + task->stimBonus, (task->cdata)->defenseAdd)));
	afterData->Set(String::NewSymbol("defPct"), Number::New(defenseChance((result->stats)->defRating + task->stimBonus, (task->cdata)->defenseAdd + (task->cdata)->defenseBonus)));
	afterData->Set(String::NewSymbol("shieldPctNB"), Number::New(shieldChance((result->stats)->shieldRating, (task->cdata)->shieldAdd)));
	afterData->Set(String::NewSymbol("shieldPct"), Number::New(shieldChance((result->stats)->shieldRating, (task->cdata)->shieldAdd + (task->cdata)->shieldBonus)));
	afterData->Set(String::NewSymbol("absorbPctNB"), Number::New(absorbChance((result->stats)->absorbRating, (task->cdata)->absorbAdd)));
	afterData->Set(String::NewSymbol("absorbPct"), Number::New(absorbChance((result->stats)->absorbRating, (task->cdata)->absorbAdd + (task->cdata)->absorbBonus)));
	afterData->Set(String::NewSymbol("drKENB"), Number::New(dmgReductionKE(task->armor, (task->cdata)->drAddKE)));
	afterData->Set(String::NewSymbol("drKE"), Number::New(dmgReductionKE(task->armor, (task->cdata)->drAddKE + (task->cdata)->drBonus)));
	afterData->Set(String::NewSymbol("drIENB"), Number::New(dmgReductionIE((task->cdata)->drAddIE)));
	afterData->Set(String::NewSymbol("drIE"), Number::New(dmgReductionIE((task->cdata)->drAddIE + (task->cdata)->drBonus)));
	afterData->Set(String::NewSymbol("mitigation"), Number::New(result->mitigation));
	
	beforeData->Set(String::NewSymbol("defRating"), Number::New((task->startingStats)->defRating));
	beforeData->Set(String::NewSymbol("shieldRating"), Number::New((task->startingStats)->shieldRating));
	beforeData->Set(String::NewSymbol("absorbRating"), Number::New((task->startingStats)->absorbRating));
	beforeData->Set(String::NewSymbol("defPctNBNS"), Number::New(defenseChance((task->startingStats)->defRating, (task->cdata)->defenseAdd)));
	beforeData->Set(String::NewSymbol("defPctNB"), Number::New(defenseChance((task->startingStats)->defRating + task->stimBonus, (task->cdata)->defenseAdd)));
	beforeData->Set(String::NewSymbol("defPct"), Number::New(defenseChance((task->startingStats)->defRating + task->stimBonus, (task->cdata)->defenseAdd + (task->cdata)->defenseBonus)));
	beforeData->Set(String::NewSymbol("shieldPctNB"), Number::New(shieldChance((task->startingStats)->shieldRating, (task->cdata)->shieldAdd)));
	beforeData->Set(String::NewSymbol("shieldPct"), Number::New(shieldChance((task->startingStats)->shieldRating, (task->cdata)->shieldAdd + (task->cdata)->shieldBonus)));
	beforeData->Set(String::NewSymbol("absorbPctNB"), Number::New(absorbChance((task->startingStats)->absorbRating, (task->cdata)->absorbAdd)));
	beforeData->Set(String::NewSymbol("absorbPct"), Number::New(absorbChance((task->startingStats)->absorbRating, (task->cdata)->absorbAdd + (task->cdata)->absorbBonus)));
	beforeData->Set(String::NewSymbol("drKENB"), Number::New(dmgReductionKE(task->armor, (task->cdata)->drAddKE)));
	beforeData->Set(String::NewSymbol("drKE"), Number::New(dmgReductionKE(task->armor, (task->cdata)->drAddKE + (task->cdata)->drBonus)));
	beforeData->Set(String::NewSymbol("drIENB"), Number::New(dmgReductionIE((task->cdata)->drAddIE)));
	beforeData->Set(String::NewSymbol("drIE"), Number::New(dmgReductionIE((task->cdata)->drAddIE + (task->cdata)->drBonus)));
	
	beforeData->Set(String::NewSymbol("mitigation"), Number::New(mitigation(task->dtypes, task->cdata, task->startingStats, task->armor, task->stimBonus, task->numRelics, task->relics, task->relictypes, task->time_per_swing)));
	
	
	retObj->Set(String::NewSymbol("before"), beforeData);
	retObj->Set(String::NewSymbol("after"), afterData);
	
	cbargv[1] = Local<Value>::New(retObj);
	
	task->callback->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
	
	//cleanup
	free(result->stats);
	free(result);
	
	unsigned int i;
	
	task->callback.Dispose();
	free(task->dtypes);
	free(task->sbounds);
	free(task->cdata);
	for (i = 0; i < task->numRelics; i++){
		free(*(task->relics + i));
	}
	free(task->relics);
	free(task->relictypes);
	free(task->rand_state);
	free(task->startingStats);
  	delete task;
		
	if (try_catch.HasCaught()) {
		FatalException(try_catch);
	}
	
}

Handle<Value> 
Optimizer(const Arguments& args)
{
	HandleScope scope;
	
	//Seed the random number generator
	rk_state *rand_state = new rk_state;
	rk_randomseed(rand_state);
		
	//Handle 5-7 arguments: dmgTypes/shieldBounds, classData, relicData, startingStats, armor, [stimBonus, [numSamples]], callback
	if (args.Length() < 5 || args.Length() > 8){
		ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    	return scope.Close(Undefined());
	}
	
	//Set up callback
	Local<Function> cb = Local<Function>::Cast(args[args.Length() - 1]);
	const unsigned int cbargc = 2;
	Handle<Value> cbargv[cbargc] = { Null(), Null() };
	
	//Check parameter types for the main arguments
	if (!args[0]->IsObject() || !args[1]->IsObject() || !args[2]->IsObject() || !args[3]->IsObject() || !args[4]->IsNumber() || (args.Length() > 6 && !args[5]->IsNumber()) || (args.Length() > 7 && !args[6]->IsNumber())){
		cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong arguments")));
		cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
		return scope.Close(Undefined());
	}
	
	unsigned int i;
	//Check the damage type values (KE and IE) and shield bounds
	const unsigned int dtCt = 6;
	const Local<String> dmgTypesProps[dtCt] = {
		String::NewSymbol("dmgMRKE"), String::NewSymbol("dmgFTKE"), String::NewSymbol("dmgFTIE"),
		String::NewSymbol("shieldLow"), String::NewSymbol("shieldHigh"), String::NewSymbol("timePerSwing")
	};
	
	for (i = 0; i < dtCt; i++){
		if (!((args[0]->ToObject())->Get(dmgTypesProps[i]))->IsNumber()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong dmgType values")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
			return scope.Close(Undefined());
		}
	}
	
	
	//Check the keys and values for classData (defenseAdd, defenseBonus, shieldAdd, shieldBonus, absorbAdd, absorbBonus, drAddKE, drAddIE, drBonus, resistPct)
	const unsigned int propCt = 11;
	const Local<String> classDataProps[propCt] = {
		String::NewSymbol("defenseAdd"), String::NewSymbol("defenseBonus"),
		String::NewSymbol("shieldAdd"), String::NewSymbol("shieldBonus"),
		String::NewSymbol("absorbAdd"), String::NewSymbol("absorbBonus"),
		String::NewSymbol("drAddKE"), String::NewSymbol("drAddIE"),
		String::NewSymbol("drBonus"), String::NewSymbol("resistPct"),
		String::NewSymbol("useKW")
	};
	
	for (i = 0; i < propCt; i++){
		if (!((args[1]->ToObject())->Get(classDataProps[i]))->IsNumber()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong classData values")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
			return scope.Close(Undefined());
		}
	}
	
	unsigned int num_relics = 0;
	unsigned int *relictypes = new unsigned int[2];
	*(relictypes + 0) = 0;
	*(relictypes + 1) = 0;
	
	relic_t **relics = new relic_t*[2];
	
	relic_t *relic1 = new relic_t;
	relic_t *relic2 = new relic_t;
	const Local<String> nRelics = String::NewSymbol("numRelics");
	const Local<String> r1name = String::NewSymbol("relic1");
	const Local<String> r2name = String::NewSymbol("relic2");
	const Local<String> type = String::NewSymbol("type");
	Local<Object> tmpobj;
	
	if (!((args[2]->ToObject())->Get(nRelics))->IsNumber()){
		cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong numRelics value")));
		cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
	
		return scope.Close(Undefined());
	}
	else {
		num_relics = ((args[2]->ToObject())->Get(nRelics))->ToNumber()->Value();
		
		if (num_relics < 0 || num_relics > 2){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong numRelics value")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
	
			return scope.Close(Undefined());
		}
	}
	
	if (num_relics >= 1){
		if (!((args[2]->ToObject())->Get(r1name))->IsObject()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong relic1 value")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
	
			return scope.Close(Undefined());
		}
		else {
			tmpobj = ((args[2]->ToObject())->Get(r1name))->ToObject();
			relictypes[0] = (tmpobj->Get(type))->ToNumber()->Value();
			
			if (relictypes[0] == RELIC_TYPE_PROC){
				procrelic_t *pr1 = new procrelic_t;
				pr1->stat = (tmpobj->Get(String::NewSymbol("stat")))->ToNumber()->Value();
				pr1->bonus_rating = (tmpobj->Get(String::NewSymbol("rating")))->ToNumber()->Value();
				pr1->rate = (tmpobj->Get(String::NewSymbol("rate")))->ToNumber()->Value();
				pr1->duration_time = (tmpobj->Get(String::NewSymbol("duration")))->ToNumber()->Value();
				pr1->cooldown_time = (tmpobj->Get(String::NewSymbol("cooldown")))->ToNumber()->Value();
				pr1->can_stack = (tmpobj->Get(String::NewSymbol("can_stack")))->ToNumber()->Value();
				
				relic1->prelic = pr1;
			}
			else if (relictypes[0] == RELIC_TYPE_CLICK){
				clickrelic_t *cr1 = new clickrelic_t;
				cr1->stat1 = (tmpobj->Get(String::NewSymbol("stat1")))->ToNumber()->Value();
				cr1->stat2 = (tmpobj->Get(String::NewSymbol("stat2")))->ToNumber()->Value();
				cr1->bonus_rating1 = (tmpobj->Get(String::NewSymbol("rating1")))->ToNumber()->Value();
				cr1->bonus_rating2 = (tmpobj->Get(String::NewSymbol("rating2")))->ToNumber()->Value();
				cr1->duration_time = (tmpobj->Get(String::NewSymbol("duration")))->ToNumber()->Value();
				cr1->cooldown_time = (tmpobj->Get(String::NewSymbol("cooldown")))->ToNumber()->Value();
				
				relic1->crelic = cr1;
			}
			
			relics[0] = relic1;
		}
	}
	
	if (num_relics >= 2){
		if (!((args[2]->ToObject())->Get(r2name))->IsObject()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong relic2 value")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
	
			return scope.Close(Undefined());
		}
		else {
			tmpobj = ((args[2]->ToObject())->Get(r2name))->ToObject();
			relictypes[1] = (tmpobj->Get(type))->ToNumber()->Value();
			
			if (relictypes[1] == RELIC_TYPE_PROC){
				procrelic_t *pr2 = new procrelic_t;
				pr2->stat = (tmpobj->Get(String::NewSymbol("stat")))->ToNumber()->Value();
				pr2->bonus_rating = (tmpobj->Get(String::NewSymbol("rating")))->ToNumber()->Value();
				pr2->rate = (tmpobj->Get(String::NewSymbol("rate")))->ToNumber()->Value();
				pr2->duration_time = (tmpobj->Get(String::NewSymbol("duration")))->ToNumber()->Value();
				pr2->cooldown_time = (tmpobj->Get(String::NewSymbol("cooldown")))->ToNumber()->Value();
				pr2->can_stack = (tmpobj->Get(String::NewSymbol("can_stack")))->ToNumber()->Value();
				
				relic2->prelic = pr2;
			}
			else if (relictypes[1] == RELIC_TYPE_CLICK){
				clickrelic_t *cr2 = new clickrelic_t;
				cr2->stat1 = (tmpobj->Get(String::NewSymbol("stat1")))->ToNumber()->Value();
				cr2->stat2 = (tmpobj->Get(String::NewSymbol("stat2")))->ToNumber()->Value();
				cr2->bonus_rating1 = (tmpobj->Get(String::NewSymbol("rating1")))->ToNumber()->Value();
				cr2->bonus_rating2 = (tmpobj->Get(String::NewSymbol("rating2")))->ToNumber()->Value();
				cr2->duration_time = (tmpobj->Get(String::NewSymbol("duration")))->ToNumber()->Value();
				cr2->cooldown_time = (tmpobj->Get(String::NewSymbol("cooldown")))->ToNumber()->Value();
				
				relic2->crelic = cr2;
			}
			
			relics[1] = relic2;
		}
	}	
	
	const unsigned int startingStatCt = 3;
	Local<Value> startingStatKey[startingStatCt] = {
		String::NewSymbol("startingDef"),
		String::NewSymbol("startingShield"),
		String::NewSymbol("startingAbsorb")
	};
	
	for (i = 0; i < startingStatCt; i++){
		if (!((args[3]->ToObject())->Get(startingStatKey[i]))->IsNumber()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong startingStats values")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
			return scope.Close(Undefined());
		}
	}
	
	
	statdist_t *startingStats = new statdist_t;
	startingStats->defRating = ((args[3]->ToObject())->Get(startingStatKey[0]))->ToNumber()->Value();
	startingStats->shieldRating = ((args[3]->ToObject())->Get(startingStatKey[1]))->ToNumber()->Value();
	startingStats->absorbRating = ((args[3]->ToObject())->Get(startingStatKey[2]))->ToNumber()->Value();
	
	//Fill in damage type percentages
	dmgtypes_t *dtypes = new dmgtypes_t;
	dtypes->MRKE = (((args[0]->ToObject())->Get(dmgTypesProps[0]))->ToNumber())->Value();
	dtypes->FTKE = (((args[0]->ToObject())->Get(dmgTypesProps[1]))->ToNumber())->Value();
	dtypes->FTIE = (((args[0]->ToObject())->Get(dmgTypesProps[2]))->ToNumber())->Value();
	
	shieldbounds_t *sbounds = new shieldbounds_t;
	sbounds->low = (((args[0]->ToObject())->Get(dmgTypesProps[3]))->ToNumber())->Value();
	sbounds->high = (((args[0]->ToObject())->Get(dmgTypesProps[4]))->ToNumber())->Value();
	
	double time_per_swing = (((args[0]->ToObject())->Get(dmgTypesProps[5]))->ToNumber())->Value();
	
	//Fill with class data to be passed in: defenseAdd, defenseBonus, shieldAdd, shieldBonus, absorbAdd, absorbBonus, drAddKE, drAddIE, drBonus, resistPct 
	classdata_t *cdata = new classdata_t;
	cdata->defenseAdd = (((args[1]->ToObject())->Get(classDataProps[0]))->ToNumber())->Value();
	cdata->defenseBonus = (((args[1]->ToObject())->Get(classDataProps[1]))->ToNumber())->Value();
	cdata->shieldAdd = (((args[1]->ToObject())->Get(classDataProps[2]))->ToNumber())->Value();
	cdata->shieldBonus = (((args[1]->ToObject())->Get(classDataProps[3]))->ToNumber())->Value();
	cdata->absorbAdd = (((args[1]->ToObject())->Get(classDataProps[4]))->ToNumber())->Value();
	cdata->absorbBonus = (((args[1]->ToObject())->Get(classDataProps[5]))->ToNumber())->Value();
	cdata->drAddKE = (((args[1]->ToObject())->Get(classDataProps[6]))->ToNumber())->Value();
	cdata->drAddIE = (((args[1]->ToObject())->Get(classDataProps[7]))->ToNumber())->Value();
	cdata->drBonus = (((args[1]->ToObject())->Get(classDataProps[8]))->ToNumber())->Value();
	cdata->resistPct = (((args[1]->ToObject())->Get(classDataProps[9]))->ToNumber())->Value();
	cdata->useKW = (((args[1]->ToObject())->Get(classDataProps[10]))->ToNumber())->Value();
	
	unsigned int armor = (args[4]->ToNumber())->Value();
	
	unsigned int stimBonus = 0;
	if (args.Length() > 6){
		stimBonus = (args[5]->ToNumber())->Value();
	}
	
	unsigned int numSamples = 1000000;
	if (args.Length() > 7){
		numSamples = (args[6]->ToNumber())->Value();
	}
	
	opttask_t *task = new opttask_t;
	task->req.data = task;
	task->dtypes = dtypes;
	task->sbounds = sbounds;
	task->cdata = cdata;
	task->startingStats = startingStats;
	task->armor = armor;
	task->numRelics = num_relics;
	task->relics = relics;
	task->relictypes = relictypes;
	task->stimBonus = stimBonus;
	task->numSamples = numSamples;
	task->time_per_swing = time_per_swing;
	task->rand_state = rand_state;
	task->output = NULL;
	task->callback = Persistent<Function>::New(cb);
	
	uv_queue_work(uv_default_loop(), &task->req, DoCalculations, (uv_after_work_cb)AfterCalculations);
	
	return Undefined();
}

void 
init(Handle<Object> target)
{
	NODE_SET_METHOD(target, "optimize", Optimizer);
}

// Here, "swtor_tanking_optimizer" has to be the actual name of the module.
NODE_MODULE(swtor_tanking_optimizer, init);
