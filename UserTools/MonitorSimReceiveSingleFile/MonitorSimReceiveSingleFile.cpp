#include "MonitorSimReceiveSingleFile.h"

MonitorSimReceiveSingleFile::MonitorSimReceiveSingleFile():Tool(){}


bool MonitorSimReceiveSingleFile::Initialise(std::string configfile, DataModel &data){

    /////////////////// Usefull header ///////////////////////
    if(configfile!="")  m_variables.Initialise(configfile); //loading config file
    //m_variables.Print();

    m_data= &data; //assigning transient data pointer
    /////////////////////////////////////////////////////////////////

    m_variables.Get("MRDDataPath", MRDDataPath);
    m_variables.Get("Mode",mode);
    m_variables.Get("verbose",verbosity);
    //m_variables.Print();

    if (verbosity > 2) {
        std::cout <<"MRDDataPath: "<<MRDDataPath<<std::endl;
        std::cout <<"Mode: "<<mode<<std::endl;
    }

    if (mode != "Single" && mode != "File") mode = "File";


    if (verbosity > 2) std::cout <<"Define CCData BoostStore"<<std::endl;
    srand(time(NULL));
    m_data->Stores["CCData"]=new BoostStore(false,2);  
    m_data->Stores["PMTData"]=new BoostStore(false,2);    

    BoostStore* indata=new BoostStore(false,0); //this leaks but its jsut for testing
    indata->Initialise(MRDDataPath);
    indata->Print(false); 
   
    MRDData = new BoostStore(false,2);
    MRDData2 = new BoostStore(false,2);
   
    std::cout <<"Getting CCData (1)"<<std::endl;
    indata->Get("CCData",*MRDData);
    MRDData->Print(false);
    std::cout <<"Getting CCData (2)"<<std::endl;
    indata->Get("CCData",*MRDData2);
    MRDData2->Print(false);
    PMTData = new BoostStore(false,2);
    std::cout <<"Getting PMTData (1)"<<std::endl;
    indata->Get("PMTData",*PMTData);
    PMTData->Print(false);
    int total_entries;
    PMTData->Header->Get("TotalEntries",total_entries);
    std::cout <<"PMTData total_entries: "<<total_entries<<std::endl;
    
    indata->Close();
    indata->Delete();
    delete indata;


    return true;
}


bool MonitorSimReceiveSingleFile::Execute(){

    if (mode == "File"){
            std::string State="DataFile";
            m_data->CStore.Set("State",State);
            MRDData2->Save("tmp");
            m_data->Stores["CCData"]->Set("FileData",MRDData2,false);
            int total_entries;
            PMTData->Header->Get("TotalEntries",total_entries);
            std::cout <<"PMTData total_entries: "<<total_entries<<std::endl; 
            PMTData->Save("tmp");
            m_data->Stores["PMTData"]->Set("FileData",PMTData,false);
	    std::cout <<"State is "<<State<<std::endl;
    }
    if (mode == "Single"){
            int event=rand() % 1000;
	    std::string State="MRDSingle";
            m_data->CStore.Set("State",State);
            MRDOut tmp;
            long entries;
            MRDData->Header->Get("TotalEntries",entries);
            MRDData->GetEntry(event);
            MRDData->Get("Data", tmp);
            m_data->Stores["CCData"]->Set("Single",tmp);
    }

    return true;
}


bool MonitorSimReceiveSingleFile::Finalise(){

    MRDData=0;
    PMTData=0;
    m_data->CStore.Remove("State");
    m_data->Stores["CCData"]->Remove("FileData");
    m_data->Stores["PMTData"]->Remove("FileData");
    m_data->Stores.clear();

    delete MRDData;
    delete MRDData2;
    delete PMTData;

    return true;
}

