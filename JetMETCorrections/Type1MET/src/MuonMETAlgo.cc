// File: MuonMETAlgo.cc
// Description:  see MuonMETAlgo.h
// Author: M. Schmitt, R. Cavanaugh, The University of Florida
// Creation Date:  MHS May 31, 2005 Initial version.
//
//--------------------------------------------
#include <math.h>
#include <vector>
#include "JetMETCorrections/Type1MET/interface/MuonMETAlgo.h"
#include "DataFormats/METReco/interface/CaloMET.h"
#include "DataFormats/METReco/interface/SpecificCaloMETData.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/CaloTowers/interface/CaloTower.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "Geometry/Records/interface/IdealGeometryRecord.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "DataFormats/Math/interface/Point3D.h"
#include "TMath.h"



using namespace std;
using namespace reco;

typedef math::XYZTLorentzVector LorentzVector;
typedef math::XYZPoint Point;

CaloMET MuonMETAlgo::makeMET (const CaloMET& fMet, 
			      double fSumEt, 
			      const vector<CorrMETData>& fCorrections, 
			      const MET::LorentzVector& fP4) {
  return CaloMET (fMet.getSpecific (), fSumEt, fCorrections, fP4, fMet.vertex ());
}
  
  
  
MET MuonMETAlgo::makeMET (const MET& fMet, 
			  double fSumEt, 
			  const vector<CorrMETData>& fCorrections, 
			  const MET::LorentzVector& fP4) {
  return MET (fSumEt, fCorrections, fP4, fMet.vertex ()); 
}

