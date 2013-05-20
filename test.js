var assert = require('assert');
var binding = require('./build/Release/swtor_tanking');

//shadow data
var classData = {
  defenseAdd: 0.06
, defenseBonus: 0.05
, shieldAdd: 0.15
,	shieldBonus: 0.20
,	absorbAdd: 0.04
,	absorbBonus: 0.036
,	drAddKE: 0.02
,	drAddIE: 0.13
,	drBonus: 0.05
,	resistPct: 0.02
};


binding.optimize(classData, 2100, 6033, function (err, results){
  if (err){
    console.log("Error!");
  }
  else {
    console.log(results);
  }
});

try {
  binding.optimize(); 
} catch (err){
  console.log(err);
}

binding.optimize(classData, 2100, null, function (err, results){
  console.log(err);
  console.log(results);
});
