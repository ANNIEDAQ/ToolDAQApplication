/* vim:set noexpandtab tabstop=4 wrap */

// CREATE+OPEN RAW FORMAT OUTPUT FILE
// ==================================
void PulseSimulation::LoadOutputFiles(){
	
	Log("PulseSimulation Tool: Constructing output files",v_debug,verbosity);
	// Use the name of the simulation file to generate the name of the fake raw file
	std::string wcsimfilename;
	get_ok = m_data->Stores.at("ANNIEEvent")->Get("MCFile", wcsimfilename); // wcsim_0.AAA.B.root
	
	// use regexp to pull out the file numbers - we'll call this 'run' and 'subrun'
	std::match_results<string::const_iterator> submatches;
	// filename is of the form "wcsim_0.AAA.B.root"
	//std::regex theexpression (".*/[^0-9]+\\.([0-9]+)\\.([0-9]+)\\.root");
	//std::regex theexpression (".*/?[^\\.]+\\.([0-9]+)\\.?([0-9]+)?\\.root");
	std::regex theexpression (".*/?[^\\.]+\\.([0-9]+)\\.?([0-9]+)?\\.?([0-9]+)?\\.root");
	Log("PulseSimulation Tool: Matching regex for filename "+wcsimfilename,v_message,verbosity);
	std::regex_match (wcsimfilename, submatches, theexpression);
	std::string submatch = (std::string)submatches[0]; // match 0 is whole match
	int rawfilerun, rawfilesubrun, rawfilepart;
	if(submatch==""){ 
		cerr<<"unrecognised input file pattern: "<<wcsimfilename
			<<", will set rawfilerun=0, rawfilesubrun=0, rawfilepart=0"<<endl;
		//return;
		rawfilerun=0;
		rawfilesubrun=0;
		rawfilepart = 0;
	} else {
		submatch = (std::string)submatches[1];
		//cout<<"extracted submatch is "<<submatch<<endl;
		rawfilerun = atoi(submatch.c_str());
		submatch = (std::string)submatches[2];
		rawfilesubrun = atoi(submatch.c_str());
		submatch = (std::string)submatches[3];
		rawfilepart = atoi(submatch.c_str());
	}
	
	std::string rawfilename="RAWDataR"+to_string(rawfilerun)+
		"S"+to_string(rawfilesubrun)+"p"+to_string(rawfilepart)+".root";
	
	// Get the necessary config information on buffer sizes etc
	m_data->CStore.Get("WCSimPreTriggerWindow",pre_trigger_window_ns);
	m_data->CStore.Get("WCSimPostTriggerWindow",post_trigger_window_ns);
	minibuffer_datapoints_per_channel = (abs(pre_trigger_window_ns)+post_trigger_window_ns) / ADC_NS_PER_SAMPLE;
	buffer_size = minibuffer_datapoints_per_channel * minibuffers_per_fullbuffer;
	full_buffer_size = buffer_size * channels_per_adc_card;
	emulated_event_size = minibuffer_datapoints_per_channel / 4.; // the 4 comes from ADC firmware stuff
//	cout<<"pretriggerwindow="<<pre_trigger_window_ns
//		<<", posttriggerwindow="<<post_trigger_window_ns<<endl;
//	cout<<"minibuffer_datapoints_per_channel="<<minibuffer_datapoints_per_channel<<endl;
//	cout<<"buffersize="<<buffer_size<<endl;
//	cout<<"fullbuffersize="<<full_buffer_size<<endl;
//	cout<<"minibuffers_per_fullbuffer="<<minibuffers_per_fullbuffer<<endl;
	
	// Create the file, TTrees, branches etc
	Log("PulseSimulation Tool: Creating fake output file " + rawfilename,v_debug,verbosity);
	rawfileout = new TFile(rawfilename.c_str(),"RECREATE");
	//*----------------------------------------------------------------------------*
	tPMTData                 = new TTree("PMTData","");                       // one entry per trigger readout
	fileout_TriggerCounts    = new ULong64_t[minibuffers_per_fullbuffer];     // must be >= TriggerNumber
	fileout_Rates            = new UInt_t[channels_per_adc_card];             // must be >= Channels
	fileout_Data             = new UShort_t[full_buffer_size];                // must be >= FullBufferSize
	//*............................................................................*
	TBranch *bLastSync       = tPMTData->Branch("LastSync", &fileout_LastSync);
	TBranch *bSequenceID     = tPMTData->Branch("SequenceID", &fileout_SequenceID);
	TBranch *bStartTimeSec   = tPMTData->Branch("StartTimeSec", &fileout_StartTimeSec);
	TBranch *bStartTimeNSec  = tPMTData->Branch("StartTimeNSec", &fileout_StartTimeNSec);
	TBranch *bStartCount     = tPMTData->Branch("StartCount", &fileout_StartCount);
	TBranch *bTriggerNumber  = tPMTData->Branch("TriggerNumber", &fileout_TriggerNumber);
	TBranch *bTriggerCounts  = tPMTData->Branch("TriggerCounts", &fileout_TriggerCounts,
		"TriggerCounts[TriggerNumber]/l");
	TBranch *bCardID         = tPMTData->Branch("CardID", &fileout_CardID);
	TBranch *bChannels       = tPMTData->Branch("Channels", &fileout_Channels);
	TBranch *bRates          = tPMTData->Branch("Rates", &fileout_Rates, 
		"Rates[Channels]/i");
	TBranch *bBufferSize     = tPMTData->Branch("BufferSize", &fileout_BufferSize);
	TBranch *bEventsize      = tPMTData->Branch("Eventsize", &fileout_Eventsize);
	TBranch *bFullBufferSize = tPMTData->Branch("FullBufferSize", &fileout_FullBufferSize);
	TBranch *bData           = tPMTData->Branch("Data", &fileout_Data, "Data[FullBufferSize]/s");
	//*----------------------------------------------------------------------------*
	tRunInformation = new TTree("RunInformation","");
	//*............................................................................*
	TBranch *bInfoTitle = tRunInformation->Branch("InfoTitle", &fileout_InfoTitle);
	TBranch *bInfoMessage = tRunInformation->Branch("InfoMessage", &fileout_InfoMessage);
	
	//*----------------------------------------------------------------------------*
	// Since we can't properly synthesize all the timing variables
	// we'll directly create the simplified output Hefty Timing file
	std::string timingfilename="DataR"+to_string(rawfilerun)+
		"S"+to_string(rawfilesubrun)+"p"+to_string(rawfilepart)+"_timing.root";
	Log("PulseSimulation Tool: Creating RAW timing file "+timingfilename,v_warning,verbosity);
	timingfileout = new TFile(timingfilename.c_str(),"RECREATE");
	timingfileout->cd();
	//*----------------------------------------------------------------------------*
	theftydb                  = new TTree("heftydb","");
	timefileout_Time          = new ULong_t[minibuffers_per_fullbuffer];
	timefileout_Label         = new Int_t[minibuffers_per_fullbuffer];
	timefileout_TSinceBeam    = new Long_t[minibuffers_per_fullbuffer];
	timefileout_More          = new Int_t[minibuffers_per_fullbuffer];
	//*----------------------------------------------------------------------------*
	theftydb->Branch("SequenceID", &fileout_SequenceID);
	theftydb->Branch("Time", &timefileout_Time, TString::Format("Time[%d]/l",minibuffers_per_fullbuffer));
	theftydb->Branch("Label", &timefileout_Label, TString::Format("Label[%d]/I",minibuffers_per_fullbuffer));
	theftydb->Branch("TSinceBeam", &timefileout_TSinceBeam,
		TString::Format("TSinceBeam[%d]/L",minibuffers_per_fullbuffer));
	theftydb->Branch("More", &timefileout_More, TString::Format("More[%d]/I",minibuffers_per_fullbuffer));
	
	gROOT->cd();
	
	FillInitialFileInfo();
	
}

