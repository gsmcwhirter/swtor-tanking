var assert = require('assert')
  , sto = require('./index')
  ;


sto.optimizer.optimize(sto.otherData, sto.classData.shadow, 2100, 6033, 70, function (err, results){
  if (err){
    console.log("Error!");
  }
  else {
    console.log(results);
  }
});

console.log();

sto.optimize('vanguard', 2100, 9033, function (err, results){
  if (err){
    console.log("Error!");
  }
  else {
    console.log(results);
  }
});

console.log();

sto.optimize('guardian', 2100, 9033, function (err, results){
  if (err){
    console.log("Error!");
  }
  else {
    console.log(results);
  }
});

console.log();

try {
  sto.optimizer.optimize(); 
} catch (err){
  console.log(err);
}

sto.optimizer.optimize(sto.otherData, sto.classData.shadow, 2100, null, function (err, results){
  console.log(err);
  console.log(results);
});
