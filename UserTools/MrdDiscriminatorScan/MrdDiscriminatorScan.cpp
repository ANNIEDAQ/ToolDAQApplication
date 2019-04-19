/* vim:set noexpandtab tabstop=4 wrap */
#include "MrdDiscriminatorScan.h"

#include <boost/algorithm/string.hpp>   // for trim
#include <sys/types.h> // for stat() test to see if file or folder
#include <sys/stat.h>
#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TMultiGraph.h"
#include "TLegend.h"
#include "TString.h"
#include "TApplication.h"


MrdDiscriminatorScan::MrdDiscriminatorScan():Tool(){}

bool MrdDiscriminatorScan::Initialise(std::string configfile, DataModel &data){
	
	/////////////////// Useful header ///////////////////////
	if(configfile!="") m_variables.Initialise(configfile); //loading config file
	//m_variables.Print();
	
	m_data= &data; //assigning transient data pointer
	/////////////////////////////////////////////////////////////////
	
	// Get the Tool configuration variables
	// ====================================
	m_variables.Get("verbosity",verbosity);
	m_variables.Get("plotDirectory",plotDirectory);
	m_variables.Get("drawHistos",drawHistos);
	m_variables.Get("filelist",filelist);
	m_variables.Get("filedir",filedir);
	
	TimeClass atime(1555684669048000000);
	atime.Print();
	
	// check the output directory exists and is suitable
	Log("MrdDiscriminatorScan Tool: Checking output directory "+plotDirectory,v_message,verbosity);
	bool isdir=false, plotDirectoryExists=false;
	struct stat s;
	if(stat(plotDirectory.c_str(),&s)==0){
		plotDirectoryExists=true;
		if(s.st_mode & S_IFDIR){        // mask to extract if it's a directory
			isdir=true;                   //it's a directory
		} else if(s.st_mode & S_IFREG){ // mask to check if it's a file
			isdir=false;                  //it's a file
		} else {
			isdir=false;                  // neither file nor folder??
		}
	} else {
		plotDirectoryExists=false;
		//assert(false&&"stat failed on input path! Is it valid?"); // error
		// errors could also be because this is a file pattern: e.g. wcsim_0.4*.root
		isdir=false;
	}
	
	if(!plotDirectoryExists || !isdir){
		Log("MrdDiscriminatorScan Tool: output directory "+plotDirectory+" does not exist or is not a writable directory; please check and re-run.",v_error,verbosity);
		return false;
	} else {
		Log("MrdDiscriminatorScan Tool: output directory OK",v_debug,verbosity);
	}
	
	// If we wish to show the histograms during running, we need a TApplication
	// There may only be one TApplication, so see if another tool has already made one
	// and register ourself as a user if so. Otherwise, make one and put a pointer in the
	// CStore for other Tools
	if(drawHistos){
		Log("MrdDiscriminatorScan Tool: Checking TApplication",v_debug,verbosity);
		// create the ROOT application to show histograms
		int myargc=0;
		//char *myargv[] = {(const char*)"mrdeff"};
		// get or make the TApplication
		intptr_t tapp_ptr=0;
		get_ok = m_data->CStore.Get("RootTApplication",tapp_ptr);
		if(not get_ok){
			Log("MrdDiscriminatorScan Tool: Making global TApplication",v_error,verbosity);
			rootTApp = new TApplication("rootTApp",&myargc,0);
			tapp_ptr = reinterpret_cast<intptr_t>(rootTApp);
			m_data->CStore.Set("RootTApplication",tapp_ptr);
		} else {
			Log("MrdDiscriminatorScan Tool: Retrieving global TApplication",v_error,verbosity);
			rootTApp = reinterpret_cast<TApplication*>(tapp_ptr);
		}
		int tapplicationusers;
		get_ok = m_data->CStore.Get("RootTApplicationUsers",tapplicationusers);
		if(not get_ok) tapplicationusers=1;
		else tapplicationusers++;
		m_data->CStore.Set("RootTApplicationUsers",tapplicationusers);
	}
	
	// try to get the list of files
	Log("MrdDiscriminatorScan Tool: Opening list of files to process: "+filelist,v_message,verbosity);
	fin.open(filelist.c_str());
	if(not fin.is_open()){
		Log("MrdDiscriminatorScan Tool: could not open file list "+filelist,v_error,verbosity);
		return false;
	}
	
	// scan through the file list for the next file name
	filei=0;
	Log("MrdDiscriminatorScan Tool: Getting first file name",v_debug,verbosity);
	get_ok = GetNextFile();
	if(not get_ok){
		Log("MrdDiscriminatorScan Tool: No files found in file list!",v_error,verbosity);
		return false;
	}
	
	Log("MrdDiscriminatorScan Tool: End of Initialise",v_debug,verbosity);
	return true;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

bool MrdDiscriminatorScan::Execute(){
	
	Log("MrdDiscriminatorScan Tool: Executing",v_debug,verbosity);
	
	// Open the next file
	// ==================
	BoostStore* indata=new BoostStore(false,0);
	Log("MrdDiscriminatorScan Tool: Initializing BoostStore from file "+filepath,v_message,verbosity);
	indata->Initialise(filepath);
	indata->Print(false);
	
	BoostStore* MRDData= new BoostStore(true,2);
	Log("MrdDiscriminatorScan Tool: Getting CCData Multi-Entry BoostStore from loaded file",v_debug,verbosity);
	get_ok = indata->Get("CCData",*MRDData);
	if(not get_ok){
		Log("MrdDiscriminatorScan Tool: No CCData in file BoostStore!",v_error,verbosity);
		return false;
	}
	
	// Get the pulse rates for each channel
	// ====================================
	// we'll need to count how many hits we have on each channel in this file
	// make a set of maps from crate:card:channel to number of hits
	std::map<int,std::map<int,std::map<int,int>>> hit_counts_on_channels;
	TimeClass first_timestamp, last_timestamp;
	Log("MrdDiscriminatorScan Tool: Scanning hits in this file",v_debug,verbosity);
	CountChannelHits(MRDData, hit_counts_on_channels, first_timestamp, last_timestamp);
	double total_run_seconds = last_timestamp.GetNs() - first_timestamp.GetNs();
	Log("MrdDiscriminatorScan Tool: Total duration of this run was: "
		+std::to_string(total_run_seconds),v_debug,verbosity);
	
	// Loop over the TGraphs and set the next datapoint
	current_threshold = filei+15;  // in mV... probably
	Log("MrdDiscriminatorScan Tool: Converting hit counts to rates",v_debug,verbosity);
	for( auto&& acrate : hit_counts_on_channels){
		int cratei=acrate.first;
		cout<<"crate "<<cratei<<endl;
		for(auto&& acard : acrate.second){
			int sloti=acard.first;
			cout<<"	slot "<<sloti<<endl;
			for(auto&& achannel : acard.second){
				// Add the rate for this channel to the TGraph of rate vs threshold for this card
				int channeli=achannel.first;
				int num_hits_on_this_channel = achannel.second;
				cout<<"		channel "<<channeli<<" : "<<num_hits_on_this_channel<<endl;
				double hitrate = double(num_hits_on_this_channel) / total_run_seconds;
				
				// check if this graph key exists and make it if not
				if(rategraphs.count(cratei)==0){
					rategraphs.emplace(cratei,std::map<int,std::map<int,TGraphErrors*>>{});
				}
				if(rategraphs.at(cratei).count(sloti)==0){
					rategraphs.at(cratei).emplace(sloti,std::map<int,TGraphErrors*>{});
				}
				if(rategraphs.at(cratei).at(sloti).count(channeli)==0){
					TGraphErrors* thisgraph = new TGraphErrors();
					//thisgraph->SetName(std::to_string(cratei)+"_"+std::to_string(sloti)+"_"+std::to_string(channeli));
					thisgraph->SetName(std::to_string(channeli).c_str());
					rategraphs.at(cratei).at(sloti).emplace(channeli,thisgraph);
				}
				
				// set the next datapoint on the tgraph for this channel
				logmessage = "MrdDiscriminatorScan Tool: Setting rate for "
					+to_string(cratei)+"_"+to_string(sloti)+"_"+to_string(channeli)
					+" to "+to_string(hitrate);
				Log(logmessage,v_debug,verbosity);
				TGraphErrors* thisgraph = rategraphs.at(cratei).at(sloti).at(channeli);
				if(thisgraph==nullptr){
					Log("MrdDiscriminatorScan Tool: Null TGraph pointer setting datapoint!?",v_error,verbosity);
				} else {
					thisgraph->SetPoint(filei, current_threshold, hitrate);  // XXX add errors
				}
			}
			Log("MrdDiscriminatorScan Tool: Looping to next slot",v_debug,verbosity);
		}
		Log("MrdDiscriminatorScan Tool: Looping to next crate",v_debug,verbosity);
	}
	Log("MrdDiscriminatorScan Tool: Done setting rates for this threshold",v_debug,verbosity);
	
	// Load the next file
	// ==================
	// Do this at the end of the loop so that we can break the loop now if there are no more files
	Log("MrdDiscriminatorScan Tool: Getting next file name",v_debug,verbosity);
	get_ok = GetNextFile();
	if(not get_ok){
		Log("MrdDiscriminatorScan Tool: reached end of file list.",v_error,verbosity);
		fin.close();
		m_data->vars.Set("StopLoop",1);
	} else {
		Log("MrdDiscriminatorScan Tool: Next file will be: "+filepath,v_message,verbosity);
		filei++;
	}
	
	// Cleanup??
	// ========
	Log("MrdDiscriminatorScan Tool: deleting indata and MRDData...",v_debug,verbosity);
	delete indata;
	delete MRDData;
	
	Log("MrdDiscriminatorScan Tool: Execute complete",v_debug,verbosity);
	return true;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

bool MrdDiscriminatorScan::Finalise(){
	
	Log("MrdDiscriminatorScan Tool: Processed "+std::to_string(filei)+" files",v_message,verbosity);
	
	// make a canvas
	int canvwidth = 700;
	int canvheight = 600;
	mrdScanCanv = new TCanvas("mrdScanCanv","",canvwidth,canvheight);
	mrdScanCanv->cd();
	
	// loop over crates
	Log("MrdDiscriminatorScan Tool: Drawing TGraphs",v_debug,verbosity);
	for(auto&& acrate : rategraphs){
		int cratei = acrate.first;
		cout<<"crate : "<<cratei<<endl;
		// loop over card in this crate
		for(auto&& aslot : acrate.second){
			int sloti = aslot.first;
			cout<<"	slot : "<<sloti<<endl;
			// clear the canvas (just in case), we'll plot one tdc per graph
			mrdScanCanv->Clear();
			TMultiGraph* allgraphsforthiscard = new TMultiGraph();
			for(auto&& achannel : aslot.second){
				int channeli=achannel.first;
				cout<<"		channel : "<<channeli<<endl;
				TGraphErrors* thisgraph = rategraphs.at(cratei).at(sloti).at(channeli);
				
				//gStyle->SetPalette(colorPalette);
				//int nColors = gStyle->GetNumberOfColors();
				//int nHistos = histos.size();
				//for (size_t i = 0; i < nHistos; ++i) {
				//	int histoColor = (float)nColors / nHistos * i;
				//	histos[i]->SetLineColor(gStyle->GetColorPalette(histoColor));
				
				//thisgraph->SetLineColor(??????);
				// option "same PLC PMC" will automatically pick unique colors for multiple TH1s
				// or for THStack just "PFC nostack" when drawing the stack
				// or for TMultiGraph, pass "PLC" when adding *and* "PMC PLC" when drawing TMultiGraph
				if(thisgraph==nullptr){
					logmessage = "MrdDiscriminatorScan Tool: No TGraph to draw for crate " + to_string(cratei)
					+ ", slot " + to_string(sloti) + ", channel " + to_string(channeli);
					Log(logmessage,v_error,verbosity);
				} else {
					allgraphsforthiscard->Add(thisgraph,"PL");
				}
			}
			Log("MrdDiscriminatorScan Tool: Drawing TMultiGraph for crate "+to_string(cratei)
				+", slot "+to_string(sloti),v_debug,verbosity);
			allgraphsforthiscard->Draw("A pmc plc");
			TLegend* theleg = mrdScanCanv->BuildLegend();  // XXX position and resize
			Log("MrdDiscriminatorScan Tool: Saving TMultiGraph",v_debug,verbosity);
			mrdScanCanv->SaveAs(TString::Format("Crate_%i_TDC_%i_Threshold_%imV.C",cratei, sloti, current_threshold));
			mrdScanCanv->SaveAs(TString::Format("Crate_%i_TDC_%i_Threshold_%imV.png",cratei, sloti, current_threshold));
			Log("MrdDiscriminatorScan Tool: graph cleanup",v_debug,verbosity);
			delete theleg; theleg=nullptr;
			delete allgraphsforthiscard; allgraphsforthiscard=nullptr;
			// the TMultiGraph owns it's contents: do not delete them individually
			Log("MrdDiscriminatorScan Tool: Looping to next slot",v_debug,verbosity);
		}
		Log("MrdDiscriminatorScan Tool: Looping to next crate",v_debug,verbosity);
	}
	
	
	// Cleanup
	// =======
	Log("MrdDiscriminatorScan Tool: Canvas cleanup",v_debug,verbosity);
	delete mrdScanCanv; mrdScanCanv=nullptr;
	
	Log("MrdDiscriminatorScan Tool: TApplication cleanup",v_debug,verbosity);
	// Cleanup TApplication if we have used it and we are the last Tool
	if(drawHistos){
		int tapplicationusers=0;
		get_ok = m_data->CStore.Get("RootTApplicationUsers",tapplicationusers);
		if(not get_ok || tapplicationusers==1){
			if(rootTApp){
				Log("MrdDiscriminatorScan Tool: deleting gloabl TApplication",v_debug,verbosity);
				delete rootTApp;
			}
		} else if (tapplicationusers>1){
			tapplicationusers--;
			m_data->CStore.Set("RootTApplicationUsers",tapplicationusers);
		}
	}
	
	Log("MrdDiscriminatorScan Tool: Finalise done",v_debug,verbosity);
	return true;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

bool MrdDiscriminatorScan::GetNextFile(){
	std::string filename="";
	bool got_file=false;
	while(getline(fin, filename)){
		boost::trim(filename);             // trim any surrounding whitespace
		if(filename.empty()){
			continue;
		} else if (filename[0] == '#'){
			continue;
		} else {
			Log("MrdDiscriminatorScan Tool: Next file will be "+filename,v_message,verbosity);
			got_file=true;
			break;
		}
	}
	if(not got_file) return false;
	filepath = filedir + "/" + filename;
	return true;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void MrdDiscriminatorScan::CountChannelHits(BoostStore* MRDData, std::map<int,std::map<int,std::map<int,int>>> &hit_counts_on_channels, TimeClass& first_timestamp, TimeClass& last_timestamp){
	
	// we should have 10k events per threshold
	int total_number_entries;
	MRDData->Header->Get("TotalEntries",total_number_entries);
	logmessage = "MrdDiscriminatorScan Tool: file " + to_string(filei)
				  +" has " + std::to_string(total_number_entries) + " readouts";
	Log(logmessage,v_debug,verbosity);
	
	// loop over the readouts
	for (int readouti = 0; readouti <  total_number_entries; readouti++){
		
		MRDData->GetEntry(readouti);       // MRDData is a multi-entry BoostStore: load the next event
		MRDOut mrdReadout;                 // class defined in the DataModel to contain an MRD readout
		MRDData->Get("Data",mrdReadout);   // get the readout
		
		// each mrdReadout has a trigger number, a readout number (usually the same), a trigger timestamp,
		// and then vectors of crate, slot, channel, tdc value and a type string, one entry for each hit
		ULong64_t timestamp = mrdReadout.TimeStamp;
		TimeClass timestampclass(timestamp);
		if(readouti==0) first_timestamp = timestamp;
		if(readouti==(total_number_entries-1)) last_timestamp = timestamp;
		
		// we don't actually care about the times of the hits, we just want to count how many there were
		// so just for interest
		if(readouti == 0){
			logmessage = "First entry of next file had timestamp: " + std::to_string(timestamp)
						+ "(" + timestampclass.AsString() + ").\n There were " 
			 			+ to_string(mrdReadout.Channel.size())+" hits in this readout";
			Log(logmessage,v_debug,verbosity);
			
			//printing intormation about the event XXX REMOVE later
			std::cout <<"------------------------------------------------------------"<<std::endl;
			std::cout <<"readouti: "<<readouti<<", TimeStamp: "<<timestamp<<std::endl;
			std::cout <<"Slot size: "<<mrdReadout.Slot.size()<<", Crate size: "<<mrdReadout.Crate.size()
					  <<", Channel size: "<<mrdReadout.Channel.size()<<std::endl
					  <<"OutN: "<<mrdReadout.OutN<<", Trigger: "<<mrdReadout.Trigger
					  <<", Type size: "<<mrdReadout.Type.size()<<std::endl;
		}
		
		// loop over all hits in this readout
		for (int hiti = 0; hiti < mrdReadout.Slot.size(); hiti++){
			
			// get the channel info
			int crate_num = mrdReadout.Crate.at(hiti);
			int slot_num = mrdReadout.Slot.at(hiti);
			int channel_num = mrdReadout.Channel.at(hiti);
			// n.b. crate 8 slot 9 channel ? is the trigger? XXX
			
			// increment the map, creating the necessary keys first if necessary
			if(hit_counts_on_channels.count(crate_num)==0){
				hit_counts_on_channels.emplace(crate_num,std::map<int,std::map<int,int>>{});
			}
			if(hit_counts_on_channels.at(crate_num).count(slot_num)==0){
				hit_counts_on_channels.at(crate_num).emplace(slot_num,std::map<int,int>{});
			}
			if(hit_counts_on_channels.at(crate_num).at(slot_num).count(channel_num)==0){
				hit_counts_on_channels.at(crate_num).at(slot_num).emplace(channel_num,0);
			}
			hit_counts_on_channels.at(crate_num).at(slot_num).at(channel_num)++;
			
//			// XXX all the following is unneeded, merely for demonstration XXX
//			unsigned int hit_time_ticks = mrdReadout.Value.at(hiti);
//			// combine the crate and slot number to a unique card index
//			int tdc_index = slot_num+(crate_num-min_crate)*100;
//			// check if this TDC contains any currently channels - 
//			// see if this tdc index is in our list of active TDC cards supplied by config file
//			std::vector<int>::iterator it = std::find(nr_slot.begin(), nr_slot.end(), tdc_index);
//			if (it == nr_slot.end()){
//				std::cout <<"Read-out Crate/Slot/Channel number not active according to configuration file."
//									<<" Check the configfile to process the data..."<<std::endl
//									<<"Crate: "<<crate_num<<", Slot: "<<slot_num<<std::endl;
//				continue;
//			}
//			// get the reduced tdc number (the n'th active TDC)
//			int tdc_num = std::distance(nr_slot.begin(),it);
//			// get a reduced channel number (the n'th active channel)
//			int abs_channel = tdc_num*num_channels+channel_num;
			
		}  // end of loop over hits this readout
		
		//clear mrdReadout vectors afterwards.... do we need to do this? XXX
		mrdReadout.Value.clear();
		mrdReadout.Slot.clear();
		mrdReadout.Channel.clear();
		mrdReadout.Crate.clear();
		mrdReadout.Type.clear();
	}  // loop to next readout
	
	return;
}