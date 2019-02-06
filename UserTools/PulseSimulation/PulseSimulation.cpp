/* vim:set noexpandtab tabstop=4 wrap */
#include "PulseSimulation.h"
#include "CreateFakeRawFile.cpp"
#include "GetTemplateRunInfo.cpp"

PulseSimulation::PulseSimulation():Tool(){}

bool PulseSimulation::Initialise(std::string configfile, DataModel &data){
	
	if(configfile!="")  m_variables.Initialise(configfile); //loading config file
	//m_variables.Print();
	m_data= &data; //assigning transient data pointer
	m_variables.Get("verbosity",verbosity);
		
	// Read ADC constants from file
	m_variables.Get("MinibuffersPerFullbuffer",minibuffers_per_fullbuffer);
	m_variables.Get("PMTGain",PMT_gain);
	m_variables.Get("PulseHeightFudgeFactor",PULSE_HEIGHT_FUDGE_FACTOR);
	m_variables.Get("DrawDebugPlots",DRAW_DEBUG_PLOTS);
	
	/////////////////////////////////////////////////////////////////
	
	Log("PulseSimulation Tool: Initializing",v_message,verbosity);
	Geometry* anniegeom;
	get_ok = m_data->Stores.at("ANNIEEvent")->Header->Get("AnnieGeometry",anniegeom);
	int numtankpmts = anniegeom->GetNumTankPMTs();
	Log("PulseSimulation Tool: constructing cards for "+to_string(numtankpmts)+"PMTs",v_debug,verbosity);
	cout<<"channels_per_adc_card="<<channels_per_adc_card<<endl;
	num_adc_cards = (numtankpmts-1)/channels_per_adc_card + 1; // rounded up
	Log("PulseSimulation Tool: Constructed "+to_string(num_adc_cards)+" ADC cards",v_debug,verbosity);
	
	Log("Creating Output Files",v_debug,verbosity);
	LoadOutputFiles();
	
	minibuffer_id=0;
	sequence_id=0;
	
	// TODO: add RWM
	
	// create the ROOT application to show debug plots
	if(DRAW_DEBUG_PLOTS){
		int myargc=0;
		char *myargv[] = {(const char*)"somestring"};
		pulseRootDrawApp = new TApplication("PulseRootDrawApp",&myargc,myargv);
		canvwidth = 700;
		canvheight = 600;
	}
	
	return true;
}

