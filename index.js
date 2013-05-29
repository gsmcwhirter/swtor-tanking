var optimizer = require("bindings")("swtor_tanking_optimizer.node")
  , classData = {
      'shadow': {
        defenseAdd: 0.11 // 6% in tree, 5% naturally higher base %
      , defenseBonus: 0.05 //force breach debuff
      , shieldAdd: 0.15 //combat technique
      ,	shieldBonus: 0.20 //kinetic ward w/ 2-piece
      ,	absorbAdd: 0.04 //in tree
      ,	absorbBonus: 0.0 //KW absorb bonus is factored dynamically
      ,	drAddKE: 0.04 //in tree w/ 4-piece
      ,	drAddIE: 0.13 //in tree w/ 4-piece
      ,	drBonus: 0.05 //slow time
      ,	resistPct: 0.02 
      , useKW: 1
      }
    , 'guardian': {
        defenseAdd: 0.03 //single saber mastery
      , defenseBonus: 0.10 //5% riposte, 5% dust storm
      , shieldAdd: 0.19 //15% soresu form, 4% in tree
      ,	shieldBonus: 0.0 //correct
      ,	absorbAdd: 0.0 //correct
      ,	absorbBonus: 0.0 //correct
      ,	drAddKE: 0.06 //soresu form
      ,	drAddIE: 0.11 //enure talent and soresu form
      ,	drBonus: 0.03 //guardian slash
      ,	resistPct: 0.00 // really?
      , useKW: 0
      }
    , 'vanguard': {
        defenseAdd: 0.06 //4% in tree, 2% from 4-piece
      , defenseBonus: 0.075 //riot gas debuff
      , shieldAdd: 0.17 //ion cell and 2% in tree
      ,	shieldBonus: 0.0 //none
      ,	absorbAdd: 0.04 //in tree
      ,	absorbBonus: 0.15 //ability proc in tree
      ,	drAddKE: 0.02 //in tree
      ,	drAddIE: 0.02 //in tree
      ,	drBonus: 0.05 //static field
      ,	resistPct: 0.02
      , useKW: 0
      }
    }
  , relicData = require("./relics")
  //The following data is from http://www.swtor.com/community/showthread.php?t=616779
  , otherData = {
      dmgMRKE: 0.7793 //defendable KE damage percent
    , dmgFTKE: 0.207 //resistable KE damage percent
    , dmgFTIE: 0.0137 //resistable IE damage percent
    , shieldLow: 0.364663353 //lower bound on shield as a percent of budget, based on arkanian
    , shieldHigh: 0.658429434 //upper bound on shield as a percent of budget, based on arkanian
    , timePerSwing: 0.9 //time per hit incoming
    }
  ;
  
module.exports = {
  optimizer: optimizer
, classData: classData
, otherData: otherData
, relicData: relicData
};
