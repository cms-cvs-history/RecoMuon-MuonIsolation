#include "RecoMuon/MuonIsolation/interface/CaloExtractorByAssociator.h"

#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "Utilities/Timing/interface/TimingReport.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "DataFormats/CaloTowers/interface/CaloTowerCollection.h"
#include "DataFormats/EcalDetId/interface/EcalSubdetector.h"
#include "DataFormats/HcalDetId/interface/HcalSubdetector.h"

#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "Geometry/Records/interface/IdealGeometryRecord.h"
#include "TrackingTools/Records/interface/TrackingComponentsRecord.h"

#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "TrackingTools/TrackAssociator/interface/TrackDetectorAssociator.h"

using namespace edm;
using namespace std;
using namespace reco;
using namespace muonisolation;

CaloExtractorByAssociator::CaloExtractorByAssociator(const ParameterSet& par) :
  theCaloTowerCollectionLabel(par.getParameter<edm::InputTag>("CaloTowerCollectionLabel")),
  theUseRecHitsFlag(par.getParameter<bool>("UseRecHitsFlag")),
  theDepositLabel(par.getUntrackedParameter<string>("DepositLabel")),
  theDepositInstanceLabels(par.getParameter<std::vector<std::string> >("DepositInstanceLabels")),
  thePropagatorName(par.getParameter<std::string>("PropagatorName")),
  theThreshold_E(par.getParameter<double>("Threshold_E")),
  theThreshold_H(par.getParameter<double>("Threshold_H")),
  theThreshold_HO(par.getParameter<double>("Threshold_HO")),
  theDR_Veto_E(par.getParameter<double>("DR_Veto_E")),
  theDR_Veto_H(par.getParameter<double>("DR_Veto_H")),
  theDR_Veto_HO(par.getParameter<double>("DR_Veto_HO")),
  theDR_Max(par.getParameter<double>("DR_Max")),
  theNoise_EB(par.getParameter<double>("Noise_EB")),
  theNoise_EE(par.getParameter<double>("Noise_EE")),
  theNoise_HB(par.getParameter<double>("Noise_HB")),
  theNoise_HE(par.getParameter<double>("Noise_HE")),
  theNoise_HO(par.getParameter<double>("Noise_HO")),	
  theNoiseTow_EB(par.getParameter<double>("NoiseTow_EB")),
  theNoiseTow_EE(par.getParameter<double>("NoiseTow_EE")),
  theAssociator(0),
  thePropagator(0),
  thePrintTimeReport(par.getUntrackedParameter<bool>("PrintTimeReport"))
{
  theAssociator = new TrackDetectorAssociator();
  theAssociator->theCaloTowerCollectionLabel = theCaloTowerCollectionLabel;
  theAssociator->theEBRecHitCollectionLabel = par.getParameter<edm::InputTag>("EBRecHitCollectionLabel");
  theAssociator->theEERecHitCollectionLabel = par.getParameter<edm::InputTag>("EERecHitCollectionLabel");
  theAssociator->theHBHERecHitCollectionLabel = par.getParameter<edm::InputTag>("HBHERecHitCollectionLabel");
  theAssociator->theHORecHitCollectionLabel = par.getParameter<edm::InputTag>("HORecHitCollectionLabel");  
}

CaloExtractorByAssociator::~CaloExtractorByAssociator(){
  if (thePrintTimeReport) TimingReport::current()->dump(std::cout);
  if (theAssociator) delete theAssociator;
  if (thePropagator) delete thePropagator;
}

void CaloExtractorByAssociator::fillVetos(const edm::Event& event, const edm::EventSetup& eventSetup, const TrackCollection& muons)
{
//   LogWarning("CaloExtractorByAssociator")
//     <<"fillVetos does nothing now: MuIsoDeposit provides enough functionality\n"
//     <<"to remove a deposit at/around given (eta, phi)";

}

MuIsoDeposit CaloExtractorByAssociator::deposit( const Event & event, const EventSetup& eventSetup, const Track & muon) const
{
  MuIsoDeposit::Direction muonDir(muon.eta(), muon.phi());
  MuIsoDeposit dep(theDepositLabel, muonDir );

//   LogWarning("CaloExtractorByAssociator")
//     <<"single deposit is not an option here\n"
//     <<"use ::deposits --> extract all and reweight as necessary";

  return dep;

}