bool PulseSimulation::Execute(){
	
	Log("PulseSimulation Tool: Executing",v_debug,verbosity);
	if(m_data->Stores.count("ANNIEEvent")==0){
		Log("PulseSimulation Tool: Found no ANNIEEvent!!", v_error,verbosity);
		return false;
	}
	get_ok = m_data->Stores.at("ANNIEEvent")->Get("MCTriggernum",triggernum);
	if(not get_ok){
		Log("PulseSimulation Tool: Failed to get MCTriggernum from ANNIEEvent",v_error,verbosity);
		return false;
	}
	cout<<"checking if skipping this minibuffer"<<endl;
	if(skippingremainders && triggernum!=0){
		// we've got a full buffer, can't process this readout
		Log("PulseSimulation Tool: Skipping readout as we have a full buffer",v_debug,verbosity);
		return true;
	} else if(skippingremainders && triggernum==0){
		skippingremainders=false;  // next event, we've read out the DAQ by now
	}
	
	// Get the Hits
	std::map<ChannelKey,std::vector<Hit>>* MCHits=nullptr;
	std::cout<<"getting MCHits"<<endl;
	get_ok = m_data->Stores.at("ANNIEEvent")->Get("MCHits",MCHits);
	if(not get_ok){
		Log("PulseSimulation Tool: Failed to get hits from ANNIEEvent!",v_error,verbosity);
		return false;
	}
	
	int numMCtriggersthisevt;
	std::cout<<"getting numtriggersthisevt"<<std::endl;
	get_ok = m_data->CStore.Get("NumTriggersThisMCEvt",numMCtriggersthisevt);
	if(not get_ok){
		Log("PulseSimulation Tool: Failed to get NumTriggersThisMCEvt from ANNIEEvent",v_error,verbosity);
		return false;
	}
	bool droppingremainingsubtriggers = 
		((minibuffer_id+1)==minibuffers_per_fullbuffer) &&      // we're filling last minibuffer
		((triggernum+1)!=(numMCtriggersthisevt));               // but there are more subtriggers
	
	// if starting a fresh readout, set the top level (non-minibuffer level) readout details
	if(minibuffer_id==0){
		Log("PulseSimulation Tool: Minibuffer_ID=0 -> Constructing a new Readout",v_debug,verbosity);
		ConstructEmulatedPmtDataReadout();
	}
	Log("PulseSimulation Tool: Adding minibuffer start time",v_debug,verbosity);
	if(droppingremainingsubtriggers){
		Log("PulseSimulation Tool: Will skip remaining delayed minibuffers as we have a full buffer",v_debug,verbosity);
	}
	AddMinibufferStartTime(droppingremainingsubtriggers);
	
	// convert hits into pulses
	Log("PulseSimulation Tool: Looping over Digits",v_debug,verbosity);
	cout<<"we had "<<MCHits->size()<<" hit PMTs"<<endl;
	for(std::pair<ChannelKey,std::vector<Hit>>&& hitsonapmt : (*MCHits)){
		std::vector<Hit>* hitsonthistube = &(hitsonapmt.second);
		if(verbosity<v_debug){
			int thetubeid=hitsonthistube->front().GetTubeId();
			int thechannelid=(thetubeid)%channels_per_adc_card;
			int thecardid=(thetubeid-thechannelid)/channels_per_adc_card;
			cout<<"next PMT is tube "<<thetubeid
				<<", corresponding to card "<<thecardid
				<<", channel "<<thechannelid
				<<"; this PMT had "<<hitsonthistube->size()<<" hits"<<endl;
		}
		for(Hit apmthit : (*hitsonthistube)){
			AddPMTDataEntry(&apmthit);
		}
	}
	cout<<"done adding hits"<<endl;
	
	// advance the counter of triggers (minibuffers)
	minibuffer_id++;
	if(minibuffer_id==minibuffers_per_fullbuffer){
		Log("PulseSimulation Tool: Filling Emulated Data Tree",v_debug,verbosity);
		FillEmulatedPMTData();
		sequence_id++;
		minibuffer_id=0;
	}
	cout<<"done filling emulated ttree"<<endl;
	
	// raw file emulation: remaining sub-triggers after the last minibuffer fall in DAQ deadtime
	if(minibuffer_id==0){
		Log("PulseSimulation Tool: Filled buffer: will skip any remaining delayed triggers",v_debug,verbosity);
		skippingremainders=true;
	}
	
	//std::this_thread::sleep_for (std::chrono::seconds(5));   // a little wait so we can look at histos
	
	logmessage = "PulseSimulation Tool: Minibuffer_id=" + to_string((minibuffer_id==0) ? 40 : (minibuffer_id-1))
			+ ", sequence_id=" + to_string(sequence_id);
	Log(logmessage,v_debug,verbosity);
	
	return true;
}

bool PulseSimulation::Finalise(){
	
	if(minibuffer_id!=0){
		// since we only write out full buffers, write out one last time any partial buffers
		Log("PulseSimulation Tool: Filling Final Partial Readout",v_debug,verbosity);
		FillEmulatedPMTData();
	}
	
	Log("PulseSimulation Tool: Writing events to file",v_debug,verbosity);
	
	timingfileout->cd();
	theftydb->Write("heftydb",TObject::kOverwrite);
	timingfileout->Close();
	delete timingfileout;
	timingfileout=nullptr;
	
	rawfileout->cd();
	tPMTData->Write("PMTData",TObject::kOverwrite);
	tRunInformation->Write("RunInformation",TObject::kOverwrite);
	rawfileout->Close();
	delete rawfileout;
	rawfileout=nullptr;
	
	if(DRAW_DEBUG_PLOTS){
		if(pulseRootDrawApp){ delete pulseRootDrawApp; pulseRootDrawApp=nullptr; }
		if(gROOT->FindObject("landaucanvas"))    { delete landaucanvas; landaucanvas=nullptr; }
		if(gROOT->FindObject("buffercanvas"))    { delete buffercanvas; buffercanvas=nullptr; }
		if(gROOT->FindObject("buffergraph"))     { delete buffergraph; buffergraph=nullptr; }
		if(gROOT->FindObject("pulsecanvas"))     { delete pulsecanvas; pulsecanvas=nullptr; }
		if(gROOT->FindObject("pulsegraph"))      { delete pulsegraph; pulsegraph=nullptr; }
		if(gROOT->FindObject("fullbuffergraph")) { delete fullbuffergraph; fullbuffergraph=nullptr; }
	}
	
	return true;
}