void PulseSimulation::FillInitialFileInfo(){
	// We can generate some of the output values immediately, as they're just dummy values
	//===============================
	Log("PulseSimulation Tool: Filling run start date and setting constants",v_debug,verbosity);
	FillEmulatedRunInformation();
	
	// StartTimeSec represents the unix seconds of the start of the run
	// Read "run date" from the options file and convert to unixns
	// first convert config file string to parts
	std::string startDate;
	get_ok = m_variables.Get("RunStartDate",startDate);
	
	int hh, mm, ss, MM, DD, YYYY;
	sscanf(startDate.c_str(), "%d/%d/%d %d:%d:%d", &DD, &MM, &YYYY, &hh, &mm, &ss);
	// combine parts into a time structure
	struct std::tm runstart;
	runstart.tm_year = YYYY;
	runstart.tm_mon = MM;
	runstart.tm_mday = DD;
	runstart.tm_hour = hh;
	runstart.tm_min = mm;
	runstart.tm_sec = ss;
	// finally convert time structure to unix seconds
	time_t runstarttime = mktime(&runstart);
	// use runstarttime = time(NULL);  to get current time.
	fileout_StartTimeSec = static_cast<Int_t>(runstarttime);
	
	// give proper sizes to the vectors of unused timing variables
	StartCountVals.assign(num_adc_cards,0);
	StartTimeNSecVals.assign(num_adc_cards,0);
	
}

void PulseSimulation::FillEmulatedRunInformation(){
	Log("PulseSimulation Tool: Filling the template Run Information tree",v_debug,verbosity);
	// RunInformation tree in RAW file is essentially a map of strings.
	// It stores (at present) 11 entries, each with a 'key' in the InfoTitle branch, 
	// and a corresponding xml object string in the InfoMessage branch - 
	// the xml string may represent multiple variables.
	// Since this is largely irrelevant, we'll just use template values and modify if necessary.
	
	std::vector<std::string> InfoTitles{"ToolChainVariables","InputVariables","PostgresVariables",
	"SlackBotVariables","HVComs","TriggerVariables","NetworkReceiveDataVariables","LoggerVariables",
	"RootDataRecorderVariables","MonitoringVariables","MRDVariables"};
	std::vector<std::string>* TemplateInfoMessages = GetTemplateRunInfo();
	
	for(int entryi=0; entryi<InfoTitles.size(); entryi++){
		fileout_InfoTitle=InfoTitles.at(entryi);
		fileout_InfoMessage=TemplateInfoMessages->at(entryi);
		tRunInformation->Fill();
	}
}