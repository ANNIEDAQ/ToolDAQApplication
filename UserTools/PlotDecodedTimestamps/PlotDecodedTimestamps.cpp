#include "PlotDecodedTimestamps.h"

PlotDecodedTimestamps::PlotDecodedTimestamps():Tool(){}


bool PlotDecodedTimestamps::Initialise(std::string configfile, DataModel &data){

  /////////////////// Useful header ///////////////////////
  if(configfile!="") m_variables.Initialise(configfile); // loading config file
  //m_variables.Print();

  m_data= &data; //assigning transient data pointer
  /////////////////////////////////////////////////////////////////


  //Get configuration variables
  m_variables.Get("verbosity",verbosity);
  m_variables.Get("OutputFile",output_timestamps);
  m_variables.Get("InputDataSummary",input_datasummary);
  m_variables.Get("InputTimestamps",input_timestamps);
  m_variables.Get("SecondsPerPlot",seconds_per_plot);
  m_variables.Get("TriggerWordConfig",triggerword_config);

  //Initialise ROOT files and trees
  this->SetupFilesAndTrees();

  //Setup color scheme for triggers
  this->SetupTriggerColorScheme();


  return true;
}


bool PlotDecodedTimestamps::Execute(){

  //Get first and last timestamp of DataSummary file
  double t_first, t_last;
  for (int i_entry = 0; i_entry < entries_datasummary; i_entry++){
    t_datasummary->GetEntry(i_entry);
    if (i_entry == 0) t_first = ctctimestamp/1000000000.;
    else if (i_entry == entries_datasummary-1) t_last = ctctimestamp/1000000000.;
  }

  //Make sure that t_last>t_first
  double t_max = (t_last >= t_first)? t_last : t_first;
  double t_min = (t_last < t_first)? t_last : t_first;
  int num_snapshots = (t_max-t_min)/seconds_per_plot;

  double t_first_pmt, t_last_pmt;
  for (int i_entry = 0; i_entry < entries_timestamps_pmt; i_entry++){
    t_timestamps_pmt->GetEntry(i_entry);
    if (i_entry == 0) t_first_pmt = t_pmt/1000000000.;
    else if (i_entry == entries_timestamps_pmt-1) t_last_pmt = t_pmt/1000000000.;
  }

  //Make sure that t_last>t_first
  double t_max_pmt = (t_last_pmt >= t_first_pmt)? t_last_pmt : t_first_pmt;
  double t_min_pmt = (t_last_pmt < t_first_pmt)? t_last_pmt : t_first_pmt;

  double t_first_mrd, t_last_mrd;
  for (int i_entry = 0; i_entry < entries_timestamps_mrd; i_entry++){
    t_timestamps_mrd->GetEntry(i_entry);
    if (i_entry == 0) t_first_mrd = t_mrd/1000000000.;
    else if (i_entry == entries_timestamps_mrd-1) t_last_mrd = t_mrd/1000000000.;
  }

  //Make sure that t_last>t_first
  double t_max_mrd = (t_last_mrd >= t_first_mrd)? t_last_mrd : t_first_mrd;
  double t_min_mrd = (t_last_mrd < t_first_mrd)? t_last_mrd : t_first_mrd;

  //Create needed canvases and frames for snapshots
  for (int i_snap = 0; i_snap < num_snapshots; i_snap++){
    std::stringstream ss_canvas, ss_frame, ss_title_frame;
    ss_canvas << "canvas_timeframe"<<i_snap;
    TCanvas *c = new TCanvas(ss_canvas.str().c_str(),ss_canvas.str().c_str(),900,600);
    ss_frame << "hist_timeframe"<<i_snap;
    ss_title_frame << "Timestamps Snapshot "<<i_snap;
    TH2F *frame = new TH2F(ss_frame.str().c_str(),ss_title_frame.str().c_str(),10,t_min+(i_snap*seconds_per_plot)-0.5,t_min+(i_snap+1)*seconds_per_plot+0.5,4,0,4);
    frame->GetYaxis()->SetBinLabel(1,"All CTC");
    frame->GetYaxis()->SetBinLabel(2,"CTC");
    frame->GetYaxis()->SetBinLabel(3,"MRD");
    frame->GetYaxis()->SetBinLabel(4,"PMT");
    frame->GetXaxis()->SetTimeDisplay(1);
    frame->GetXaxis()->SetLabelSize(0.03);
    frame->GetXaxis()->SetLabelOffset(0.03);
    frame->GetXaxis()->SetTimeFormat("#splitline{%m/%d}{%H:%M:%S}");
    frame->GetYaxis()->SetTickLength(0.);
    frame->SetStats(0);
    c->cd();
    frame->Draw();
    canvas_snapshot.push_back(c);
  } 

  //Loop over DataSummary file information

  double t_first_matched, t_last_matched;
  for (int i_entry = 0; i_entry < entries_datasummary; i_entry++){
    t_datasummary->GetEntry(i_entry);
    if (i_entry==0) t_first_matched = ctctimestamp/1000000000.;
    else if (i_entry==entries_datasummary-1) t_last_matched = ctctimestamp/1000000000.;
    int index_hist = trunc((ctctimestamp/1000000000.-t_min)/seconds_per_plot);
    if (index_hist < 0 || index_hist >= num_snapshots) continue;
    int linecolor=1;
    if (triggerword == 5) linecolor=8;
    else if (triggerword == 36) linecolor=9;
    TLine *l_mrd = new TLine(mrdtimestamp/1000000000.,2.1,mrdtimestamp/1000000000.,2.9);
    l_mrd->SetLineColor(1);
    l_mrd->SetLineStyle(1);
    l_mrd->SetLineWidth(1);
    l_mrd->Draw("same");
    TLine *l_pmt = new TLine(pmttimestamp/1000000000.,3.1,pmttimestamp/1000000000.,3.9);
    l_pmt->SetLineColor(1);
    l_pmt->SetLineStyle(1);
    l_pmt->SetLineWidth(1);
    TLine *l_ctc = new TLine(ctctimestamp/1000000000.,1.1,ctctimestamp/1000000000.,1.9);
    l_ctc->SetLineColor(linecolor);
    l_ctc->SetLineStyle(1);
    l_ctc->SetLineWidth(1);
    canvas_snapshot.at(index_hist)->cd();
    l_mrd->Draw("same");
    l_pmt->Draw("same");
    l_ctc->Draw("same");
    timestamp_snapshot.push_back(l_mrd);
    timestamp_snapshot.push_back(l_pmt);
    timestamp_snapshot.push_back(l_ctc);
  }

  double t_max_matched = (t_last_matched >= t_first_matched)? t_last_matched : t_first_matched;
  double t_min_matched = (t_last_matched < t_first_matched)? t_last_matched : t_first_matched;

  //Loop over complete trigger timestamp information
  for (int i_entry = 0; i_entry < entries_timestamps; i_entry++){
    t_timestamps->GetEntry(i_entry);
    if (t_ctc/1000000000.<t_min || t_ctc/1000000000.>t_max) continue;
    int index_hist = trunc((t_ctc/1000000000.-t_min)/seconds_per_plot);
    if (index_hist < 0 || index_hist >= num_snapshots) continue;
    if (std::find(vector_triggerwords.begin(),vector_triggerwords.end(),triggerword_ctc)==vector_triggerwords.end()) continue;
    int linecolor = map_triggerword_color[triggerword_ctc];
    TLine *l_ctc = new TLine(t_ctc/1000000000.,0.1,t_ctc/1000000000.,0.9);
    l_ctc->SetLineColor(linecolor);
    l_ctc->SetLineStyle(1);
    l_ctc->SetLineWidth(1);
    canvas_snapshot.at(index_hist)->cd();
    l_ctc->Draw("same");
    timestamp_snapshot.push_back(l_ctc);
  }

  //Loop over orphan timestamp information
  double t_first_orphan, t_last_orphan;
  for (int i_entry = 0; i_entry < entries_orphan; i_entry++){
    t_datasummary_orphan->GetEntry(i_entry);
    if (i_entry==0) t_first_orphan = orphantimestamp/1000000000.;
    else if (i_entry==entries_orphan-1) t_last_orphan = orphantimestamp/1000000000.;
    if (orphantimestamp/1000000000.<t_min || orphantimestamp/1000000000.>t_max) continue;
    int index_hist = trunc((orphantimestamp/1000000000.-t_min)/seconds_per_plot);
    if (index_hist < 0 || index_hist >=num_snapshots) continue;
    if (*cause_orphan == "tank_no_ctc") std::cout <<"OrphanedEventType: "<<*type_orphan<<", timestamp: "<<orphantimestamp<<", cause: "<<*cause_orphan<<", index_hist: "<<index_hist<<std::endl;
    std::cout <<"OrphanedEventType: "<<*type_orphan<<", timestamp: "<<orphantimestamp<<", cause: "<<*cause_orphan<<", index_hist: "<<index_hist<<std::endl;
    double lmin=0.;
    double lmax=0.;
    int linecolor=1;
    if (*cause_orphan == "tank_no_ctc" || *cause_orphan =="mrd_beam_no_ctc" || *cause_orphan =="mrd_cosmic_no_ctc") linecolor = 2;
    else if (*cause_orphan == "incomplete_tank_event") linecolor = kCyan;
    if (*type_orphan=="Tank"){
      lmin=3.1;
      lmax=3.9;
    } else if (*type_orphan=="MRD"){
      lmin=2.1;
      lmax=2.9;
    } else if (*type_orphan=="CTC"){
      lmin=1.1;
      lmax=1.9;
    }
    TLine *l_orphan = new TLine(orphantimestamp/1000000000.,lmin,orphantimestamp/1000000000.,lmax);
    l_orphan->SetLineColor(linecolor);
    l_orphan->SetLineStyle(2);
    l_orphan->SetLineWidth(1);
    canvas_snapshot.at(index_hist)->cd();
    l_orphan->Draw("same");
    timestamp_snapshot.push_back(l_orphan);
  }

  double t_max_orphan = (t_last_orphan >= t_first_orphan)? t_last_orphan : t_first_orphan;
  double t_min_orphan = (t_last_orphan < t_first_orphan)? t_last_orphan : t_first_orphan;

  double t_min_global = (t_min_orphan < t_min_matched)? t_min_orphan : t_min_matched;
  if (t_min < t_min_global) t_min_global = t_min;
  double t_max_global = (t_max_orphan > t_max_matched)? t_max_orphan : t_max_matched;
  if (t_max > t_max_global) t_max_global = t_max;

  canvas_timestreams = new TCanvas("canvas_timestreams","Timestreams",900,600);
  TH2F *frame_timestreams = new TH2F("frame_timestreams","Timestreams",10,t_min_global-100,t_max_global+100,5,0,5);
  frame_timestreams->GetYaxis()->SetBinLabel(1,"Orphaned Event");
  frame_timestreams->GetYaxis()->SetBinLabel(2,"Matched Event");
  frame_timestreams->GetYaxis()->SetBinLabel(3,"CTC Data");
  frame_timestreams->GetYaxis()->SetBinLabel(4,"MRD Data");
  frame_timestreams->GetYaxis()->SetBinLabel(5,"Tank PMT");
  frame_timestreams->GetXaxis()->SetTimeDisplay(1);
  frame_timestreams->GetXaxis()->SetLabelSize(0.03);
  frame_timestreams->GetXaxis()->SetLabelOffset(0.03);
  frame_timestreams->GetXaxis()->SetTimeFormat("#splitline{%m/%d}{%H:%M:%S}");
  frame_timestreams->GetYaxis()->SetTickLength(0.);
  frame_timestreams->SetStats(0);
  canvas_timestreams->cd();
  frame_timestreams->Draw();

  canvas_timestreams->cd();
  TBox *timestream_orphan = new TBox(t_min_orphan,0.1,t_max_orphan,0.9);
  timestream_orphan->SetFillColor(kBlack);
  timestream_orphan->Draw();
  TBox *timestream_matched = new TBox(t_min_matched,1.1,t_max_matched,1.9);
  timestream_matched->SetFillColor(kGreen+8);
  timestream_matched->Draw();
  TBox *timestream_ctc = new TBox(t_min,2.1,t_max,2.9);
  timestream_ctc->SetFillColor(kOrange);
  timestream_ctc->Draw();
  TBox *timestream_mrd = new TBox(t_min_mrd,3.1,t_max_mrd,3.9);
  timestream_mrd->SetFillColor(kCyan);
  timestream_mrd->Draw();
  TBox *timestream_pmt = new TBox(t_min_pmt,4.1,t_max_pmt,4.9);
  timestream_pmt->SetFillColor(kRed);
  timestream_pmt->Draw();

  timestreams_boxes.push_back(timestream_orphan);
  timestreams_boxes.push_back(timestream_matched);
  timestreams_boxes.push_back(timestream_ctc);
  timestreams_boxes.push_back(timestream_mrd);
  timestreams_boxes.push_back(timestream_pmt);

  f_out->cd();
  canvas_timestreams->Write();
  for (int i_snap = 0; i_snap < (int) canvas_snapshot.size(); i_snap++){
    canvas_snapshot.at(i_snap)->Write();
  }
  f_out->Close();
  delete f_out;
 
  return true;
}