// #######################################################################

void PulseSimulation::AddPMTDataEntry(Hit* digihit){
	// Construct and add the waveform from a PMT digit to the appropriate ADC trace
	// ============================================================================
	//Log("PulseSimulation Tool: adding digit",v_debug,verbosity);
	
	//Hit has methods GetTubeId(), GetTime(), GetCharge()
	int channelnum = (digihit->GetTubeId())%channels_per_adc_card;
	int cardid = ((digihit->GetTubeId())-channelnum)/channels_per_adc_card;
	
	/* digit time is relative to the trigger time (ndigits threshold crossing).
		 we need to convert this to position of the digit within the minibuffer data array 
		 and construct a waveform centred at that location with suitable integral to represent its charge. */
	
	// first get the digit time relative to start of minibuffer
	// XXX pre-trigger window may depend on trigger type XXX FIXME
	int digits_time_ns = digihit->GetTime() + abs(pre_trigger_window_ns);
	// convert to index of this time in the minibuffer waveform array
	int digits_time_index = digits_time_ns / ADC_NS_PER_SAMPLE;
//	cout<<"hit was at "<<digihit->GetTime()<<"ns relative to the trigger; with pre-trigger window size of "
//		<<abs(pre_trigger_window_ns)<<" and ADC_NS_PER_SAMPLE of "<<ADC_NS_PER_SAMPLE
//		<<" this places the hit at index "<<digits_time_index<<endl;
	
	// we need to scale the area of the generated pulse so that the amplitude is in ADC counts
	// and length is in samples
	// 0. convert digihit->GetCharge() photoelectrons (?) into total electrons by * gain ~few*10^7
	// 1. convert total electrons to Coulombs by * e.
	// 2. C corresponds to y axis I[A], x axis Time[s].
	// 3. change y axis from I to ADC => ADC = V/ADC_TO_VOLT = I*ADC_INPUT_R/ADC_TO_VOLT
	// 3. change x axis from s to samples (ns/NS_PER_SAMPLE) => duration[s] = samples * 1e9 * (1/NS_PER_SAMPLE)
	// so total area scaling = digihit->GetQ() * gain * e * (ADC_TO_VOLT/ADC_INPUT_R) * 1e9 * (1/NS_PER_SAMPLE) 
	
	// FIXME what's going on with this
	double adjusted_digit_q = digihit->GetCharge() * (1./ADC_NS_PER_SAMPLE) /* * pow(10.,9.) */
		* (ADC_INPUT_RESISTANCE/ADC_TO_VOLT) * PULSE_HEIGHT_FUDGE_FACTOR;
	logmessage = "PulseSimulation Tool: Digit Charge is: " + to_string(digihit->GetCharge())
		+ ", area calculated to be: " + to_string(adjusted_digit_q);
	//Log(logmessage,v_debug,verbosity);
	
	// construct a suitable pulse waveform using the position and charge of the digit
	pulsevector.assign(minibuffer_datapoints_per_channel,0);  // clear the temporary pulse vector
	// fill it with a pulse waveform with suitable form and size
	//Log("PulseSimulation Tool: Generating Waveform for this pulse",v_debug,verbosity);
	GenerateMinibufferPulse(digits_time_index, adjusted_digit_q, pulsevector);
	
	// calculate the offset of the minibuffer, for this channel, within the full buffer for the readout
	int channeloffset = channelnum * (full_buffer_size / channels_per_adc_card);
	int minibufferoffset = minibuffer_id*minibuffer_datapoints_per_channel;
//	cout<<"channeloffset="<<channeloffset<<", minibufferoffset="<<minibufferoffset
//		<<", digitoffset="<<digits_time_index<<" : pulse should be at "
//		<<(channeloffset+minibufferoffset+digits_time_index)<<" in the full buffer"<<endl;
	
	// get the full data array for this card, and an iterator to the appropriate start point
	std::vector<uint16_t>* thiscards_fullbuffer = &temporary_databuffers.at(cardid);
	std::vector<uint16_t>::iterator minibuffer_start = thiscards_fullbuffer->begin() + channeloffset + minibufferoffset;
	
	// add the generated pulse to the minibuffer at the appropriate location
	//Log("PulseSimulation Tool: Adding pulse to full trace",v_debug,verbosity);
	std::transform(minibuffer_start, minibuffer_start+minibuffer_datapoints_per_channel, pulsevector.begin(),
		minibuffer_start, std::plus<uint16_t>() );
	
//	int pulsepeakindex=(channeloffset+minibufferoffset+digits_time_index);
//	cout<<"non-zero section of trace (from sample "<<std::max(0,pulsepeakindex-10)
//		<<" to "<<std::min(full_buffer_size,pulsepeakindex+100)
//		<<") : {";
//	for(int samplei=std::max(0,pulsepeakindex-10);
//			samplei<std::min(minibuffer_datapoints_per_channel,pulsepeakindex+100);
//			samplei++){
//		cout<<"("<<samplei<<","<<(*(minibuffer_start+samplei))<<"), ";
//	}
//	cout<<"}"<<endl;
	
//	// ==============
//	// draw for debug
//	if(DRAW_DEBUG_PLOTS){
//		cout<<"making graph of this channel's buffer of size "<<(full_buffer_size/channels_per_adc_card)<<endl;
//		// make the graph+canvas if we haven't
//		if(gROOT->FindObject("buffercanvas")==nullptr){
//			buffercanvas = new TCanvas("buffercanvas","buffercanvas",canvwidth,canvheight);
//			buffergraph  = new TGraph(full_buffer_size/channels_per_adc_card);
//			buffergraph->SetName("buffergraph");
//		}
//		cout<<"adding hit at "<<digihit->GetTime()<<"ns, index "<<digits_time_index<<endl;
//		cout<<"non-zero values of the buffer: {";
//		for(int i=0; i<(full_buffer_size/channels_per_adc_card); i++){
//			uint16_t theval = thiscards_fullbuffer->at(channeloffset+i);
//			buffergraph->SetPoint(i,i,theval);
//			if(theval!=0) cout<<"("<<i<<","<<theval<<")";
//		}
//		cout<<"}"<<endl;
//		buffercanvas->cd();
//		buffergraph->Draw("alp");
//		buffercanvas->Update();
//		//gPad->WaitPrimitive();
//		do{
//			gSystem->ProcessEvents();
//			std::this_thread::sleep_for (std::chrono::milliseconds(100));
//		} while(gROOT->FindObject("buffercanvas"));
//	}
//	// ===============
	
}

