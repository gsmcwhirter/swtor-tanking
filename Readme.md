# swtor-tanking

This is a library providing fast tank stat optimizations with a node.js interface.

## API

This is installable via npm: ```npm install swtor-tanking```

Then you can use it via the following:

```js
var sto = require("swtor-tanking");

var combatAndShieldData = {
  dmgMRKE: 0.5 //pct damage melee and ranged, kinetic and energy type
, dmgFTKE: 0.4 //pct damage force and tech, kinetic and energy type
, dmgFTIE: 0.1 //pct damage force and tech, internal and elemental type
, shieldLow: 0.0 //lower bound on shield as a percent of total budget
, shieldHigh: 1.0 //upper bound on shield as a percent of total budget
, timePerSwing: 1.0 //time between each incoming swing
};

var classData = {
  defenseAdd: 0.0 //additions to defense from set, tree, etc.
, defenseBonus: 0.0 //bonuses to defense from skills, accuracy debuffs, etc.
, shieldAdd: 0.0 //additions to shield from set, tree, etc.
, shieldBonus: 0.0 //bonuses to shield from skills, accuracy debuffs, etc.
, absorbAdd: 0.0 //additions to absorb from set, tree, etc.
, absorbBonus: 0.0 //bonuses to absorb from skills, accuracy debuffs, etc.
, drAddKE: 0.0 //additions to Kinetic and Energy damage reduction
, drAddIE: 0.0 //additions to Internal and Elemental damage reduction
, drBonus: 0.0 //bonus damage reduction from skills, etc.
, resistPct: 0.0 //chance to resist Force and Tech attacks
, useKW: 0 //0 - no Kinetic Ward in calculations, 1 - calculate using kinetic ward
};

var relicData = {
  numRelics: 2 //number of relics
, relic1: {
    type: 1 //1 = proc, 2 = click
  , stat: 1 //1 = defense, 2 = shield, 3 = absorb
  , rating: 510 //rating added
  , rate: 0.3 //chance of proccing (shield chance is built in elsewhere, so absorb proc on shield is stat 3, rate 1.0)
  , duration: 6 //duration of the proc in seconds
  , cooldown: 20 //internal cooldown in seconds
  , can_stack: 0 //very few relics can stack, but this is 1 if it can
  } 
, relic2: {
    type: 2 //click
  , stat1: 2
  , rating1: 265
  , stat2: 3 //this can be 0 if it is a defense click or the like, and this stat will be ignored
  , rating2: 265
  , duration: 20
  , cooldown: 120
  //click relics ignore can_stack, since none can
  }
};

var startingStats = {
  startingDef: 0 //rating
, startingShield: 0 //rating
, startingAbsorb: 0 //rating
};

var armor = 9000;
var stim = 70;

sto.optimizer.optimize(combatAndShieldData, classData, relicData, startingStats, armor, stim, function (err, result){
  if (err){
    //handle error
  }
  else {
    /*
    result.before holds the initial data
    result.after holds the optimized data
    
    Object.keys(result.before) === Object.keys(result.after) === [
      defRating //defense rating
    , shieldRating //shield rating
    , absorbRating //absorb rating
    , defPctNBNS //defense chance % before bonus, before stim
    , defPctNB //defense chance % before bonus, with stim
    , defPct //effective defense chance %
    , shieldPctNB //shield chance % before bonus
    , shieldPct //effective shield chance %
    , absorbPctNB //absorb chance % before bonus
    , absorbPct //effective absorb chance %
    , drKENB //damage reduction Kinetic and Energy, before bonus
    , drKE //effective damage reduction Kinetic and Energy
    , drIENB //damage reduction Internal and Elemental, before bonus
    , drIE //effective damage reduction Internal and Elemental
    , mitigation //percent of damage mitigated, on average
    ]
    */
  }
});
```

After the armor parameter and before the callback you can also optionally pass the number of samples to take (not commonly used, default is 10,000).

## License

MIT (see LICENSE file)
