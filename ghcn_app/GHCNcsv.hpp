#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>

#if defined(_WIN32)
#include "getopt.h"
#else
#include <unistd.h>
#endif

#include <stdlib.h>

/*

  How to compile:

    g++ GHCNcsv.cpp -o gcsv.exe



  How to run:

    Download and uncompress the GHCN v2 temperature files 
              (v2.mean.Z, v2.mean_adj.Z, etc.)

    The compressed data files are available at: 
                     ftp://ftp.ncdc.noaa.gov/pub/data/ghcn/v2/
        

    To uncompress the GHCN files:
      uncompress v2.whatever.Z


    To run this utility:

      ./gcsv.exe  v2.mean v2.mean_adj v2.max...  > data.csv


      Anomaly outputs will be stored in data.csv in spreadsheet-readable form.


  Some parameters to twiddle with...:

  MIN_GISS_YEAR defines the first year of data to extract.
                The standard value is 1880 (NASA/GISS convention)

  FIRST_BASELINE_YEAR defines the first year of the baseline period.
  LAST_BASELINE_YEAR defines the last year of the baseline period.
                     The NASA/GISS standard baseline period is 1951-1980

  DEFAULT_MIN_BASELINE_SAMPLE_COUNT defines the default minimum number 
                                   of valid temperature samples in the baseline 
                                   period for a temperature station in a particular 
                                   month to be included in the anomaly calculations.  
                                   Valid range is 1-#baseline-years. #baseline years 
                                   is determined by FIRST_BASELINE_YEAR and 
                                   LAST_BASELINE_YEAR above.

                                   Can be overridden by the command-line arg -B

  DEFAULT_AVG_NYEAR defines the default length of the moving-average smoothing 
                   filter.  This is set to 5 years (below).  Can be overridden
                   with the command-line arg -A .



Overall approach.


1) The program reads in temperature data from a GHCN version-2 
   temperature data file and places the temperature samples in
   the mTempsMap class member.

   mTempsMap is a nested STL map template indexed by station
   ID number and year.  Each station-id/year element is a
   12 element vector containing the temperature for each month
   for the given station and year.

   temperatures are indexed by: mTempsMap[station-id][year][month]

   The STL map template and iterator function make easy to deal
   with data gaps (not all stations have temperature date for
   all years/months).


2) The 1950-1981 baseline temperatures are computed for each 
   station/month and placed in the class member mBaselineTemperature

   mBaselineTemperature is a nested STL map indexed by station-id and month.

   Since there are gaps in the data, there will be variations in the
   number of valid temperature samples in the baseline period for the
   different stations.

   The class member mBaselineSampleCount keeps track of the number
   of valid samples for each station/month in the baseline period.

   mBaselineSampleCount is a nested map indexed by station-id and month.

   For a temperature station to be included in the final average anomaly
   calculations for any given month, the station must have at least
   a minumum number of  valid temperature samples 
   (see DEFAULT_MIN_BASELINE_SAMPLE_COUNT above). 

   
3) After all the baseline temperatures are calculated for each
   station/month, temperature anomalies for each station/year/month
   are calculated by subtracting the mBaselineTemperature[station][month]
   element from the mTempMap elements corresponding to the same station/month;
   and averaged over all stations to produce global average anomalies
   for each year/month (stored in mGlobalAverageMonthlyAnomalies).


4) The monthly global-average anomalies are then merged into 
   annual global-average anomalies by merging each set of 12 months
   into a single annual global average anomaly.  

   Merging choices are average, minimum, or maximum (must change the
   argument GHCN::MERGE_MODE argument passed into the 
   GHCN::MergeMonthsToYear() method call and recompile).  Current 
   merging is GHCN::MERGE_AVG (averaging).


5) Annual anomalies are then smoothed with a moving-average filter.
   Moving-average filter length can be changed by changing AVG_NYEAR
   and recompiling.

6) Final results are written out to standard output in CSV format.
   Redirect to a file with the unix redirect (>) operator.


 */


using namespace std;

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define BUFLEN (512)  // more than long enough for input lines



class GHCN
{
 public:

  // All missing temperature fields in the GHCN files are designated
  // by -9999
	 static float GHCN_NOTEMP() { return -9999.0f; }

  // When comparing floating pt. vals against GHCN_NOTEMP,
  // make sure that we don't get bitten by floating-pt precision limitations.
	 static float ERR_EPS() { return 0.1f; }
  
  // Starting year to process -- NASA/GISS doesn't try
  // to compute temperature anomalies prior to this year
  // due to coverage and data quality deficiencies.
  // Results prior to the late 1800's are pretty crappy.
  static const int MIN_GISS_YEAR=1880;


  // These define the NASA/GISS temperature baseline time period.
  static const int FIRST_BASELINE_YEAR=1951;
  static const int LAST_BASELINE_YEAR=1980;


  // Minimum number of valid temperature samples during the baseline period
  // for a station to be included in the computation.
  static const int DEFAULT_MIN_BASELINE_SAMPLE_COUNT = 15; 

  static const int DEFAULT_AVG_NYEAR=5;
  // Default ength of moving-average smoothing filter in years 

  static const int MAX_AVG_NYEAR=20;
  // Max length of moving-average filter

  // Method of merging/averaging monthly anomalies
  // into a single number for a particular year.
  enum MERGE_MODE { MERGE_AVG, MERGE_MAX, MERGE_MIN };

  // This will contain the moving-average smoothed global temperature
  // anomalies (1 per year). Map is indexed by year.
  map<int, double> mSmoothedGlobalAverageAnnualAnomalies;

  GHCN(const char *inFile, const int& avgNyear);

  virtual ~GHCN();

  bool  IsFileOpen(void);
  void  ReadTemps(void);
  void  ComputeBaselines(void);
  void  ComputeGlobalAverageAnomalies(const int& minBaselineSampleCount);
  void  MergeMonthsToYear(MERGE_MODE mode);
//  void  ComputeMovingAvg(void);
  void  ComputeMovingAvg(const int& nel);
  void  DumpResults(void);
  void  DumpSmoothedResults(); // MERGE_MODE mode);
  

 protected:

  fstream* mInputFstream;

  bool mbFileIsOpen;
  
  char *mCbuf;
  char *mCcountry;
  char *mCstation;
  char *mCyear;
  char *mCtemps;

  
  // GHCN data line format
  //
  // (temperature data station ID)
  //   3-digit country code
  //   5-digit WMO station number
  //   3-digit modifier
  //   1-digit duplicate number
  //
  // (data year follows)
  // 4-digit year
  //
  // (temperature samples follow)
  // 12 5-digit temperature fields
  // 
  // (end of line)

  char mId[16]; // Station id info read into mId (more than long enough).

  int mIstation;
  int mIyear;
  
  // WMO station id, year, 12-element temperature vector (1 per month)
  map<int, map<int, vector<float> > > mTempsMap;

  // Station number, month:  baseline sample count for each individual month
  // for each station over the baseline interval 1950-1980
  map<int, map<int, int> > mBaselineSampleCount;

  // Station number, month: baseline average temperature
  map<int, map<int,float> > mBaselineTemperature;

  // Indexed by year, month -- average global anomalies for each year&month.
  map<int, vector<double> > mGlobalAverageMonthlyAnomalies;

  // Indexed by year -- average global anomalies for each year
  // (merged year&month anomalies).
  map<int, double >  mGlobalAverageAnnualAnomalies;

  //  Year, month, station count
  // number of valid stations per year, month
  map<int, map<int, int> > mAverageStationCount;

  fstream* openFile(const string& infile);
  
};