void PulseSimulation::GenerateMinibufferPulse(int digit_index, double adjusted_digit_q, std::vector<uint16_t> &pulsevector){
	// Construct a waveform representing the pulse from a single digit
	// ===============================================================
	// we need to construct a waveform which crosses a Hefty threshold at digit_index
	// (actually we position it centred at digit_index, it should be shifted according to threshold crossing)
	// and has an integral of adjusted_digit_q. A landau function has approximately the right shape.
	//Log("PulseSimulation Tool: Generating pulse waveform",v_debug,verbosity);
	
	if(fLandau==nullptr){
		fLandau = new TF1("fLandau","[2]*TMath::Landau(x,[0],[1],1)",digit_index-10,digit_index+100);
	}
	// [0] is ~position of maximum, [1] is width (sigma), [2] is a scaling.
	// the last parameter (fixed to 1 above) is 'normalise by dividing by sigma'.
	// with 1, the integral is fixed to 1 and the maximum is varied
	// (with 0 the maximum is fixed to ~0.18 and the integral varies)
	// by choosing 1 we can always get the desired integral (Q). How should we vary height vs width?
	// that's given by the typical aspect ratio of a PMT pulse: 
	// Looking at data: with X scale in samples (8ns) fitting a landau gives a sigma of ~2
	// typical digit Qs are 0-30. (PEs?)
	
	// TODO improve this by trading off the width vs height based on the time between the first and last
	// photons within the digit XXX
	
	//cout<<"making pulse with integral "<<adjusted_digit_q<<" and peak "<<digit_index<<endl;
	fLandau->SetParameters(digit_index, 2., adjusted_digit_q);
	
//	if(DRAW_DEBUG_PLOTS){
//		// make the graph+canvas if we haven't
//		if(gROOT->FindObject("landaucanvas")==nullptr){
//			landaucanvas = new TCanvas("landaucanvas","landaucanvas",canvwidth,canvheight);
//		}
//		landaucanvas->cd();
//		fLandau->Draw();
//		landaucanvas->Update();
//		gSystem->ProcessEvents();
//	}
	
	// landau function is interesting in region -5*sigma -> 50*sigma, or for sigma=2, -10 to 100
	//fLandau->SetRange(-10,100); only set in creation as fixed
	for(int i=(digit_index-10); i<(digit_index+100); i++){
		// pulses very close to the front/end of the minibuffer: get tructated.
		if( (i<0) || (i>(pulsevector.size()-1)) ) continue;
		pulsevector.at(i)=fLandau->Eval(i);
	}
	
	// ==============
//	// draw for debug
//	if(DRAW_DEBUG_PLOTS){
//		cout<<"making graph of the new pulse of size "<<pulsevector.size()<<endl;
//		// make the graph+canvas if we haven't
//		if(gROOT->FindObject("pulsecanvas")==nullptr){
//			pulsecanvas = new TCanvas("pulsecanvas","pulsecanvas",canvwidth,canvheight);
//			pulsegraph  = new TGraph(pulsevector.size());
//			pulsegraph->SetName("pulsegraph");
//		}
//		for(int i=0; i<pulsevector.size(); i++) pulsegraph->SetPoint(i,(float)i,(float)pulsevector.at(i));
//		// for validation, read back the datapoint values
//		cout<<"non-zero values of pulsevector: {";
//		for(auto&& aval : pulsevector){ if(aval!=0) cout<<aval<<", "; }
//		cout<<"}"<<endl;
//		pulsecanvas->cd();
//		pulsegraph->Draw("alp");
//		pulsecanvas->Update();
//		gSystem->ProcessEvents();
//		//std::this_thread::sleep_for (std::chrono::seconds(5));
//		//gPad->WaitPrimitive();
//	}
//	// ===============
	
}