template <class T> void MuonMETAlgo::MuonMETAlgo_run(const edm::Event& iEvent,
						     const edm::EventSetup& iSetup, 
						     const vector<T>& v_uncorMET,
						     const edm::View<Muon>& inputMuons,
						     TrackDetectorAssociator& trackAssociator,
						     TrackAssociatorParameters& trackAssociatorParameters,
						     vector<T>* v_corMET,
						     bool useTrackAssociatorPositions,
						     bool useRecHits,
						     bool useHO,
						     double towerEtThreshold) {
  
  
  edm::ESHandle<MagneticField> magneticField;
  iSetup.get<IdealMagneticFieldRecord>().get(magneticField);
    
//get the B-field at the origin
  double Bfield = magneticField->inTesla(GlobalPoint(0.,0.,0.)).z();
  T uncorMETObj = v_uncorMET.front();
    
  double uncorMETX = uncorMETObj.px();
  double uncorMETY = uncorMETObj.py();
    
  double corMETX = uncorMETX;
  double corMETY = uncorMETY;
  
  CorrMETData delta;
  double sumMuEt=0;
  double sumMuDepEt = 0;
  for(edm::View<Muon>::const_iterator mus_it = inputMuons.begin();
      mus_it != inputMuons.end(); mus_it++) {

    bool useAverage = false;
    //decide whether or not we want to correct on average based 
    //on isolation information from the muon
    double sumPt   = mus_it->isIsolationValid()? mus_it->isolationR03().sumPt       : 0.0;
    double sumEtEcal = mus_it->isIsolationValid() ? mus_it->isolationR03().emEt     : 0.0;
    double sumEtHcal    = mus_it->isIsolationValid() ? mus_it->isolationR03().hadEt : 0.0;
    
    if(sumPt > 3 || sumEtEcal + sumEtHcal > 5) useAverage = true;

    //get the energy using TrackAssociator if
    //the muon turns out to be isolated
    MuonMETInfo muMETInfo;
    muMETInfo.useAverage = useAverage;
    muMETInfo.useTkAssociatorPositions = useTrackAssociatorPositions;
    muMETInfo.useHO = useHO;
    
    TrackRef mu_track = mus_it->combinedMuon();
    TrackDetMatchInfo info = 
      trackAssociator.associate(iEvent, iSetup,
				trackAssociator.getFreeTrajectoryState(iSetup, *mu_track),
				trackAssociatorParameters);
    if(useTrackAssociatorPositions) {
      muMETInfo.ecalPos  = info.trkGlobPosAtEcal;
      muMETInfo.hcalPos  = info.trkGlobPosAtHcal;
      muMETInfo.hoPos    = info.trkGlobPosAtHO;
    }
    
    if(!useAverage) {

      if(useRecHits) {
	muMETInfo.ecalE = mus_it->calEnergy().emS9;
	muMETInfo.hcalE = mus_it->calEnergy().hadS9;
	if(useHO) //muMETInfo.hoE is 0 by default
	  muMETInfo.hoE   = mus_it->calEnergy().hoS9;
      } else {// use Towers (this is the default)
	//only include towers whose Et > 0.5 since 
	//by default the MET only includes towers with Et > 0.5
	vector<const CaloTower*> towers = info.crossedTowers;
	for(vector<const CaloTower*>::const_iterator it = towers.begin();
	    it != towers.end(); it++) {
	  if( (*it)->et() <  towerEtThreshold) continue;
	  muMETInfo.ecalE =+ (*it)->emEt();
	  muMETInfo.hcalE =+ (*it)->hadEt();
	  if(useHO)
	    muMETInfo.hoE =+ (*it)->outerEt();
	}
      }//use Towers
    }
  
    
    sumMuEt += mus_it->et();
    double metxBeforeCorr = corMETX;
    double metyBeforeCorr = corMETY;
    
    
    //The tracker has better resolution for pt < 200 GeV
    math::XYZTLorentzVector mup4;
    if(mus_it->combinedMuon()->pt() < 200) {
      mup4 = LorentzVector(mus_it->innerTrack()->px(), mus_it->innerTrack()->py(),
			   mus_it->innerTrack()->pz(), mus_it->innerTrack()->p());
    } else {
      mup4 = LorentzVector(mus_it->combinedMuon()->px(), mus_it->combinedMuon()->py(),
			   mus_it->combinedMuon()->pz(), mus_it->combinedMuon()->p());
    }	
    
    //call function that does the work 
    correctMETforMuon(corMETX, corMETY, Bfield, mus_it->charge(),
		      mup4, mus_it->vertex(),
		      muMETInfo);
    double sumMuDepEx = corMETX - metxBeforeCorr + mus_it->px();
    double sumMuDepEy = corMETY - metyBeforeCorr + mus_it->py();
    sumMuDepEt += sqrt(pow(sumMuDepEx,2)+pow(sumMuDepEy,2));
    
  }
  
  double corMET   = sqrt(corMETX*corMETX   +   corMETY*corMETY    );
  delta.mex   = corMETX - uncorMETX;
  delta.mey   = corMETY - uncorMETY;
  //remove muon's contribution to sumEt and add in the 
  //muon Et from track
  delta.sumet = sumMuEt - sumMuDepEt;
    
  MET::LorentzVector correctedMET4vector( corMETX, corMETY, 0., corMET);
  //----------------- get previous corrections and push into new corrections
  std::vector<CorrMETData> corrections = uncorMETObj.mEtCorr();
  corrections.push_back( delta );
  
  T result = makeMET(uncorMETObj, uncorMETObj.sumEt()+delta.sumet, corrections, correctedMET4vector);
  v_corMET->push_back(result);
  
}
   
//----------------------------------------------------------------------------

