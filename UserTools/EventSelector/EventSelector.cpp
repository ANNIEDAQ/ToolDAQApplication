#include "EventSelector.h"

EventSelector::EventSelector():Tool(){}


bool EventSelector::Initialise(std::string configfile, DataModel &data){

  /////////////////// Usefull header ///////////////////////
  if(configfile!="")  m_variables.Initialise(configfile); //loading config file
  //m_variables.Print();

  m_data= &data; //assigning transient data pointer
  /////////////////////////////////////////////////////////////////

  //Get the tool configuration variables
  m_variables.Get("verbosity",verbosity);
  m_variables.Get("RunMRDRecoCut", fRunMRDRecoCut);
  m_variables.Get("RunMCTruthCut", fRunMCTruthCut);

  /// Construct the other objects we'll be setting at event level,
  fMuonStartVertex = new RecoVertex();
  fMuonStopVertex = new RecoVertex();
  
  // Make the RecoDigit Store if it doesn't exist
  int recoeventexists = m_data->Stores.count("RecoEvent");
  if(recoeventexists==0) m_data->Stores["RecoEvent"] = new BoostStore(false,2);
  m_data->Stores.at("RecoEvent")->Set("EvtSelectionMask", fEvtSelectionMask); 
  return true;
}


bool EventSelector::Execute(){
  // Reset everything
  this->Reset();
  
  // see if "ANNIEEvent" exists
  auto get_annieevent = m_data->Stores.count("ANNIEEvent");
  if(!get_annieevent){
  	Log("EventSelector Tool: No ANNIEEvent store!",v_error,verbosity); 
  	return false;
  };

	// Retrieve particle information from ANNIEEvent
  auto get_mcparticles = m_data->Stores.at("ANNIEEvent")->Get("MCParticles",fMCParticles);
	if(!get_mcparticles){ 
		Log("EventSelector:: Tool: Error retrieving MCParticles from ANNIEEvent!",v_error,verbosity); 
		return false; 
	}
	
	/// check if "MRDTracks" store exists
	int mrdtrackexists = m_data->Stores.count("MRDTracks");
	if(mrdtrackexists == 0) {
	  Log("EventSelector Tool: MRDTracks store doesn't exist!",v_message,verbosity);
	  return false;
	} 	
	
  /// Find true neutrino vertex which is defined by the start point of the Primary muon
  this->FindTrueVertexFromMC();
  fEventCutStatus = true;
  if(fRunMCTruthCut){
    bool passMCTruth= this->EventSelectionByMCTruthInfo();
    if(!passMCTruth) fEventCutStatus = false; 
  }
  //FIXME: This isn't working according to Jingbo
  if(fRunMRDRecoCut){
    std::cout << "EventSelector Tool: Currently not implemented. Setting to false" << std::endl;
    Log("EventSelector Tool: MRDReco not implemented.  Setting cut bit to false",v_message,verbosity);
    bool passMRDRecoCut = false:
    //bool passMRDRecoCut = this->EventSelectionByMRDReco(); 
    if(!passMRDRecoCut) fEventCutStatus = false; 
  //}
  // Event selection successfully run
  EventSelectionRan = true;
  m_data->Stores.at("RecoEvent")->Set("EventCutStatus", fEventCutStatus);
  return true;
}


bool EventSelector::Finalise(){

  return true;
}

RecoVertex* EventSelector::FindTrueVertexFromMC() {
  
  // loop over the MCParticles to find the highest enery primary muon
  // MCParticles is a std::vector<MCParticle>
  MCParticle primarymuon;  // primary muon
  bool mufound=false;
  if(fMCParticles){
    Log("EventSelector::  Tool: Num MCParticles = "+to_string(fMCParticles->size()),v_message,verbosity);
    for(int particlei=0; particlei<fMCParticles->size(); particlei++){
      MCParticle aparticle = fMCParticles->at(particlei);
      //if(v_debug<verbosity) aparticle.Print();       // print if we're being *really* verbose
      if(aparticle.GetParentPdg()!=0) continue;      // not a primary particle
      if(aparticle.GetPdgCode()!=13) continue;       // not a muon
      primarymuon = aparticle;                       // note the particle
      mufound=true;                                  // note that we found it
      //primarymuon.Print();
      break;                                         // won't have more than one primary muon
    }
  } else {
    Log("EventSelector::  Tool: No MCParticles in the event!",v_error,verbosity);
  }
  if(not mufound){
    Log("EventSelector::  Tool: No muon in this event",v_warning,verbosity);
    return 0;
  }
  
  // retrieve desired information from the particle
  Position muonstartpos = primarymuon.GetStartVertex();    // only true if the muon is primary
  double muonstarttime = primarymuon.GetStartTime().GetNs() + primarymuon.GetStopTime().GetTpsec()/1000;
  Position muonstoppos = primarymuon.GetStopVertex();    // only true if the muon is primary
  double muonstoptime = primarymuon.GetStopTime().GetNs() + primarymuon.GetStopTime().GetTpsec()/1000;
  
  Direction muondirection = primarymuon.GetStartDirection();
  double muonenergy = primarymuon.GetStartEnergy();
  // set true vertex
  // change unit
  muonstartpos.UnitToCentimeter(); // convert unit from meter to centimeter
  muonstoppos.UnitToCentimeter(); // convert unit from meter to centimeter
  // change coordinate for muon start vertex
  muonstartpos.SetY(muonstartpos.Y()+14.46469);
  muonstartpos.SetZ(muonstartpos.Z()-168.1);
  fMuonStartVertex->SetVertex(muonstartpos, muonstarttime);
  fMuonStartVertex->SetDirection(muondirection);
  //  charge coordinate for muon stop vertex
  muonstoppos.SetY(muonstoppos.Y()+14.46469);
  muonstoppos.SetZ(muonstoppos.Z()-168.1);
  fMuonStopVertex->SetVertex(muonstoppos, muonstoptime); 
  
  logmessage = "  trueVtx=(" +to_string(muonstartpos.X()) + ", " + to_string(muonstartpos.Y()) + ", " + to_string(muonstartpos.Z()) +", "+to_string(muonstarttime)+ "\n"
            + "           " +to_string(muondirection.X()) + ", " + to_string(muondirection.Y()) + ", " + to_string(muondirection.Z()) + ") " + "\n";
  Log(logmessage,v_debug,verbosity);
  return fMuonStartVertex;
}

void EventSelector::PushTrueVertex(bool savetodisk) {
  Log("EventSelector Tool: Push true vertex to the RecoEvent store",v_message,verbosity);
  m_data->Stores.at("RecoEvent")->Set("TrueVertex", fMuonStartVertex, savetodisk); 
}


// This function isn't working now, because the MRDTracks store must have been changed. 
// We have to contact Marcus and ask how we can retieve the MRD track information. 
bool EventSelector::EventSelectionByMRDReco() {
  /// Get number of subentries
  int NumMrdTracks = 0;
  double mrdtracklength = 0.;
  double MaximumMRDTrackLength = 0;
  int mrdlayershit = 0;
  bool mrdtrackisstoped = false;
  int longesttrackEntryNumber = -9999;
  
  auto getmrdtracks = m_data->Stores.at("MRDTracks")->Get("NumMrdTracks",NumMrdTracks);
  if(!getmrdtracks) {
    Log("EventSelector Tool: Error retrieving MRDTracks Store!",v_error,verbosity); 
  	return false;	
  }
  logmessage = "EventSelector Tool: Found " + to_string(NumMrdTracks) + " MRD tracks";
  Log(logmessage,v_message,verbosity);
  
  // If mrd track isn't found
  if(NumMrdTracks == 0) {
    Log("EventSelector Tool: Found no MRD Tracks",v_message,verbosity);
    return false;
  }
  // If mrd track is found
  // MRD tracks are saved in the "MRDTracks" Store. Each entry is an MRD track
  
  for(int entrynum=0; entrynum<NumMrdTracks; entrynum++) {
    //m_data->Stores["MRDTracks"]->GetEntry(entrynum);
    m_data->Stores.at("MRDTracks")->Get("TrackLength",mrdtracklength);
    m_data->Stores.at("MRDTracks")->Get("LayersHit",mrdlayershit);
    if(MaximumMRDTrackLength<mrdtracklength) {
      MaximumMRDTrackLength = mrdtracklength;
      longesttrackEntryNumber = entrynum;
    }
  }
  // check if the longest track is stopped inside MRD
  //m_data->Stores["MRDTracks"]->GetEntry(longesttrackEntryNumber);
  m_data->Stores.at("MRDTracks")->Get("IsMrdStopped",mrdtrackisstoped);
  if(!mrdtrackisstoped) return false;
  return true;
}


bool EventSelector::EventSelectionByMCTruthInfo() {
  if(!fMuonStartVertex) return false;
  
  double trueVtxX, trueVtxY, trueVtxZ;
  Position vtxPos = fMuonStartVertex->GetPosition();
  Direction vtxDir = fMuonStartVertex->GetDirection();
  trueVtxX = vtxPos.X();
  trueVtxY = vtxPos.Y();
  trueVtxZ = vtxPos.Z();
  
  // fiducial volume cut
  double tankradius = ANNIEGeometry::Instance()->GetCylRadius();	
  double fidcutradius = 0.8 * tankradius;
  double fidcuty = 50.;
  double fidcutz = 0.;
  if(trueVtxZ > fidcutz) return false;
  if( (TMath::Sqrt(TMath::Power(trueVtxX, 2) + TMath::Power(trueVtxZ,2)) > fidcutradius) 
  	  || (TMath::Abs(trueVtxY) > fidcuty) 
  	  || (trueVtxZ > fidcutz) ){
  return false;
  }	 	
  
//	// mrd cut
//	double muonStopX, muonStopY, muonStopZ;
//	Position muonStopPos = fMuonStopVertex->GetPosition();
//	muonStopX = muonStopPos.X();
//	muonStopY = muonStopPos.Y();
//	muonStopZ = muonStopPos.Z();
//	
//	// The following dimensions are inaccurate. We need to get these numbers from the geometry class
//	double distanceBetweenTankAndMRD = 10.; // about 10 cm
//	double mrdThicknessZ = 60.0;
//	double mrdWidthX = 305.0;
//	double mrdHeightY = 274.0;
//	if(muonStopZ<tankradius + distanceBetweenTankAndMRD || muonStopZ>tankradius + distanceBetweenTankAndMRD + mrdThicknessZ
//		|| muonStopX<-1.0*mrdWidthX/2 || muonStopX>mrdWidthX/2
//		|| muonStopY<-1.0*mrdHeightY/2 || muonStopY>mrdHeightY/2) {
//	  return false;	
//	}
	
  return true;	
}

void EventSelector::Reset() {
  // Reset 
  fMuonStartVertex->Reset();
  fMuonStopVertex->Reset();
  fEventSelectionRan = false;
  m_data->Stores.at("RecoEvent")->Set("EventSelectionRan", fEventSelectionRan);
} 