void PulseSimulation::AddMinibufferStartTime(bool droppingremainingsubtriggers){
	// Add an array entry corresponding to a new minibuffer start to this ADC readout entry
	// ====================================================================================
	Log("PulseSimulation Tool: Adding minibuffer for this trigger",v_debug,verbosity);
	
	// we need to make the timing variables correctly line up to give the correct TSinceBeam.
	// It's probably possible to use TriggerCounts, which defines the start time of the minibuffer
	// in clock ticks, but this is not relative to the beam arrival. In fact the relationship between
	// TSinceBeam (which we have) and the timing variables in PMTData and TrigData is non-trivial.
	// For now, though, we can just skip proper simulation of remaining timing variables.
	// This is because ToolAnalysis doesn't actually read the raw timing variables, but instead
	// reads TSinceBeam from a pre-processed 'Hefty Timing file', which we can also generate.
	
	if(triggernum==0){
		// First, to replicate the beam structure and timing, we need to add some running time offsets.
		// We expect ~0.03 events per spill, so throw from a Poisson distribution with mean of 1/0.03
		// to see how many beam spills there were since the last event.
		int numspillssincelast = R.Poisson(1./0.03);
		// spills occur at an (average) rate of 7.5Hz. Add time to the running timer since last spill.
		runningeventtime += (numspillssincelast * (1./7.5) * SEC_TO_NS);
		
		// now calculate time from start of the spill to the first trigger (start of this event)
		// each spill lasts 1.6us, but is made up of 84 bunches, each separated by 19ns
		int starttimeofbunch = floor(R.Uniform(0,84)) * 19;
		// each bunch is a gaussian with width +-2.5ns
		int timewithinbunch = R.Gaus(0.,2.5);
		// this gives the time to the start of the event within the beam spill
		currenteventtime = starttimeofbunch + timewithinbunch;
	}
	
	// get start time of this minibuffer (trigger) since event start in ns
	// TODO for phase 1 data we may need to subtract the ~6670ns offset from upstream....?
	int minibuffer_start_time_ns = triggertime; // relative to the start of the simulation event
	
	// and for now put it into the TriggerCounts variable
	timefileout_TSinceBeam[minibuffer_id] = minibuffer_start_time_ns;
	
	// fill the remaining timing things, if we know what they ought to be...
	timefileout_More[minibuffer_id]=(droppingremainingsubtriggers) ? 1 : 0;
	//cout<<"setting heftyMore["<<minibuffer_id<<"]="<<timefileout_More[minibuffer_id]<<endl;
	timefileout_Label[minibuffer_id] = (triggernum==0) ? (0x1<<4) : (0x1<<24); // Beam or Window trigger
	timefileout_Time[minibuffer_id] = fileout_StartTimeSec*SEC_TO_NS           // run start time
		 + currenteventtime + minibuffer_start_time_ns; // this beam trigger offset + this minibuffer offset
	
}

