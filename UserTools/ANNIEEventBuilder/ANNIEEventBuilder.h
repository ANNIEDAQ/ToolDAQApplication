#ifndef ANNIEEventBuilder_H
#define ANNIEEventBuilder_H

#include <string>
#include <iostream>

#include "Tool.h"
#include "TimeClass.h"
#include "TriggerClass.h"
#include "Waveform.h"

/**
 * \class ANNIEEventBuilder
 *
 * This is a blank template for a Tool used by the script to generate a new custom tool. Please fill out the description and author information.
*
* $Author: B.Richards $
* $Date: 2019/05/28 10:44:00 $
* Contact: b.richards@qmul.ac.uk
*/
class ANNIEEventBuilder: public Tool {


 public:

  ANNIEEventBuilder(); ///< Simple constructor
  bool Initialise(std::string configfile,DataModel &data); ///< Initialise Function for setting up Tool resources. @param configfile The path and name of the dynamic configuration file to read in. @param data A reference to the transient data class used to pass information between Tools.
  bool Execute(); ///< Execute function used to perform Tool purpose.
  bool Finalise(); ///< Finalise function used to clean up resources.

  void PauseDecodingOnAheadStream();  // Put together timestamps of finished decoding Tank Triggers and MRD Triggers 
  void PairTankPMTAndMRDTriggers();  // Put together timestamps of finished decoding Tank Triggers and MRD Triggers 
  void BuildANNIEEventRunInfo(int RunNum, int SubRunNum, int RunType, uint64_t RunStartTime);  //Loads run level information, as well as the entry number
  void BuildANNIEEventTank(uint64_t CounterTime, std::map<std::vector<int>, std::vector<uint16_t>> WaveMap);
  void BuildANNIEEventMRD(std::vector<std::pair<unsigned long,int>> MRDHits, 
        unsigned long MRDTimeStamp, std::string MRDTriggerType);
  void CardIDToElectronicsSpace(int CardID, int &CrateNum, int &SlotNum);
  void SaveEntryToFile(int RunNum, int SubRunNum);

 private:

  //####### MAPS THAT ARE LOADED FROM OR CONTAIN INFO FROM THE CSTORE (FROM MRD/PMT DECODING) #########
  std::map<uint64_t, std::vector<std::pair<unsigned long, int> > > MRDEvents;  //Key: {MTCTime}, value: "WaveMap" with key (CardID,ChannelID), value FinishedWaveform
  std::map<uint64_t, std::string>  TriggerTypeMap;  //Key: {MTCTime}, value: string noting what type of trigger occured for the event 
  std::map<uint64_t, std::map<std::vector<int>, std::vector<uint16_t> > > InProgressTankEvents;  //Key: {MTCTime}, value: map of in-progress PMT trigger decoding from WaveBank
  std::map<uint64_t, std::map<std::vector<int>, std::vector<uint16_t> > > FinishedTankEvents;  //Key: {MTCTime}, value: map of fully-built waveforms from WaveBank
  Store RunInfoPostgress;   //Has Run number, subrun number, etc...

  std::map<std::vector<int>,int> TankPMTCrateSpaceToChannelNumMap;
  std::map<std::vector<int>,int> AuxCrateSpaceToChannelNumMap;
  std::map<std::vector<int>,int> MRDCrateSpaceToChannelNumMap;
  BoostStore *RawData;
  BoostStore *TrigData;

  //######### INFORMATION USED FOR PAIRING UP TANK AND MRD DATA TRIGGERS ########
  int EventsPerPairing;  //Determines how many Tank and MRD events are needed before starting to pair up for event building
  std::vector<uint64_t> IPTankTimestamps;  //Contains timestamps for all PMT Events encountered so far, finished or unfinished
  std::vector<uint64_t> FinishedMRDTimestamps;  //Contains timestamps for all MRD events encountered so far
  std::vector<uint64_t> FinishedTankTimestamps;  //Contains timestamps for PMT Events that are fully built
  std::map<uint64_t,uint64_t> FinishedTankMRDPairs; //Pairs of Tank PMT/MRD counters signifying these events are ready to be built
  
  std::string BuildType;
  bool TankFileComplete;

  BoostStore *ANNIEEvent = nullptr;
  std::map<unsigned long, std::vector<Hit>> *TDCData = nullptr;

  std::string InputFile;

  // Number of trigger entries from TriggerData loaded here
  long trigentries=0;

  // Number of PMTs that must be found in a WaveSet to build the event
  //
  unsigned int NumWavesInSet = 131;  
  
  bool IsNewMRDData;
  bool IsNewTankData;

  //Run Number defined in config, others iterated over as ANNIEEvent filled
  uint32_t ANNIEEventNum;
  int CurrentRunNum;
  int CurrentSubRunNum;
  int CurrentRunType;
  int CurrentStarTime;
  int LowestRunNum;
  int LowestSubRunNum;
  int LowestRunType;
  int LowestStarTime;

  bool SaveToFile; 
  std::string SavePath;
  std::string ProcessedFilesBasename;


  /// \brief verbosity levels: if 'verbosity' < this level, the message type will be logged.
  int verbosity;
  int v_error=0;
  int v_warning=1;
  int v_message=2;
  int v_debug=3;
  std::string logmessage;
};


#endif
