// This tool creates a CalibratedWaveform object for each RawWaveform object
// that it finds stored under the "RawADCData" key in the ANNIEEvent store.
// It saves the CalibratedWaveform objects to the ANNIEEvent store using the
// key "CalibratedADCData".
//
// Phase I version by Steven Gardiner <sjgardiner@ucdavis.edu>
// Modified for Phase II by Teal Pershing <tjpershing@ucdavis.edu>
#pragma once

// ToolAnalysis includes
#include "CalibratedADCWaveform.h"
#include "Tool.h"
#include "Waveform.h"
#include "annie_math.h"
#include "ANNIEalgorithms.h"
#include "ANNIEconstants.h"

class TApplication;
class TCanvas;
class TGraph;
class TF1;
class TH1D;

class PhaseIIADCCalibrator : public Tool {

  public:

    PhaseIIADCCalibrator();
    bool Initialise(const std::string configfile,DataModel& data) override;
    bool Execute() override;
    bool Finalise() override;

  protected:

    /// @brief Compute the baseline for a particular RawChannel
    /// object using a technique taken from the ZE3RA code.
    /// @details See section 2.2 of https://arxiv.org/pdf/1106.0808.pdf for a
    /// description of the algorithm.
    void ze3ra_baseline(const std::vector< Waveform<unsigned short> >& raw_data,
      double& baseline, double& sigma_baseline, size_t num_baseline_samples);

    std::vector< CalibratedADCWaveform<double> > make_calibrated_waveforms_ze3ra(
      const std::vector< Waveform<unsigned short> >& raw_waveforms);
    
    /// @brief Fit a polynomial to the baseline of each waveform.
    std::vector< CalibratedADCWaveform<double> > make_calibrated_waveforms_rootfit(
      const std::vector<Waveform<short unsigned int> >& raw_waveforms);
    
    bool use_ze3ra_algorithm;
    bool use_root_algorithm;
    
    int verbosity;
    // All F-distribution probabilities above this value will pass the
    // variance consistency test in ze3ra_baseline(). That is, p_critical
    // is the maximum p-value for which we will reject the null hypothesis
    // of equal variances.
    double p_critical;
    
    size_t num_baseline_samples;
    size_t num_sub_waveforms;
    size_t num_waveform_points;
    size_t baseline_start_sample;
    
    int baseline_fit_order;
    bool redo_fit_without_outliers;
    int refit_threshold; // mV range of the initial baseline subtracted waveform must be > this to trigger refit
    
    // ROOT stuff for drawing the fit of the baseline
    bool draw_baseline_fit=false;
    TApplication* rootTApp=nullptr;
    TCanvas* baselineFitCanvas=nullptr;
    TGraph* calibrated_waveform_tgraph=nullptr;
    TF1* calibrated_waveform_fit=nullptr;
    TH1D* raw_datapoint_hist=nullptr;
    
    // verbosity levels: if 'verbosity' < this level, the message type will be logged.
    int v_error=0;
    int v_warning=1;
    int v_message=2;
    int v_debug=3;
    std::string logmessage;
    int get_ok;
};