void PulseSimulation::ConstructEmulatedPmtDataReadout(){
	// construct the empty data structures (vectors, arrays) needed to hold an ADC readout
	// ===================================================================================
	Log("PulseSimulation Tool: Constructing CardData entries",v_debug,verbosity);
	// fill all the non-minibuffer info and reset the minibuffers
	if(emulated_pmtdata_readout.size()<num_adc_cards){
		cout<<"constructing vector of "<<num_adc_cards<<" CardData objects"
			<<" and temp databuffer vector with "<<full_buffer_size<<" element data arrays"<<endl;
		emulated_pmtdata_readout= std::vector<CardData>(num_adc_cards);
		temporary_databuffers = std::vector< std::vector<uint16_t> >(num_adc_cards);
	}
	// Calculate the timing values
	///////////////////////////////////
	// TriggerCounts has 40 entries per readout (one per minibuffer).
	// this is unneeded since we're generating TSinceBeam separately; just use a placeholder
	std::vector<ULong64_t> theTriggerCounts(minibuffers_per_fullbuffer);
	
	/////////////////////////////
	// fill the PMTData entries for each card.
	Log("PulseSimulation Tool: Looping over cards",v_debug,verbosity);
	for(int cardi=0; cardi<num_adc_cards; cardi++){
		//Log("PulseSimulation Tool: Getting emulated traces",v_debug,verbosity);
		auto&& acard = emulated_pmtdata_readout.at(cardi);
		
		// fixed values
		// these need to be set first as they're used by CardData::Reset
		//Log("PulseSimulation Tool: Getting trace characteristics",v_debug,verbosity);
		acard.Channels = channels_per_adc_card;
		acard.TriggerNumber = minibuffers_per_fullbuffer;
		acard.FullBufferSize = full_buffer_size;                  // trigger window sizes and sampling rate
		acard.Eventsize = emulated_event_size;                    // PulseSimulation::emulated_event_size etc.
		acard.BufferSize = buffer_size;                           // calculated in utilityfuncs based on
		
		acard.Reset();                                            // set members to default (bogus) values
		acard.CardID = cardi;
		
		// mostly randomly generated timing references
		acard.StartTimeSec = fileout_StartTimeSec;                // start time of run (read from config file)
		acard.StartTimeNSec = StartTimeNSecVals.at(cardi);        // steady decreases
		acard.StartCount = StartCountVals.at(cardi);              // periodic spikes
		acard.LastSync = 0;                                       // always a mutiple of 4
		
		// randomly generated TriggerCounts
		// In each of the 16 entries with the same sequence_id (one for each card) the array of
		// TriggerCounts is roughly the same, creating a sawtooth with only small variations
		// For now though, just fill with zeroes
		std::vector<ULong64_t> TriggerCountsWithNoise(minibuffers_per_fullbuffer,0);
		acard.TriggerCounts = TriggerCountsWithNoise;
		
		// actually set per event
		//Log("PulseSimulation Tool: Allocating trace memory",v_debug,verbosity);
		acard.SequenceID = sequence_id;
		//acard.Data.assign(full_buffer_size,0); not necessary since we're using the temporary buffers
		temporary_databuffers.at(cardi).assign(full_buffer_size,0);
		
		//	Remaining event data to be filled while processing triggers:
		//	-----------------------------------------------------------
		//	TriggerCounts.assign(TriggerNumber,BOGUS_UINT64);      // start counts of each minibuffer
		//	Rates.assign(Channels,BOGUS_UINT32);                   // avg pulse rate on each PMT
		//	Data.assign(FullBufferSize,BOGUS_UINT16);              // 160k datapoints
	}
	
}

