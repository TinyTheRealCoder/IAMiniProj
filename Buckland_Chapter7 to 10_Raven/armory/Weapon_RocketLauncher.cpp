#include "Weapon_RocketLauncher.h"
#include "../Raven_Bot.h"
#include "misc/Cgdi.h"
#include "../Raven_Game.h"
#include "../Raven_Map.h"
#include "../lua/Raven_Scriptor.h"
#include "fuzzy/FuzzyOperators.h"


//--------------------------- ctor --------------------------------------------
//-----------------------------------------------------------------------------
RocketLauncher::RocketLauncher(Raven_Bot*   owner):

                      Raven_Weapon(type_rocket_launcher,
                                   script->GetInt("RocketLauncher_DefaultRounds"),
                                   script->GetInt("RocketLauncher_MaxRoundsCarried"),
                                   script->GetDouble("RocketLauncher_FiringFreq"),
                                   script->GetDouble("RocketLauncher_IdealRange"),
                                   script->GetDouble("Rocket_MaxSpeed"),
                                   owner)
{
    //setup the vertex buffer
  const int NumWeaponVerts = 8;
  const Vector2D weapon[NumWeaponVerts] = {Vector2D(0, -3),
                                           Vector2D(6, -3),
                                           Vector2D(6, -1),
                                           Vector2D(15, -1),
                                           Vector2D(15, 1),
                                           Vector2D(6, 1),
                                           Vector2D(6, 3),
                                           Vector2D(0, 3)
                                           };
  for (int vtx=0; vtx<NumWeaponVerts; ++vtx)
  {
    m_vecWeaponVB.push_back(weapon[vtx]);
  }

  //setup the fuzzy module
  InitializeFuzzyModule();

}


//------------------------------ ShootAt --------------------------------------
//-----------------------------------------------------------------------------
inline void RocketLauncher::ShootAt(Vector2D pos)
{ 
  if (NumRoundsRemaining() > 0 && isReadyForNextShot())
  {
    //fire off a rocket!
    m_pOwner->GetWorld()->AddRocket(m_pOwner, pos);

    m_iNumRoundsLeft--;

    UpdateTimeWeaponIsNextAvailable();

    //add a trigger to the game so that the other bots can hear this shot
    //(provided they are within range)
    m_pOwner->GetWorld()->GetMap()->AddSoundTrigger(m_pOwner, script->GetDouble("RocketLauncher_SoundRange"));
  }
}

//---------------------------- Desirability -----------------------------------
//
//-----------------------------------------------------------------------------
double RocketLauncher::GetDesirability(double DistToTarget, int remainingHealth)
{
  if (m_iNumRoundsLeft == 0)
  {
    m_dLastDesirabilityScore = 0;
  }
  else
  {
    //fuzzify distance and amount of ammo
    m_FuzzyModule.Fuzzify("DistToTarget", DistToTarget);
    m_FuzzyModule.Fuzzify("AmmoStatus", (double)m_iNumRoundsLeft);
    m_FuzzyModule.Fuzzify("HealthStatus", (double)remainingHealth);
    m_FuzzyModule.Fuzzify("ProjectileSpeed", (double)m_dMaxProjectileSpeed);
    m_FuzzyModule.Fuzzify("RateOfFire", (double)m_dRateOfFire);

    m_dLastDesirabilityScore = m_FuzzyModule.DeFuzzify("Desirability", FuzzyModule::max_av);
  }

  return m_dLastDesirabilityScore;
}