void MuonMETAlgo::correctMETforMuon(double& metx, double& mety, double bfield, int muonCharge,
				    math::XYZTLorentzVector muonP4, math::XYZPoint muonVertex,
				    MuonMETInfo& muonMETInfo) {
  
  double mu_p     = muonP4.P();
  double mu_pt    = muonP4.Pt();
  double mu_phi   = muonP4.Phi();
  double mu_eta   = muonP4.Eta();
  double mu_vz    = muonVertex.z()/100.;
  double mu_pz    = muonP4.Pz();
  
  
  
  //add in the muon's pt
  metx -= mu_pt*cos(mu_phi);
  mety -= mu_pt*sin(mu_phi);

  double ecalPhi, ecalTheta;
  double hcalPhi, hcalTheta;
  double hoPhi, hoTheta;
  

  //should always be false for FWLite
  //unless you want to supply co-ordinates at 
  //the calorimeter sub-detectors yourself
  if(muonMETInfo.useTkAssociatorPositions) {
    ecalPhi   = muonMETInfo.ecalPos.Phi();
    ecalTheta = muonMETInfo.ecalPos.Theta();
    hcalPhi   = muonMETInfo.hcalPos.Phi();
    hcalTheta = muonMETInfo.hcalPos.Theta();
    hoPhi     = muonMETInfo.hoPos.Phi();
    hoTheta   = muonMETInfo.hoPos.Theta();
  } else {
    
    /*
      use the analytical solution for the
      intersection of a helix with a cylinder
      to find the positions of the muon
      at the various calo surfaces
    */
    
    //radii of subdetectors in meters
    double rEcal = 1.290;
    double rHcal = 1.9;
    double rHo   = 3.82;
    if(abs(mu_eta) > 0.3) rHo = 4.07;
    //distance from the center of detector to face of Ecal
    double zFaceEcal = 3.209;
    if(mu_eta < 0 ) zFaceEcal = -1*zFaceEcal;
    //distance from the center of detector to face of Hcal
    double zFaceHcal = 3.88;
    if(mu_eta < 0 ) zFaceHcal = -1*zFaceHcal;    
    
    //now we have to get Phi
    //bending radius of the muon (units are meters)
    double bendr = mu_pt*1000/(300*bfield);

    double tb_ecal = TMath::ACos(1-rEcal*rEcal/(2*bendr*bendr)); //helix time interval parameter
    double tb_hcal = TMath::ACos(1-rHcal*rHcal/(2*bendr*bendr)); //helix time interval parameter
    double tb_ho   = TMath::ACos(1-rHo*rHo/(2*bendr*bendr));     //helix time interval parameter
    double xEcal,yEcal,zEcal;
    double xHcal,yHcal,zHcal; 
    double xHo, yHo,zHo;
    //Ecal
    //in the barrel and if not a looper
    if(fabs(mu_pz*bendr*tb_ecal/mu_pt+mu_vz) < fabs(zFaceEcal) && rEcal < 2*bendr) { 
      xEcal = bendr*(TMath::Sin(tb_ecal+mu_phi)-TMath::Sin(mu_phi));
      yEcal = bendr*(-TMath::Cos(tb_ecal+mu_phi)+TMath::Cos(mu_phi));
      zEcal = bendr*tb_ecal*mu_pz/mu_pt + mu_vz;
    } else { //endcap 
      if(mu_pz > 0) {
        double te_ecal = (fabs(zFaceEcal) - mu_vz)*mu_pt/(bendr*mu_pz);
        xEcal = bendr*(TMath::Sin(te_ecal+mu_phi) - TMath::Sin(mu_phi));
        yEcal = bendr*(-TMath::Cos(te_ecal+mu_phi) + TMath::Cos(mu_phi));
        zEcal = fabs(zFaceEcal);
      } else {
        double te_ecal = -(fabs(zFaceEcal) + mu_vz)*mu_pt/(bendr*mu_pz);
        xEcal = bendr*(TMath::Sin(te_ecal+mu_phi) - TMath::Sin(mu_phi));
        yEcal = bendr*(-TMath::Cos(te_ecal+mu_phi) + TMath::Cos(mu_phi));
        zEcal = -fabs(zFaceEcal);
      }
    }

    //Hcal
    if(fabs(mu_pz*bendr*tb_hcal/mu_pt+mu_vz) < fabs(zFaceHcal) && rEcal < 2*bendr) { //in the barrel
      xHcal = bendr*(TMath::Sin(tb_hcal+mu_phi)-TMath::Sin(mu_phi));
      yHcal = bendr*(-TMath::Cos(tb_hcal+mu_phi)+TMath::Cos(mu_phi));
      zHcal = bendr*tb_hcal*mu_pz/mu_pt + mu_vz;
    } else { //endcap 
      if(mu_pz > 0) {
        double te_hcal = (fabs(zFaceHcal) - mu_vz)*mu_pt/(bendr*mu_pz);
        xHcal = bendr*(TMath::Sin(te_hcal+mu_phi) - TMath::Sin(mu_phi));
        yHcal = bendr*(-TMath::Cos(te_hcal+mu_phi) + TMath::Cos(mu_phi));
        zHcal = fabs(zFaceHcal);
      } else {
        double te_hcal = -(fabs(zFaceHcal) + mu_vz)*mu_pt/(bendr*mu_pz);
        xHcal = bendr*(TMath::Sin(te_hcal+mu_phi) - TMath::Sin(mu_phi));
        yHcal = bendr*(-TMath::Cos(te_hcal+mu_phi) + TMath::Cos(mu_phi));
        zHcal = -fabs(zFaceHcal);
      }
    }
    
    //Ho - just the barrel
    xHo = bendr*(TMath::Sin(tb_ho+mu_phi)-TMath::Sin(mu_phi));
    yHo = bendr*(-TMath::Cos(tb_ho+mu_phi)+TMath::Cos(mu_phi));
    zHo = bendr*tb_ho*mu_pz/mu_pt + mu_vz;  
    
    ecalTheta = TMath::ACos(zEcal/sqrt(pow(xEcal,2) + pow(yEcal,2)+pow(zEcal,2)));
    ecalPhi   = atan2(yEcal,xEcal);
    hcalTheta = TMath::ACos(zHcal/sqrt(pow(xHcal,2) + pow(yHcal,2)+pow(zHcal,2)));
    hcalPhi   = atan2(yHcal,xHcal);
    hoTheta   = TMath::ACos(zHo/sqrt(pow(xHo,2) + pow(yHo,2)+pow(zHo,2)));
    hoPhi     = atan2(yHo,xHo);

    //2d radius in x-y plane
    double r2dEcal = sqrt(pow(xEcal,2)+pow(yEcal,2));
    double r2dHcal = sqrt(pow(xHcal,2)+pow(yHcal,2));
    double r2dHo   = sqrt(pow(xHo,2)  +pow(yHo,2));
    
    /*
      the above prescription is for right handed helicies only
      Positively charged muons trace a left handed helix
      so we correct for that 
    */
    if(muonCharge > 0) {
         
      //Ecal
      double dphi = mu_phi - ecalPhi;
      if(fabs(dphi) > TMath::Pi()) 
        dphi = 2*TMath::Pi() - fabs(dphi);
      ecalPhi = mu_phi - fabs(dphi);
      if(fabs(ecalPhi) > TMath::Pi()) {
        double temp = 2*TMath::Pi() - fabs(ecalPhi);
        ecalPhi = -1*temp*ecalPhi/fabs(ecalPhi);
      }
      xEcal = r2dEcal*TMath::Cos(ecalPhi);
      yEcal = r2dEcal*TMath::Sin(ecalPhi);
      
      //Hcal
      dphi = mu_phi - hcalPhi;
      if(fabs(dphi) > TMath::Pi()) 
        dphi = 2*TMath::Pi() - fabs(dphi);
      hcalPhi = mu_phi - fabs(dphi);
      if(fabs(hcalPhi) > TMath::Pi()) {
        double temp = 2*TMath::Pi() - fabs(hcalPhi);
        hcalPhi = -1*temp*hcalPhi/fabs(hcalPhi);
      }
      xHcal = r2dHcal*TMath::Cos(hcalPhi);
      yHcal = r2dHcal*TMath::Sin(hcalPhi);
         
      //Ho
      dphi = mu_phi - hoPhi;
      if(fabs(dphi) > TMath::Pi())
        dphi = 2*TMath::Pi() - fabs(dphi);
      hoPhi = mu_phi - fabs(dphi);
      if(fabs(hoPhi) > TMath::Pi()) {
        double temp = 2*TMath::Pi() - fabs(hoPhi);
        hoPhi = -1*temp*hoPhi/fabs(hoPhi);
      }
      xHo = r2dHo*TMath::Cos(hoPhi);
      yHo = r2dHo*TMath::Sin(hoPhi);
      
    }
  }
  
  //for isolated muons
  if(!muonMETInfo.useAverage) {
    
    double mu_Ex =  muonMETInfo.ecalE*sin(ecalTheta)*cos(ecalPhi)
      + muonMETInfo.hcalE*sin(hcalTheta)*cos(hcalPhi)
      + muonMETInfo.hoE*sin(hoTheta)*cos(hoPhi);
    double mu_Ey =  muonMETInfo.ecalE*sin(ecalTheta)*sin(ecalPhi)
      + muonMETInfo.hcalE*sin(hcalTheta)*sin(hcalPhi)
      + muonMETInfo.hoE*sin(hoTheta)*sin(hoPhi);

    metx += mu_Ex;
    mety += mu_Ey;
    
  } else { //non-isolated muons - derive the correction
    
    //dE/dx in matter for iron:
    //-(11.4 + 0.96*fabs(log(p0*2.8)) + 0.033*p0*(1.0 - pow(p0, -0.33)) )*1e-3
    //from http://cmslxr.fnal.gov/lxr/source/TrackPropagation/SteppingHelixPropagator/src/SteppingHelixPropagator.ccyes,
    //line ~1100
    //normalisation is at 50 GeV
    double dEdx_normalization = -(11.4 + 0.96*fabs(log(50*2.8)) + 0.033*50*(1.0 - pow(50, -0.33)) )*1e-3;
    double dEdx_numerator     = -(11.4 + 0.96*fabs(log(mu_p*2.8)) + 0.033*mu_p*(1.0 - pow(mu_p, -0.33)) )*1e-3;
    
    double temp = 0.0;
    
    if(muonMETInfo.useHO) {
      //for the Towers, with HO
      if(fabs(mu_eta) < 0.2)
	temp = 2.75*(1-0.00003*mu_p);
      if(fabs(mu_eta) > 0.2 && fabs(mu_eta) < 1.0)
	temp = (2.38+0.0144*fabs(mu_eta))*(1-0.0003*mu_p);
      if(fabs(mu_eta) > 1.0 && fabs(mu_eta) < 1.3)
	temp = 7.413-5.12*fabs(mu_eta);
      if(fabs(mu_eta) > 1.3)
	temp = 2.084-0.743*fabs(mu_eta);
    } else {
      if(fabs(mu_eta) < 1.0)
	temp = 2.33*(1-0.0004*mu_p);
      if(fabs(mu_eta) > 1.0 && fabs(mu_eta) < 1.3)
	temp = (7.413-5.12*fabs(mu_eta))*(1-0.0003*mu_p);
      if(fabs(mu_eta) > 1.3)
	temp = 2.084-0.743*fabs(mu_eta);
    }

    double dep = temp*dEdx_normalization/dEdx_numerator;
    if(dep < 0.5) dep = 0;
    //use the average phi of the 3 subdetectors
    if(fabs(mu_eta) < 1.3) {
      metx += dep*cos((ecalPhi+hcalPhi+hoPhi)/3);
      mety += dep*sin((ecalPhi+hcalPhi+hoPhi)/3);
    } else {
      metx += dep*cos( (ecalPhi+hcalPhi)/2);
      mety += dep*cos( (ecalPhi+hcalPhi)/2);
    }
  }

  
}
//----------------------------------------------------------------------------
void MuonMETAlgo::run(const edm::Event& iEvent,
		      const edm::EventSetup& iSetup,
		      const reco::METCollection& uncorMET, 
		      const edm::View<Muon>& Muons, 
		      TrackDetectorAssociator& trackAssociator,
		      TrackAssociatorParameters& trackAssociatorParameters,
		      METCollection *corMET,
		      bool useTrackAssociatorPositions,
		      bool useRecHits,
		      bool useHO,
		      double towerEtThreshold) {
		     
  
  return MuonMETAlgo_run(iEvent, iSetup, uncorMET, Muons, 
			 trackAssociator, trackAssociatorParameters,
			 corMET, useTrackAssociatorPositions,
			 useRecHits, useHO, towerEtThreshold);
}


//----------------------------------------------------------------------------
void MuonMETAlgo::run(const edm::Event& iEvent,
		      const edm::EventSetup& iSetup,
		      const reco::CaloMETCollection& uncorMET, 
		      const edm::View<Muon>& Muons,
		      TrackDetectorAssociator& trackAssociator,
		      TrackAssociatorParameters& trackAssociatorParameters,
		      CaloMETCollection *corMET,
		      bool useTrackAssociatorPositions,
		      bool useRecHits,
		      bool useHO,
		      double towerEtThreshold) {
		      
  
  return MuonMETAlgo_run(iEvent, iSetup, uncorMET, Muons,
			 trackAssociator, trackAssociatorParameters,
			 corMET, useTrackAssociatorPositions,
			 useRecHits, useHO, towerEtThreshold);
}


//----------------------------------------------------------------------------
MuonMETAlgo::MuonMETAlgo() {}
//----------------------------------------------------------------------------
  
//----------------------------------------------------------------------------
MuonMETAlgo::~MuonMETAlgo() {}
//----------------------------------------------------------------------------