void PulseSimulation::FillEmulatedPMTData(){
	
	// PMTData tree has: one entry per VME card, 16 entries (cards) per readout, 40 minibuffers per readout.
	// A single trigger may span multiple minibuffers, but there's nothing to indicate it - just the 
	// timestamps of the starts of each minibuffer will be separated by 2us. 
	// The Data[] array in each entry concatenates all channels on that card:
	// ([Card 0: {minibuffer 0}{minibuffer 1}...{minibuffer 39}][Card 1: {minibuffer 0}...]...)
	// Entries are ordered according to the card position in the vme crate, so are consistent but not 
	// 0,1,2...	(there are 16 cards, numbered up to 21). Each readout has a unique SequenceID.
	// Data[] arrays are waveforms of 40,000 datapoints per minibuffer
	
	// first, add noise to the temporary waveforms
	AddNoiseToWaveforms();
	
	// next, shuffle your library. this also copies temporary_databuffers into emulated_pmtdata_readout
	RiffleShuffle(false); // pass true for phase 1 shuffle, needed for either
	
	//cout<<"Filling PMTData tree"<<endl;
	// loop over all cards and fill the PMTData tree with the constructed data
	Log("PulseSimulation Tool: Loading traces into TTree branch variables",v_debug,verbosity);
	for(auto& acard : emulated_pmtdata_readout){
		fileout_LastSync			= acard.LastSync;
		fileout_SequenceID			= acard.SequenceID;
		fileout_StartTimeSec		= acard.StartTimeSec;
		fileout_StartTimeNSec		= acard.StartTimeNSec;
		fileout_StartCount			= acard.StartCount;
		fileout_TriggerNumber		= acard.TriggerNumber;
		fileout_CardID				= acard.CardID;
		fileout_Channels			= acard.Channels;
		fileout_BufferSize			= acard.BufferSize;
		fileout_Eventsize			= acard.Eventsize;
		fileout_FullBufferSize		= acard.FullBufferSize;
		fileout_TriggerCounts		= acard.TriggerCounts.data();
		fileout_Rates				= acard.Rates.data();
		fileout_Data				= acard.Data.data();
		tPMTData->SetBranchAddress("Data",fileout_Data);
		// ^ without this first set of full buffers all read garbage... 
		
		// =================
		if(DRAW_DEBUG_PLOTS){
			if(full_buffer_size!=acard.Data.size()){
				cout<<"full_buffer_size="<<full_buffer_size
					<<", fileout_Data.size()="<<acard.Data.size()<<endl;
			}
			// make the graph+canvas if we haven't
			if(gROOT->FindObject("buffercanvas")==nullptr){
				buffercanvas = new TCanvas("buffercanvas","buffercanvas",canvwidth,canvheight);
				fullbuffergraph  = new TGraph(full_buffer_size/channels_per_adc_card);
				fullbuffergraph->SetName("fullbuffergraph");
			}
			for(int i=0; i<acard.Data.size(); i++){
				uint16_t theval = acard.Data.at(i);
				fullbuffergraph->SetPoint(i,i,theval);
				//if(theval!=0) cout<<"("<<i<<","<<theval<<")"<<", ";
			}
			//cout<<endl;
			buffercanvas->Clear();
			buffercanvas->cd();
			fullbuffergraph->SetTitle(TString::Format("Card %d",fileout_CardID));
			fullbuffergraph->Draw("alp");
			buffercanvas->Update();
			//std::this_thread::sleep_for (std::chrono::seconds(5));
			gPad->WaitPrimitive();
		}
		// ================
		
		//Log("PulseSimulation Tool: Adding entry to the fake file TTree",v_debug,verbosity);
		tPMTData->Fill();
	}
	
	// Fill the hefty timing file. 
	// for partial minibuffer readouts, we need to blank out the remainder of the arrays
	for(int i=minibuffer_id; i<minibuffers_per_fullbuffer; i++){
		timefileout_Time[i]=0;
		timefileout_Label[i]=0;
		timefileout_TSinceBeam[i]=0;
		timefileout_More[i]=0;
	}
	
	// debug print
	cout<<"Filling heftdb tree: More branch has values: ";
	for(int i=0; i<minibuffers_per_fullbuffer; i++){
		cout<<timefileout_More[i]<<", ";
	}
	cout<<endl;
	Log("PulseSimulation Tool: Filling heftydb file",v_debug,verbosity);
	theftydb->Fill();
	
//	// draw for debug *** NEEDS UPDATING ***s
//	int samples_per_channel = full_buffer_size/channels_per_adc_card;
//	static TGraph* buffergraph = new TGraph(samples_per_channel);
//	static TCanvas* buffercanvas = new TCanvas();
//	for(int i=0; i<samples_per_channel; i++){
//		buffergraph->SetPoint(i,(float)i,(float)fileout_Data[i]);
//	}
//	buffergraph->SetLineColor(kRed);
//	buffercanvas->cd();
//	buffergraph->Draw("alp");
//	buffercanvas->Update();
//	gPad->WaitPrimitive();
//	
//	// retrieve the version in the tree
//	std::vector<uint16_t> z(full_buffer_size,0);
//	tPMTData->SetBranchAddress("Data",z.data());
//	cout<<"retrieving Data[] from tree entry "<<(tPMTData->GetEntries()-1)<<endl;
//	tPMTData->GetEntry(tPMTData->GetEntries()-1);
//	for(int i=0; i<samples_per_channel; i++){
//		buffergraph->SetPoint(i,(float)i,(float)z.at(i));
//	}
//	buffergraph->SetLineColor(kBlue);
//	buffercanvas->cd();
//	buffergraph->Draw("alp");
//	buffercanvas->Update();
//	gPad->WaitPrimitive();
//	tPMTData->SetBranchAddress("Data",fileout_Data);
//	z.clear();
	
}