//-------------------------  InitializeFuzzyModule ----------------------------
//
//  set up some fuzzy variables and rules
//-----------------------------------------------------------------------------
void RocketLauncher::InitializeFuzzyModule()
{
  FuzzyVariable& DistToTarget = m_FuzzyModule.CreateFLV("DistToTarget");

  FzSet& Target_Close = DistToTarget.AddLeftShoulderSet("Target_Close",0,25,150);
  FzSet& Target_Medium = DistToTarget.AddTriangularSet("Target_Medium",25,150,300);
  FzSet& Target_Far = DistToTarget.AddRightShoulderSet("Target_Far",150,300,1000);

  FuzzyVariable& Desirability = m_FuzzyModule.CreateFLV("Desirability"); 
  FzSet& VeryDesirable = Desirability.AddRightShoulderSet("VeryDesirable", 50, 75, 100);
  FzSet& Desirable = Desirability.AddTriangularSet("Desirable", 25, 50, 75);
  FzSet& Undesirable = Desirability.AddLeftShoulderSet("Undesirable", 0, 25, 50);

  FuzzyVariable& AmmoStatus = m_FuzzyModule.CreateFLV("AmmoStatus");
  FzSet& Ammo_Loads = AmmoStatus.AddRightShoulderSet("Ammo_Loads", 10, 30, 100);
  FzSet& Ammo_Okay = AmmoStatus.AddTriangularSet("Ammo_Okay", 0, 10, 30);
  FzSet& Ammo_Low = AmmoStatus.AddTriangularSet("Ammo_Low", 0, 0, 10);

  //RemainingHealth
  FuzzyVariable& HealthStatus = m_FuzzyModule.CreateFLV("HealthStatus");
  FzSet& Health_Hight = HealthStatus.AddRightShoulderSet("RemainingHealth_Hight", 50, 75, 100);
  FzSet& Health_Mid = HealthStatus.AddTriangularSet("RemainingHealth_Medium", 25, 50, 75);
  FzSet& Health_Low = HealthStatus.AddTriangularSet("RemainingHealth_Low", 0, 25, 50);

  //WeaponProjectileSpeed
  FuzzyVariable& ProjectileSpeed = m_FuzzyModule.CreateFLV("ProjectileSpeed");
  FzSet& ProjectileSpeed_Slow = ProjectileSpeed.AddRightShoulderSet("ProjectileSpeed_Slow", 0, 200, 300);
  FzSet& ProjectileSpeed_Mid = ProjectileSpeed.AddTriangularSet("ProjectileSpeed_Medium", 200, 300, 350);
  FzSet& ProjectileSpeed_Fast = ProjectileSpeed.AddTriangularSet("ProjectileSpeed_Fast", 300, 350, 400);

  //RateOfFire
  FuzzyVariable& RateOfFire = m_FuzzyModule.CreateFLV("RateOfFire");
  FzSet& RateOfFire_Slow = RateOfFire.AddRightShoulderSet("RateOfFire_Slow", 200, 300, 400);
  FzSet& RateOfFire_Mid = RateOfFire.AddTriangularSet("RateOfFire_Medium", 100, 200, 300);
  FzSet& RateOfFire_Fast = RateOfFire.AddTriangularSet("RateOfFire_Fast", 0, 100, 200);

  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Loads), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Okay), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Low), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Loads), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Okay), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Low), Desirable);

  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Loads), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Okay), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Low), Undesirable);

  m_FuzzyModule.AddRule(FzAND(RateOfFire_Slow, Ammo_Okay), Desirable);
  m_FuzzyModule.AddRule(FzAND(RateOfFire_Slow, Ammo_Low), VeryDesirable);

  m_FuzzyModule.AddRule(FzAND(RateOfFire_Mid, Ammo_Okay), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(RateOfFire_Mid, Ammo_Low), Desirable);

  m_FuzzyModule.AddRule(FzAND(RateOfFire_Fast, Ammo_Okay), Desirable);
  m_FuzzyModule.AddRule(FzAND(RateOfFire_Fast, Ammo_Low), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Medium, ProjectileSpeed_Slow), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, ProjectileSpeed_Mid), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, ProjectileSpeed_Fast), VeryDesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Far, ProjectileSpeed_Slow), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, ProjectileSpeed_Mid), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, ProjectileSpeed_Fast), VeryDesirable);

  m_FuzzyModule.AddRule(FzAND(Health_Hight, Ammo_Okay), Desirable);
  m_FuzzyModule.AddRule(FzAND(Health_Hight, Ammo_Low), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Health_Low, Ammo_Okay), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Health_Low, Ammo_Low), Desirable);
}


//-------------------------------- Render -------------------------------------
//-----------------------------------------------------------------------------
void RocketLauncher::Render()
{
    m_vecWeaponVBTrans = WorldTransform(m_vecWeaponVB,
                                   m_pOwner->Pos(),
                                   m_pOwner->Facing(),
                                   m_pOwner->Facing().Perp(),
                                   m_pOwner->Scale());

  gdi->RedPen();

  gdi->ClosedShape(m_vecWeaponVBTrans);
}