//Make separate deposits: for ECAL, HCAL, HO
std::vector<MuIsoDeposit> CaloExtractorByAssociator::deposits( const Event & event, const EventSetup& eventSetup, const Track & muon) const
{
  if (thePropagator == 0){
    ESHandle<Propagator> prop;
    eventSetup.get<TrackingComponentsRecord>().get(thePropagatorName, prop);
    thePropagator = prop->clone();
    theAssociator->setPropagator(thePropagator);
  }
  if (theDepositInstanceLabels.size() != 3){
    LogError("MuonIsolation")<<"Configuration is inconsistent: Need 3 deposit instance labels";
  }
  if (! theDepositInstanceLabels[0].compare(0,1, std::string("e")) == 0
      || ! theDepositInstanceLabels[1].compare(0,1, std::string("h")) == 0
      || ! theDepositInstanceLabels[2].compare(0,2, std::string("ho")) == 0){
    LogWarning("MuonIsolation")<<"Deposit instance labels do not look like  (e*, h*, ho*):"
			       <<"proceed at your own risk. The extractor interprets lab0=from ecal; lab1=from hcal; lab2=from ho";
  }

  typedef MuIsoDeposit::Veto Veto;
  MuIsoDeposit::Direction muonDir(muon.eta(), muon.phi());
  
  MuIsoDeposit depEcal(theDepositInstanceLabels[0], muonDir);
  MuIsoDeposit depHcal(theDepositInstanceLabels[1], muonDir);
  MuIsoDeposit depHOcal(theDepositInstanceLabels[2], muonDir);

  TrackDetectorAssociator::AssociatorParameters asParams;
  asParams.useMuon = false;
  asParams.dREcal = theDR_Max;
  asParams.dRHcal = theDR_Max;
  asParams.dREcalPreselection = asParams.dREcal;
  asParams.dRHcalPreselection = asParams.dRHcal;
  if (theUseRecHitsFlag){
    asParams.useCalo = false;
    asParams.useEcal = true;
    asParams.useHcal = true;
    asParams.useHO = true;
  } else {
    asParams.useCalo = true;
    asParams.useEcal = false;
    asParams.useHcal = false;
    asParams.useHO = false;
  }

  edm::ESHandle<MagneticField> bField;
  eventSetup.get<IdealMagneticFieldRecord>().get(bField);


  reco::TransientTrack tMuon(muon, &*bField);
  FreeTrajectoryState iFTS = tMuon.initialFreeState();
  TrackDetMatchInfo mInfo = theAssociator->associate(event, eventSetup, iFTS, asParams);

  depEcal.setVeto(Veto(Direction(mInfo.trkGlobPosAtEcal.eta(), mInfo.trkGlobPosAtEcal.phi()),
		       theDR_Veto_E));
  depHcal.setVeto(Veto(Direction(mInfo.trkGlobPosAtHcal.eta(), mInfo.trkGlobPosAtHcal.phi()),
		       theDR_Veto_H));
  depHOcal.setVeto(Veto(Direction(mInfo.trkGlobPosAtHO.eta(), mInfo.trkGlobPosAtHO.phi()),
			theDR_Veto_HO));

  if (theUseRecHitsFlag){
    edm::ESHandle<CaloGeometry> caloGeom;
    eventSetup.get<IdealGeometryRecord>().get(caloGeom);

    //Ecal
    std::vector<EcalRecHit>::const_iterator eHitCI = mInfo.ecalRecHits.begin();
    for (; eHitCI != mInfo.ecalRecHits.end(); ++eHitCI){
      GlobalPoint eHitPos = caloGeom->getPosition(eHitCI->detid());
      double deltar0 = deltaR(muon, eHitPos);
      double cosTheta = 1./cosh(eHitPos.eta());
      double energy = eHitCI->energy();
      double et = energy*cosTheta; 
      if (deltar0 > theDR_Max 
	  || ! (et > theThreshold_E && energy > 3*noiseRecHit(eHitCI->detid()))) continue;

      bool vetoHit = false;
      double deltar = 0;
      std::vector<EcalRecHit>::const_iterator vHitCI = mInfo.crossedEcalRecHits.begin();
      for(; vHitCI != mInfo.crossedEcalRecHits.end(); ++vHitCI){
	GlobalPoint vHitPos = caloGeom->getPosition(vHitCI->detid());
	deltar = deltaR(eHitPos, vHitPos);
	if (deltar < theDR_Veto_E ){
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Veto ECAL hit: Calo deltaR= " << deltar;
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Calo eta phi ethcal: " << eHitPos.eta() << " " << eHitPos.phi() << " " << et;
	  vetoHit = true;
	  break;	  
	}
      }
      if (vetoHit ){
	depEcal.addMuonEnergy(et);
      } else {
	depEcal.addDeposit(Direction(eHitPos.eta(), eHitPos.phi()), et);      
      }
    }

    //Hcal
    std::vector<HBHERecHit>::const_iterator hHitCI = mInfo.hcalRecHits.begin();
    for (; hHitCI != mInfo.hcalRecHits.end(); ++hHitCI){
      GlobalPoint hHitPos = caloGeom->getPosition(hHitCI->detid());
      double deltar0 = deltaR(muon, hHitPos);
      double cosTheta = 1./cosh(hHitPos.eta());
      double energy = hHitCI->energy();
      double et = energy*cosTheta;
      if (deltar0 > theDR_Max 
	  || ! (et > theThreshold_H && energy > 3*noiseRecHit(hHitCI->detid()))) continue;

      bool vetoHit = false;
      double deltar = 0;
      std::vector<HBHERecHit>::const_iterator vHitCI = mInfo.crossedHcalRecHits.begin();
      for(; vHitCI != mInfo.crossedHcalRecHits.end(); ++vHitCI){
	GlobalPoint vHitPos = caloGeom->getPosition(vHitCI->detid());
	deltar = deltaR(hHitPos, vHitPos);
	if (deltar < theDR_Veto_H ){
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Veto HBHE hit: Calo deltaR= " << deltar;
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Calo eta phi ethcal: " << hHitPos.eta() << " " << hHitPos.phi() << " " << et;
	  vetoHit = true;
	  break;	  
	}
      }
      if (vetoHit ){
	depHcal.addMuonEnergy(et);
      } else {
	depHcal.addDeposit(Direction(hHitPos.eta(), hHitPos.phi()), et);      
      }
    }

    //HOcal
    std::vector<HORecHit>::const_iterator hoHitCI = mInfo.hoRecHits.begin();
    for (; hoHitCI != mInfo.hoRecHits.end(); ++hoHitCI){
      GlobalPoint hoHitPos = caloGeom->getPosition(hoHitCI->detid());
      double deltar0 = deltaR(muon, hoHitPos);
      double cosTheta = 1./cosh(hoHitPos.eta());
      double energy = hoHitCI->energy();
      double et = energy*cosTheta;
      if (deltar0 > theDR_Max 
	  || ! (et > theThreshold_HO && energy > 3*noiseRecHit(hoHitCI->detid()))) continue;

      bool vetoHit = false;
      double deltar = 0;
      std::vector<HORecHit>::const_iterator vHitCI = mInfo.crossedHORecHits.begin();
      for(; vHitCI != mInfo.crossedHORecHits.end(); ++vHitCI){
	GlobalPoint vHitPos = caloGeom->getPosition(vHitCI->detid());
	deltar = deltaR(hoHitPos, vHitPos);
	if (deltar < theDR_Veto_HO ){
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Veto HO hit: Calo deltaR= " << deltar;
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Calo eta phi ethcal: " << hoHitPos.eta() << " " << hoHitPos.phi() << " " << et;
	  vetoHit = true;
	  break;	  
	}
      }
      if (vetoHit ){
	depHOcal.addMuonEnergy(et);
      } else {
	depHOcal.addDeposit(Direction(hoHitPos.eta(), hoHitPos.phi()), et);      	
      }
    }


  } else {
    //use calo towers    
    CaloTowerCollection::const_iterator calCI = mInfo.towers.begin();
    for (; calCI != mInfo.towers.end(); ++calCI){
      double deltar0 = deltaR(muon,*calCI);
      if (deltar0>theDR_Max) continue;
    
      double etecal = calCI->emEt();
      double eecal = calCI->emEnergy();
      bool doEcal = etecal>theThreshold_E && eecal>3*noiseEcal(*calCI);
      double ethcal = calCI->hadEt();
      double ehcal = calCI->hadEnergy();
      bool doHcal = ethcal>theThreshold_H && ehcal>3*noiseHcal(*calCI);
      double ethocal = calCI->outerEt();
      double ehocal = calCI->outerEnergy();
      bool doHOcal = ethocal>theThreshold_HO && ehocal>3*noiseHOcal(*calCI);
      if ((!doEcal) && (!doHcal) && (!doHcal)) continue;
    
      bool vetoTower = false;
      double deltar = 0;
      CaloTowerCollection::const_iterator calVetoCI = mInfo.crossedTowers.begin();
      for(; calVetoCI != mInfo.crossedTowers.end(); ++calVetoCI){
	deltar = deltaR(*calCI, *calVetoCI);
	if (deltar<theDR_Veto_H || deltar < theDR_Veto_E || deltar < theDR_Veto_HO) {
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Veto tower: Calo deltaR= " << deltar;
	  LogDebug("Muon|RecoMuon|L2MuonIsolationProducer")
	    << " >>> Calo eta phi ethcal: " << calCI->eta() << " " << calCI->phi() << " " << ethcal;
	  vetoTower = true;
	  break;
	}
      }

      Direction towerDir(calCI->eta(), calCI->phi());
      if (vetoTower){
	if (doEcal && deltar < theDR_Veto_E) depEcal.addMuonEnergy(etecal);
	if (doHcal && deltar < theDR_Veto_H) depHcal.addMuonEnergy(ethcal);
	if (doHOcal && deltar < theDR_Veto_HO) depHOcal.addMuonEnergy(ethocal);
      } else {	
	if (doEcal && deltar > theDR_Veto_E){
	  depEcal.addDeposit(towerDir, etecal);
	}
	if (doHcal && deltar > theDR_Veto_H){
	  depHcal.addDeposit(towerDir, ethcal);
	}
	if (doHOcal && deltar > theDR_Veto_HO){
	  depHOcal.addDeposit(towerDir, ethocal);
	}
      }
    }
  }

  std::vector<MuIsoDeposit> resultDeps;    
  resultDeps.push_back(depEcal);
  resultDeps.push_back(depHcal);
  resultDeps.push_back(depHOcal);

  return resultDeps;

}