void PulseSimulation::AddNoiseToWaveforms(){
	Log("PulseSimulation Tool: Adding noise to traces",v_debug,verbosity);
	// add noise to the temporary buffers
	for(int cardi=0; cardi<num_adc_cards; cardi++){
		// get the temporary Data array for this card, which currently only has pulses in
		auto& temp_card_buffer = temporary_databuffers.at(cardi);
		
		// loop over all the minibuffers in the full buffer
		int16_t mboffset=0;
		for(int samplei=0; samplei<temp_card_buffer.size(); samplei++){
			// Each minibuffer has an offset of ~330 +- 20 ADC counts, distributed... well..
			// there may be an underlying sine wave of varying amplitude, which is sorta kinda uniform..?
			if((samplei%(full_buffer_size/minibuffers_per_fullbuffer))==0){
				// if new minibuffer, generate a new minibuffer offset
				mboffset = static_cast<uint16_t>(R.Uniform(310,350));
			}
			
			// within the minibuffer there's a spread of gaussian noise +-5 ADC counts
			int16_t currentsamplei = static_cast<int32_t>(temp_card_buffer.at(samplei));
			currentsamplei += mboffset + static_cast<int32_t>(R.Gaus(0,2));
			temp_card_buffer.at(samplei) = static_cast<uint32_t>(currentsamplei);
		}
	}
}

void PulseSimulation::RiffleShuffle(bool do_shuffle){
	Log("PulseSimulation Tool: Performing phase 1 interleaving",v_debug,verbosity);
/*
	4 channels are separate and isolated within the full buffer:
	[chan 1][chan 2][chan 3][chan 4]
	within each channel, there are 40 minibuffers concatenated:
	[{ch1:mb1}{ch1:mb2}...{ch1:mb40}][{ch2:mb1}{ch2:mb2}...{ch2:mb40}] --- [{ch4:mb1}{ch4:mb2}...{ch4:mb40}]
	but wait! within each channel section, each subarray is split in half and pairs are interleaved:
	so if a channel subarray had indices:
	[0, 1, 2, 3, 4 ... 40k]
	then the actual sample ordering should be:
	[0, 1, 4, 5, 8, 9 ... 20k, 2, 3, 6, 7, 10, 11 ... 40k]
	
	So let's shuffle!
*/
	//cout<<"Shuffling Data arrays"<<endl;
	
	int channel_buffer_size = (full_buffer_size / channels_per_adc_card);
	for(int cardi=0; cardi<num_adc_cards; cardi++){
		auto& acard = emulated_pmtdata_readout.at(cardi);
		auto& card_buffer = acard.Data;
		auto& temp_card_buffer = temporary_databuffers.at(cardi);
		
		if(do_shuffle){
			// split the deck into four piles
			for(int channeli=0; channeli<channels_per_adc_card; channeli++){
				auto channel_buffer_start = card_buffer.begin() + (channel_buffer_size * channeli);
				auto temp_channel_buffer_start = temp_card_buffer.begin() + (channel_buffer_size * channeli);
				// split each pile into two, then interleave pairs in the two piles
				for(int samplei=0, samplej=0; samplei<channel_buffer_size; samplei+=4, samplej+=2){
					//0 = 0
					*(channel_buffer_start + samplej) = 
						*(temp_channel_buffer_start + samplei);
					//1 = 1
					*(channel_buffer_start + samplej + 1) = 
						*(temp_channel_buffer_start + samplei + 1);
					//20,000 = 2
					*(channel_buffer_start + (channel_buffer_size/2) + samplej ) 
						= *(temp_channel_buffer_start + samplei + 2);
					//20,001 = 3
					*(channel_buffer_start + (channel_buffer_size/2) + samplej + 1) 
						= *(temp_channel_buffer_start + samplei + 3);
				}
			}
		} else {
			for(int i=0; i<temp_card_buffer.size(); i++) card_buffer.at(i)=temp_card_buffer.at(i);
		}
	}
	// finally, ask a player to cut the deck
}