/* vim:set noexpandtab tabstop=4 wrap */
// #######################################################################

// called in DoMRDdigitHits() loop over MRD digits.
void PulseSimulation::AddCCDataEntry(Hit* digihit){
	//WCSimRootChernkovDigiHit has methods GetTubeId(), GetT(), GetQ()
	
	int digits_time_ns = digihit->GetTime() + pre_trigger_window_ns - HistoricTriggeroffset;
	// MRD TDC common stop timout: 85 x 50ns 
	if(digits_time_ns>MRD_TIMEOUT_NS) return;
	
	// TODO read this mapping from a file
	// remember tubeids start from 1
	int channelnum = (digihit->GetTubeId()-1)%channels_per_tdc_card;
	int cardid = ((digihit->GetTubeId()-1)-channelnum)/channels_per_tdc_card;
	
	// convert to 'value' = number of TDC ticks
	int digits_time_ticks = digits_time_ns / TDC_NS_PER_SAMPLE;
	fileout_Value.push_back(digits_time_ticks);
	fileout_Slot.push_back(cardid);
	fileout_Channel.push_back(channelnum);
}

void PulseSimulation::FillEmulatedCCData(){
	// called in post mrd hit loop
	// the following are all per-readout (per trigger).
	if(fileout_Value.size()==0){
		Log("PulseSimulation Tool: No CCData to fill, skipping",v_debug,verbosity);
		return; // no hits, no entries.
	} else {
		Log("PulseSimulation Tool: Filling CCData tree with "+to_string(fileout_Value.size())
				+" MRD hits",v_debug,verbosity);
	}
	
	// get event info from the ANNIEEvent
	uint32_t eventnum;
	m_data->Stores.at("ANNIEEvent")->Get("EventNumber",eventnum);
	TimeClass* EventTime=nullptr;
	m_data->Stores.at("ANNIEEvent")->Get("EventTime",EventTime);
	
	// Timestamp is applied by the PC post-readout so is actually delayed from the trigger!
	unsigned long long timestamp_ms = (
		static_cast<unsigned long long>(
		fileout_StartTimeSec*SEC_TO_NS +                     // run start time
		currenteventtime +                                   // ns from Run start to this event start
		EventTime->GetNs() +                                 // trigger ns since event start
		MRD_TIMESTAMP_DELAY                                  // delay between trigger card and mrd PC
	) / 1000000.);                                           // NS TO MS
	
	fileout_Trigger   = eventnum;                            // TDC readout number
	fileout_TimeStamp = timestamp_ms;                        // UTC MS since unix epoch
	fileout_OutNumber = fileout_Value.size();                // number of hits in this event
	fileout_Type.assign(fileout_OutNumber,"TDC");            // all cards are TDC cards for now.
	tCCData->Fill();
}