double CaloExtractorByAssociator::PhiInRange(const double& phi) {
      double phiout = phi;

      if( phiout > 2*M_PI || phiout < -2*M_PI) {
            phiout = fmod( phiout, 2*M_PI);
      }
      if (phiout <= -M_PI) phiout += 2*M_PI;
      else if (phiout >  M_PI) phiout -= 2*M_PI;

      return phiout;
}

template <class T, class U>
double CaloExtractorByAssociator::deltaR(const T& t, const U& u) {
      return sqrt(pow(t.eta()-u.eta(),2) +pow(PhiInRange(t.phi()-u.phi()),2));
}

double CaloExtractorByAssociator::noiseEcal(const CaloTower& tower) const {
      double noise = theNoiseTow_EB;
      double eta = tower.eta();
      if (fabs(eta)>1.479) noise = theNoiseTow_EE;
      return noise;
}

double CaloExtractorByAssociator::noiseHcal(const CaloTower& tower) const {
  double noise = fabs(tower.eta())> 1.479 ? theNoise_HE : theNoise_HB;      
  return noise;
}

double CaloExtractorByAssociator::noiseHOcal(const CaloTower& tower) const {
      double noise = theNoise_HO;
      return noise;
}


double CaloExtractorByAssociator::noiseRecHit(const DetId& detId) const {
  double  noise = 100;
  DetId::Detector det = detId.det();
  if (det == DetId::Ecal){
    EcalSubdetector subDet = (EcalSubdetector)(detId.subdetId());
    if (subDet == EcalBarrel){
      noise = theNoise_EB;
    } else if (subDet == EcalEndcap){
      noise = theNoise_EE;
    }
  } else if (det == DetId::Hcal){
    HcalSubdetector subDet = (HcalSubdetector)(detId.subdetId());
    if (subDet == HcalBarrel){
      noise = theNoise_HB;
    } else if (subDet == HcalEndcap){
      noise = theNoise_HE;      
    } else if (subDet == HcalOuter){
      noise = theNoise_HO;
    }
  }
  return noise;
}