bool PlotDecodedTimestamps::Finalise(){

  f_datasummary->Close();
  delete f_datasummary;
  f_timestamps->Close();
  delete f_timestamps;
  
  for (int i_canvas = 0; i_canvas < (int) canvas_snapshot.size(); i_canvas++){
    delete canvas_snapshot.at(i_canvas);
  }
 
  for (int i_line = 0; i_line < (int) timestamp_snapshot.size(); i_line++){
    delete timestamp_snapshot.at(i_line);
  }

  delete canvas_timestreams;

  for (int i_box = 0; i_box < (int) timestreams_boxes.size(); i_box++){
    delete timestreams_boxes.at(i_box);
  }

  return true;
}

void PlotDecodedTimestamps::SetupFilesAndTrees(){

  f_datasummary = new TFile(input_datasummary.c_str(),"READ");
  t_datasummary = (TTree*) f_datasummary->Get("EventStats");
  t_datasummary_orphan = (TTree*) f_datasummary->Get("OrpahStats");
  f_timestamps = new TFile(input_timestamps.c_str(),"READ");
  t_timestamps = (TTree*) f_timestamps->Get("tree_timestamps_ctc");
  t_timestamps_pmt = (TTree*) f_timestamps->Get("tree_timestamps_pmt");
  t_timestamps_mrd = (TTree*) f_timestamps->Get("tree_timestamps_mrd");

  t_datasummary->SetBranchAddress("TriggerWord",&triggerword);
  t_datasummary->SetBranchAddress("CTCTimestamp",&ctctimestamp);
  t_datasummary->SetBranchAddress("MRDTimestamp",&mrdtimestamp);
  t_datasummary->SetBranchAddress("PMTTimestamp",&pmttimestamp);

  type_orphan = new std::string;
  cause_orphan = new std::string;
  t_datasummary_orphan->SetBranchAddress("OrphanedEventType",&type_orphan);
  t_datasummary_orphan->SetBranchAddress("OrphanTimestamp",&orphantimestamp);
  t_datasummary_orphan->SetBranchAddress("OrphanCause",&cause_orphan);

  t_timestamps->SetBranchAddress("t_ctc",&t_ctc);
  t_timestamps->SetBranchAddress("triggerword_ctc",&triggerword_ctc);
  t_timestamps_pmt->SetBranchAddress("t_pmt",&t_pmt);
  t_timestamps_mrd->SetBranchAddress("t_mrd",&t_mrd);

  entries_datasummary = t_datasummary->GetEntries();
  entries_orphan = t_datasummary_orphan->GetEntries();
  entries_timestamps = t_timestamps->GetEntries();
  entries_timestamps_pmt = t_timestamps_pmt->GetEntries();
  entries_timestamps_mrd = t_timestamps_mrd->GetEntries();

  f_out = new TFile(output_timestamps.c_str(),"RECREATE");

}

void PlotDecodedTimestamps::SetupTriggerColorScheme(){

  //Always include beam and MRD CR trigger
  vector_triggerwords.push_back(5);
  vector_triggerwords.push_back(36);
  map_triggerword_color.emplace(5,8);
  map_triggerword_color.emplace(36,9);

  ifstream filetrigger(triggerword_config.c_str());
  int temp_word;
  int temp_color;
  while(!filetrigger.eof()){
    filetrigger >> temp_word >> temp_color;
    if (filetrigger.eof()) break;
    map_triggerword_color.emplace(temp_word,temp_color);
    vector_triggerwords.push_back(temp_word);
  }
  filetrigger.close();

}
