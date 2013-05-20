#include <node.h>
#include <v8.h>
#include <math.h>
#include <stdlib.h>
//TODO: remove after testing
#include <stdio.h>

#include "randomkit.h"
#include "swtor_tanking.h"

using namespace node;
using namespace v8;

extern "C" {
	//TODO: Add actual optimization stuff here
	
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
	mitigation(dmgtypes_t *dtypes, classdata_t *cdata, statdist_t *stats, unsigned int armor, unsigned int stimDefense)
	{
		double d, s, a, r, drke, drie;
		
		d = defenseChance(stats->defRating + stimDefense, cdata->defenseAdd + cdata->defenseBonus);
		s = shieldChance(stats->shieldRating, cdata->shieldAdd + cdata->shieldBonus);
		a = absorbChance(stats->absorbRating, cdata->absorbAdd + cdata->absorbBonus);
		r = cdata->resistPct;
		drke = dmgReductionKE(armor, cdata->drAddKE + cdata->drBonus);
		drie = dmgReductionIE(cdata->drAddIE + cdata->drBonus);
		
		printf("%f\n", d);
		printf("%f\n", s);
		printf("%f\n", a);
		printf("%f\n", r);
		printf("%f\n", drke);
		printf("%f\n", drie);
		
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
			   		
		/*	
		return dmgTypes['MRKE'] * (\
             d(r=r_defense) +\
             0.5 * 0.1 +\
             (1.0 - d(r=r_defense) - 0.5 * 0.1) * (\
                 s(r=r_shield) *\
                     (1 - (1 - a(r=r_absorb)) * (1 - drmr(r=r_armor))) +\
                 (1-s(r=r_shield)) * \
                     drmr(r=r_armor))) +\
             dmgTypes['FTIE'] * (\
             res +\
             (1.0 - res) * (\
                 s(r=r_shield) *\
                     (1 - (1 - a(r=r_absorb)) * (1 - drft)) +\
                 (1-s(r=r_shield)) *\
                     drft)))
    	*/
	}
	
	statdist_t *
	randomStats(const unsigned int budget, rk_state *rand_state_ptr)
	{
		statdist_t *stats = (statdist_t *)malloc(sizeof(statdist_t));
		const double shieldBoundLow = 0.364663353;
		const double shieldBoundHigh = 0.658429434;
		
		double shieldDraw = rk_double(rand_state_ptr);
		stats->shieldRating = (unsigned int)floor((double)budget * (shieldBoundLow + shieldDraw * (shieldBoundHigh - shieldBoundLow)));
		double defDraw = rk_double(rand_state_ptr);
		stats->defRating = (unsigned int)floor(defDraw * (double)(budget - stats->shieldRating));
		stats->absorbRating = budget - stats->shieldRating - stats->defRating;
		
		return stats;
	}
	
	/* need these for annealing, but since the annealing I was doing is really just random sampling, I leave them out
	
	double
	temperature(const double pct_time)
	{
		return 1.0 / pct_time;
	}
	
	double 
	acceptProb(const double currentTarget, const double newTarget, const double temp)
	{
		if (newTarget > currentTarget){
			return 1.0;
		}
		else {
			return exp((newTarget - currentTarget) / temp);
		}
	}
	
	*/
	
	oresult_t *
	optimalStats(dmgtypes_t *dtypes, classdata_t *cdata, unsigned int statBudget, unsigned int armor, unsigned int stimBonus, unsigned int numSamples, rk_state *rand_state_ptr)
	{
		oresult_t *bestResult = (oresult_t *)malloc(sizeof(oresult_t));
		
		statdist_t *currentStats;
		double currentMitigation;
		
		currentStats = randomStats(statBudget, rand_state_ptr);
		currentMitigation = mitigation(dtypes, cdata, currentStats, armor, stimBonus);
		
		bestResult->stats = currentStats;
		bestResult->mitigation = currentMitigation;
		
		unsigned int i;
		for (i = 0; i < numSamples; i++){
			currentStats = randomStats(statBudget, rand_state_ptr);
			currentMitigation = mitigation(dtypes, cdata, currentStats, armor, stimBonus);
			
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
		
	//Handle 4-6 arguments: classData, statBudget, armor, [stimBonus, [numSamples]], callback
	if (args.Length() < 4 || args.Length() > 6){
		ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    	return scope.Close(Undefined());
	}
	
	//Set up callback
	Local<Function> cb = Local<Function>::Cast(args[args.Length() - 1]);
	const unsigned int cbargc = 2;
	Local<Value> cbargv[cbargc] = { Local<Value>::New(Undefined()), Local<Value>::New(Undefined()) };
	
	//Check parameter types for the main arguments
	if (!args[0]->IsObject() || !args[1]->IsNumber() || !args[2]->IsNumber() || (args.Length() > 4 && !args[3]->IsNumber()) || (args.Length() > 5 && !args[4]->IsNumber())){
		cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong arguments")));
		cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
		return scope.Close(Undefined());
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
	
	unsigned int i;
	for (i = 0; i < propCt; i++){
		if (!((args[0]->ToObject())->Get(classDataProps[i]))->IsNumber()){
			cbargv[0] = Local<Value>::New(Exception::TypeError(String::New("Wrong classData values")));
			cb->Call(Context::GetCurrent()->Global(), cbargc, cbargv);
		
			return scope.Close(Undefined());
		}
	}
	
	//Seed the random number generator
	rk_state rand_state;
	rk_randomseed(&rand_state);
	
	//Fill with class data to be passed in: defenseAdd, defenseBonus, shieldAdd, shieldBonus, absorbAdd, absorbBonus, drAddKE, drAddIE, drBonus, resistPct 
	classdata_t cdata;
	cdata.defenseAdd = (((args[0]->ToObject())->Get(classDataProps[0]))->ToNumber())->Value();
	cdata.defenseBonus = (((args[0]->ToObject())->Get(classDataProps[1]))->ToNumber())->Value();
	cdata.shieldAdd = (((args[0]->ToObject())->Get(classDataProps[2]))->ToNumber())->Value();
	cdata.shieldBonus = (((args[0]->ToObject())->Get(classDataProps[3]))->ToNumber())->Value();
	cdata.absorbAdd = (((args[0]->ToObject())->Get(classDataProps[4]))->ToNumber())->Value();
	cdata.absorbBonus = (((args[0]->ToObject())->Get(classDataProps[5]))->ToNumber())->Value();
	cdata.drAddKE = (((args[0]->ToObject())->Get(classDataProps[6]))->ToNumber())->Value();
	cdata.drAddIE = (((args[0]->ToObject())->Get(classDataProps[7]))->ToNumber())->Value();
	cdata.drBonus = (((args[0]->ToObject())->Get(classDataProps[8]))->ToNumber())->Value();
	cdata.resistPct = (((args[0]->ToObject())->Get(classDataProps[9]))->ToNumber())->Value();
	
	dmgtypes_t dtypes;
	dtypes.KE = 0.790044607;
	dtypes.IE = 0.209955393;
	
	unsigned int statBudget = (args[1]->ToNumber())->Value();
	unsigned int armor = (args[2]->ToNumber())->Value();
	unsigned int numSamples = 1000000;
	if (args.Length() > 5){
		numSamples = (args[4]->ToNumber())->Value();
	}
	
	unsigned int stimBonus = 70;
	if (args.Length() > 4){
		stimBonus = (args[3]->ToNumber())->Value();
	}
	
	oresult_t *result = optimalStats(&dtypes, &cdata, statBudget, armor, stimBonus, numSamples, &rand_state);
	
	/*
	printf("defenseRating: %i\n", (result->stats)->defRating);
	printf("defense%% (no stim, no bonus): %.2f\n", defenseChance((result->stats)->defRating, cdata.defenseAdd) * 100);
	printf("defense%% (stim, no bonus): %.2f\n", defenseChance((result->stats)->defRating + stimBonus, cdata.defenseAdd) * 100);
	printf("defense%% (stim, bonus): %.2f\n", defenseChance((result->stats)->defRating + stimBonus, cdata.defenseAdd + cdata.defenseBonus) * 100);
	printf("shieldRating: %i\n", (result->stats)->shieldRating);
	printf("shield%% (no bonus): %.2f\n", shieldChance((result->stats)->shieldRating, cdata.shieldAdd) * 100);
	printf("shield%% (bonus): %.2f\n", shieldChance((result->stats)->shieldRating, cdata.shieldAdd + cdata.shieldBonus) * 100);
	printf("absorbRating: %i\n", (result->stats)->absorbRating);
	printf("absorb%% (no bonus): %.2f\n", absorbChance((result->stats)->absorbRating, cdata.absorbAdd) * 100);
	printf("absorb%% (bonus): %.2f\n", absorbChance((result->stats)->absorbRating, cdata.absorbAdd + cdata.absorbBonus) * 100);
	printf("dmgReduction%% KE (no bonus): %.2f\n", dmgReductionKE(armor, cdata.drAddKE) * 100);
	printf("dmgReduction%% KE (bonus): %.2f\n", dmgReductionKE(armor, cdata.drAddKE + cdata.drBonus) * 100);
	printf("dmgReduction%% IE (no bonus): %.2f\n", dmgReductionIE(cdata.drAddIE) * 100);
	printf("dmgReduction%% IE (bonus): %.2f\n", dmgReductionIE(cdata.drAddIE + cdata.drBonus) * 100);
	printf("mitigation score: %f\n", result->mitigation);
	*/
	
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

Handle<Value> 
Test(const Arguments& args)
{
	HandleScope scope;
	
	//Seed the random number generator
	rk_state rand_state;
	rk_randomseed(&rand_state);
	
	//Fill with class data to be passed in: defenseAdd, defenseBonus, shieldAdd, shieldBonus, absorbAdd, absorbBonus, drAddKE, drAddIE, drBonus, resistPct 
	classdata_t cdata;
	cdata.defenseAdd = 0.06;
	cdata.defenseBonus = 0.05;
	cdata.shieldAdd = 0.15;
	cdata.shieldBonus = 0.20;
	cdata.absorbAdd = 0.04;
	cdata.absorbBonus = 0.036;
	cdata.drAddKE = 0.02;
	cdata.drAddIE = 0.13;
	cdata.drBonus = 0.05;
	cdata.resistPct = 0.02;
	
	dmgtypes_t dtypes;
	dtypes.KE = 0.790044607;
	dtypes.IE = 0.209955393;
	
	unsigned int statBudget = 589 + 807 + 775;
	unsigned int armor = 6033;
	
	statdist_t stats;
	stats.defRating = 589;
	stats.shieldRating = 807;
	stats.absorbRating = 775;
	
	printf("\nOutside:\n");
	printf("%f\n", defenseChance(stats.defRating, cdata.defenseAdd + cdata.defenseBonus));
	printf("%f\n", shieldChance(stats.shieldRating, cdata.shieldAdd + cdata.shieldBonus));
	printf("%f\n", absorbChance(stats.absorbRating, cdata.absorbAdd + cdata.absorbBonus));
	printf("%f\n", cdata.resistPct);
	printf("%f\n", dmgReductionKE(armor, cdata.drAddKE + cdata.drBonus));
	printf("%f\n", dmgReductionIE(cdata.drAddIE + cdata.drBonus));
	printf("%f\n", dtypes.KE);
	printf("%f\n", dtypes.IE);
	printf("\nInside:\n");
	printf("%f\n", mitigation(&dtypes, &cdata, &stats, armor, 0));
	
	return scope.Close(Undefined());
}

void 
init(Handle<Object> target)
{
	NODE_SET_METHOD(target, "optimize", Optimizer);
	NODE_SET_METHOD(target, "test", Test);
}

// Here, "swtor_tanking" has to be the actual name of the module.
NODE_MODULE(swtor_tanking, init);
