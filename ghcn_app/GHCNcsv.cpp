#include "GHCNcsv.hpp"

// Globals, yuck.  
int avgNyear_g;
int minBaselineSampleCount_g;
// #define MAXFILES (10)


GHCN::GHCN(const char  *inFile, const int& avgNyear)
{
  int ichar=0;

  try
  {
    mInputFstream = new fstream(inFile,fstream::in);
    mbFileIsOpen=true;
  }
  catch(...) // some sort of file open failure
  {
    cerr << endl << endl;
    cerr << "Failed to create a stream for " << inFile << endl;
    cerr << "Exiting.... " << endl;
    cerr << endl << endl;
    mInputFstream->close(); // make sure it's closed
    exit(1);
  }

  if(!mInputFstream->is_open())
  {
    cerr << endl << endl;
    cerr << "Failed to open " << inFile << endl;
    cerr << "Exiting.... " << endl;
    cerr << endl << endl;
    exit(1);
  }
  
  mCbuf = new char[BUFLEN];
  mCcountry = &mCbuf[ichar];
  ichar+=3;
  mCstation = &mCbuf[ichar];
  ichar+=9;
  mCyear = &mCbuf[ichar];
  ichar+=4;
  mCtemps = &mCbuf[ichar];
  
}

GHCN::~GHCN()
{
  delete mInputFstream;
  delete[] mCbuf;
}

bool GHCN::IsFileOpen()
{
  return mbFileIsOpen;
}


void GHCN::DumpResults()
{
  // map<int, vector<double> >::iterator iyy;
  map<int, vector<double> >::iterator iyy;
  int imm;
  float year_avg;
  
  for(iyy=mGlobalAverageMonthlyAnomalies.begin(); 
      iyy!=mGlobalAverageMonthlyAnomalies.end(); iyy++)
  {
    year_avg=0;
    
    for(imm=0; imm<12; imm++)
    {
      year_avg += iyy->second[imm];
    }

    year_avg/=12;

    cout << 1.*iyy->first << "," << year_avg << endl;

  }

}

void GHCN::DumpSmoothedResults() // MERGE_MODE mode)
{
  map<int, double >::iterator iyy;

  
  for(iyy=mSmoothedGlobalAverageAnnualAnomalies.begin();
      iyy!=mSmoothedGlobalAverageAnnualAnomalies.end();
      iyy++)
  {
    cout << iyy->first << ","
	 << iyy->second << "," << endl;
  }
  
  return;

}


void GHCN::MergeMonthsToYear(MERGE_MODE mode)
{
  
  map<int, vector<double> >::iterator iyy;
  int imm;
  float year_avg;
  float year_max;
  float year_min;
  
  for(iyy=mGlobalAverageMonthlyAnomalies.begin(); 
      iyy!=mGlobalAverageMonthlyAnomalies.end(); iyy++)
  {
    year_avg=0;
    year_max=0;
    year_min=0;
    
    int mm_avg_count=0;
    switch(mode)
    {
      case MERGE_AVG:
	// Compute an average of all months for this year.
	for(imm=0; imm<12; imm++)
	{
	  if(iyy->second[imm]>GHCN_NOTEMP+ERR_EPS)
	  {
	    year_avg += iyy->second[imm];
	    mm_avg_count+=1;
	  }
	}
	// Add the value to the anomaly map only if it's
	// a valid temperature value.
	if(mm_avg_count>=1)
	{
	  year_avg/=mm_avg_count;
	  mGlobalAverageAnnualAnomalies[iyy->first] = year_avg;
	}
	break;
	
      case MERGE_MAX:
	// Use the maximum monthly anomaly for the year
	// as this year's global anomaly.
	year_max=iyy->second[0];
	for(imm=1; imm<12; imm++)
	{
	  year_max=MAX(year_max,iyy->second[imm]);
	}
	// Add the value to the anomaly map only if it's
	// a valid temperature value.
	if(year_max > GHCN_NOTEMP+ERR_EPS)
	{
	  mGlobalAverageAnnualAnomalies[iyy->first] = year_max;
	}
	break;

      case MERGE_MIN:
	// Use the minimum monthly anomaly for the year
	// as this year's global anomaly.
	year_min=iyy->second[0];
	for(imm=1; imm<12; imm++)
	{
	  if(iyy->second[imm]>GHCN_NOTEMP+ERR_EPS)
	  {
	    year_min=MIN(year_min,iyy->second[imm]);
	  }
	}
	// Add the value to the anomaly map only if it's
	// a valid temperature value.
	if(year_min > GHCN_NOTEMP+ERR_EPS)
	{
	  mGlobalAverageAnnualAnomalies[iyy->first] = year_min;
	}
	break;
    }
	
  }

  return;

}


void GHCN::ComputeGlobalAverageAnomalies(const int& minBaselineSampleCount)
{
  
  map<int, map<int, vector<float> > >::iterator iss; //station-id iterator
  map<int, vector<float> >:: iterator iyy; // year iterator

  int imm; // month index

  for(iss=mTempsMap.begin(); iss!=mTempsMap.end(); iss++)
  {
    for(iyy=iss->second.begin(); iyy!=iss->second.end(); iyy++)
    {
      if(mGlobalAverageMonthlyAnomalies[iyy->first].size() == 0)
      {
	mGlobalAverageMonthlyAnomalies[iyy->first].resize(12);
	for(imm=0; imm<12; imm++)
	{
	  mGlobalAverageMonthlyAnomalies[iyy->first][imm]=0;
	}
      }
      
      for(imm=0; imm<12; imm++)
      {
	// Do we have a valid temperature sample?
	if(iyy->second[imm]>GHCN_NOTEMP+ERR_EPS)
	{
	  // Do we have enough baseline temperature samples to include
	  // this station/month in the anomaly average?
	  if(mBaselineSampleCount[iss->first][imm]>=minBaselineSampleCount)
	  {
	    if(mGlobalAverageMonthlyAnomalies.find(iyy->first)
	       ==mGlobalAverageMonthlyAnomalies.end())  
	    {
	      // 1st element for this particular year and month?  Need
	      // to create a new map entry.
	      mGlobalAverageMonthlyAnomalies[iyy->first][imm] = 
		iyy->second[imm]-mBaselineTemperature[iss->first][imm];
	      mAverageStationCount[iyy->first][imm]=1;
	    }
	    else
	    {
	      // Station counter element already exists for year/month -- increment it.
	      mGlobalAverageMonthlyAnomalies[iyy->first][imm] 
		+= iyy->second[imm]-mBaselineTemperature[iss->first][imm];
	      mAverageStationCount[iyy->first][imm]+=1;
	    }
	  }
	}
      }
    }
  }

  // Now have anomaly sums (summed over all qualifying stations) 
  // for each year and month. Divide by the number of stations included 
  // for each year and month to get the average anomaly  values.
  map<int,vector<double> >::iterator avg_iyy;
  for(avg_iyy=mGlobalAverageMonthlyAnomalies.begin(); 
      avg_iyy!=mGlobalAverageMonthlyAnomalies.end(); avg_iyy++)
  {
    // Loop over months in a given year.
    for(imm=0; imm<12; imm++)
    {
      if(mAverageStationCount[avg_iyy->first][imm]>=1)
      {
	avg_iyy->second[imm] /= mAverageStationCount[avg_iyy->first][imm];
      }
      else
      {
	// No station data found for this year/month?
	// Then set to GHCN_NOTEMP so that this entry won't
	// used to compute the annual anomaly temperatures.
	avg_iyy->second[imm]=GHCN_NOTEMP;
      }
    }
    
  }

  return;
  
}

// void GHCN::ComputeMovingAvg()
// {
//   ComputeMovingAvg(mIavgNyear);
//   return;
// }

void GHCN::ComputeMovingAvg(const int& nel_in)
{
  map<int, double >::iterator iyy_init;
  map<int, double >::iterator iyy_center;
  map<int, double >::iterator iyy_leading;
  map<int, double >::iterator iyy_trailing;

  int filt_halfwidth;
  
  double year_avg;
  int icount;
  int nel;
  
  nel=nel_in;
  
  if(nel%2==0)
  {
    // make sure nel is odd
    cerr << endl;
    cerr << "Moving average filter length incremented to " << nel+1 << endl;
    cerr << "Moving average filter length must be odd. " << endl;
    cerr << endl;
    
    nel+=1;
  }
  filt_halfwidth = nel/2;
  
  if(nel==1)
  {
    // No smoothing -- Just do a straight copy and return.
    for(iyy_init=mGlobalAverageAnnualAnomalies.begin(); 
	iyy_init!=mGlobalAverageAnnualAnomalies.end(); iyy_init++)
    {
      mSmoothedGlobalAverageAnnualAnomalies[iyy_init->first]=iyy_init->second;
    }
    return;
  }

  icount=0;

  iyy_trailing=mGlobalAverageAnnualAnomalies.begin();
  iyy_init=iyy_trailing;
  // This is ugly -- but it's the only way that I could
  // get the iterator initialized to the moving-average
  // window center.
  iyy_center=mGlobalAverageAnnualAnomalies.begin();
  for(icount=0; icount<filt_halfwidth; icount++)
  {
    iyy_center++;
  }

  // Initialize the moving-average window sum...
  year_avg=0;
  iyy_leading=mGlobalAverageAnnualAnomalies.begin();
  for(icount=0; icount<nel; icount++)
  {
    year_avg+=iyy_leading->second/nel;
    iyy_leading++;
  }
  mSmoothedGlobalAverageAnnualAnomalies[iyy_center->first] = year_avg;

  iyy_center++;
  iyy_trailing++;
//   iyy_leading++;
  
  // Got the moving average started up -- walk through the remaining
  // data, adding the leading value and subtracting the trailing
  // value for each new moving-average output.
  while(iyy_leading != mGlobalAverageAnnualAnomalies.end())
  {
      year_avg += 1.*(iyy_leading->second-iyy_trailing->second)/nel;
      mSmoothedGlobalAverageAnnualAnomalies[iyy_center->first] = year_avg;

      iyy_center++;
      iyy_leading++;
      iyy_trailing++;
  }

  return;
  
}


void GHCN::ComputeBaselines(void)
{

  int yykey;
  
  map<int, map<int, vector<float> > >::iterator iss;
  int imm;

  // Iterate through all the stations in the temperature map.
  for(iss=mTempsMap.begin(); iss!=mTempsMap.end(); iss++)
  {
    // Loop through the years in the temperature baseline period.
    for(yykey=FIRST_BASELINE_YEAR; yykey<=LAST_BASELINE_YEAR; yykey++)
    {
      // Do we have an entry for this particular year?
      if(iss->second.find(yykey)!=iss->second.end())
      {
	for(imm=0; imm<12; imm++)
	{
	  // Check for sample validity.  Invalid/missing samples
	  // have been set equal to GHCN_NOTEMP. Skip over -9999
	  // missing temperature values.
	  if(iss->second[yykey][imm] > GHCN_NOTEMP+ERR_EPS)
	  {
	    bool first_count=false;

	    // Probably overkill here -- but I don't want to assume that
	    // all new map entries are initialized to 0.
	    // Check to see if there's a station entery in the baseline sample count map.
	    if(mBaselineSampleCount.find(iss->first) 
	       == mBaselineSampleCount.end())
	    {
	      first_count=true;
	    }
	    // If there is a station entry, check to see if there's a month
	    // entry in this map.
	    else if(mBaselineSampleCount[iss->first].find(imm) 
	            == mBaselineSampleCount[iss->first].end())
	    {
	      first_count=true;
	    }

	    // No station/month entry in this map yet -- so create one
	    // and make sure that the baseline sample-count value is initialized to 0.
	    if(first_count==true)
	    {
	      mBaselineSampleCount[iss->first][imm]=0;
	    }

	    // First valid temperature for this station and month?
	    // Then initialize the baseline temperature map to the
	    // temperature value.
	    if(mBaselineSampleCount[iss->first][imm]<1)
	    {
	      mBaselineTemperature[iss->first][imm]=iss->second[yykey][imm];
	    }
	    else
	    {
	      // Already have a temperature sum going -- update it.
	      mBaselineTemperature[iss->first][imm]+=iss->second[yykey][imm];
	    }
	    // Increment the baseline sample count for this station and month.
	    mBaselineSampleCount[iss->first][imm]+=1;
	  }
	}
      }
    }
  }

  // Divide each baseline temperature sum by the number of valied samples 
  // found in the baseline time-period for each station and month to
  // get the baseline average temperature for the corresponding station and month.
  for(iss=mTempsMap.begin(); iss!=mTempsMap.end(); iss++)
  {
    for(imm=0; imm<12; imm++)
    {
      if(mBaselineSampleCount[iss->first][imm]>1)
      {
	mBaselineTemperature[iss->first][imm] /= mBaselineSampleCount[iss->first][imm];
      }
    }
  }
}

void GHCN::ReadTemps(void)
{
  string year;
  string month;
  int cc;
  int ss;
  int tt[12];
  int yy;
  
  mInputFstream->exceptions(fstream::badbit 
			    | fstream::failbit 
			    | fstream::eofbit);

  try
  {
    while(!mInputFstream->eof())
    {
	
      mInputFstream->getline(mCbuf,BUFLEN);

      sscanf(mCcountry, "%3d", &cc);

      sscanf(mCstation, "%5d", &ss);
      mIstation=ss;

      sscanf(mCyear,"%4d",&yy);
      mIyear=yy;

      // Initialize to "invalid" temperature values
      // so we can keep track of data-gaps.
      int ii;
      for(ii=0; ii<12; ii++)
      {
	tt[ii]=GHCN_NOTEMP;
      }

      if(mIyear >= MIN_GISS_YEAR)
      {
	sscanf(mCtemps,"%5d%5d%5d%5d%5d%5d%5d%5d%5d%5d%5d%5d",
	       &tt[0],&tt[1],&tt[2],&tt[3],&tt[4],&tt[5],
	       &tt[6],&tt[7],&tt[8],&tt[9],&tt[10],&tt[11]);
	
	mTempsMap[mIstation][mIyear].resize(12);
	
	for(ii=0; ii<12; ii++)
	{
	  if(tt[ii]>GHCN_NOTEMP+ERR_EPS)
	  {
	    // Got a valid value? Divide the GHCN temperature*10
	    // number down to get the proper temperature value.
	    mTempsMap[mIstation][mIyear][ii]=tt[ii]/10.0;
	  }
	  else
	  {
	    // Make sure missing temperature values are marked
	    // by -9999 entries in the station/year/month temperature map.
	    mTempsMap[mIstation][mIyear][ii]=GHCN_NOTEMP;
	  }
	}
      }
    }
  }
  catch(fstream::failure ff)
  {
    cerr << endl 
	 << "fstream:: Failure error thrown. " << endl
	 << "(No problemo -- just reached end of file)." 
	 << endl << endl;
  }

  mInputFstream->close();
  
}


void ProcessOptions(int argc, char **argv)
{
  int optRtn;

  if(argc<2)
  {
    cerr << endl 
	 << "Usage: " << argv[0]  << endl
	 << "         [-A (int)smoothing-filter-length-years] \\ " << endl
         << "         [-B (int)min-baseline-sample-count] \\ "     << endl
         << "         (char*)GHCN-file1 (char*)GHCN-file2... " << endl
	 << endl;
    exit(1);
  }
  
  minBaselineSampleCount_g=GHCN::DEFAULT_MIN_BASELINE_SAMPLE_COUNT;
  avgNyear_g=GHCN::DEFAULT_AVG_NYEAR;
  
  while ((optRtn=getopt(argc,argv,"A:B:"))!=-1)
  {
    switch(optRtn)
    {
      case 'A':
	avgNyear_g=atoi(optarg);
	break;
	
      case 'B':
	minBaselineSampleCount_g=atoi(optarg);
	break;
	
      default:
	cerr << endl;
	cerr << "Usage: " << argv[0] << endl
	     << "         [-A (int)smoothing-filter-length-years] \\ " << endl
	     << "         [-B (int)min-baseline-sample-count] \\ "  << endl
	     << "         (char*)GHCN-file1 (char*)GHCN-file2... " << endl
	     << endl;
	exit(1);
    }
  }
  avgNyear_g=MAX(1,MIN(GHCN::MAX_AVG_NYEAR,avgNyear_g));
  minBaselineSampleCount_g=MAX(1,
     MIN(GHCN::LAST_BASELINE_YEAR-GHCN::FIRST_BASELINE_YEAR+1,
	 minBaselineSampleCount_g));
  
}

void DumpSmoothedResults(GHCN **ghcn, int ngh)
{
  
  map<int, double >::iterator iyy;
  int igh;
  
  // Iterate over years
  for(iyy=ghcn[0]->mSmoothedGlobalAverageAnnualAnomalies.begin();
      iyy!=ghcn[0]->mSmoothedGlobalAverageAnnualAnomalies.end();
      iyy++)
  {
    // Results for first input file
    // First 
    cout << iyy->first << ","
	 << iyy->second << ",";

    // Results for additional input files
    for(igh=1; igh<ngh; igh++)
    {
      if(ghcn[igh]->mSmoothedGlobalAverageAnnualAnomalies.find(iyy->first) != 
	 ghcn[igh]->mSmoothedGlobalAverageAnnualAnomalies.end())
      {
	cout << ghcn[igh]->mSmoothedGlobalAverageAnnualAnomalies[iyy->first];
	// Don't need a trailing comma after the last field
	if(igh<ngh-1)
	     cout << ",";
      }
      else
      {
	// No valid data for this year -- put in a blank/null csv placeholder
	// Also, don't need a trailing comma after the last field
	if(igh<ngh-1)
	  cout << ",";
      }

    }
    cout << endl; // end of this csv line..

  }

  return;
  
}



int main(int argc, char **argv)
{

  // GHCN* ghcn[MAXFILES];
  int igh;
  
  int ProcessOptions(int argc, char **argv);
  
  void DumpSmoothedResults(GHCN **ghcn, int nghcn);

  ProcessOptions(argc,argv);
  
  // if(argc-optind>MAXFILES)
  // {
  //   cerr << "Too many input file args" << endl;
  //   exit(1);
  // }
  
  
  cerr << endl 
       << "Smoothing filter length = " << avgNyear_g 
       << endl << endl;
  
  cerr << endl
       << "Will crunch " << argc-optind << " temperature files. " 
       << endl << endl;
  
  GHCN** ghcn = new GHCN*[argc-optind];
  

  // Loop through the GHCN file command-line args.
  for(igh=0; igh<argc-optind; igh++)
  {
    ghcn[igh] = new GHCN(argv[igh+optind],avgNyear_g);
  
    cerr << "Reading data from " 
	      << argv[igh+optind] << endl;
    ghcn[igh]->ReadTemps();
    
    cerr << "Computing baseline temps for " 
	 << argv[igh+optind]<< endl;
    ghcn[igh]->ComputeBaselines();
    
    cerr << "Computing average anomalies for " 
	 << argv[igh+optind] << endl;
    ghcn[igh]->ComputeGlobalAverageAnomalies(minBaselineSampleCount_g);
    
    ghcn[igh]->MergeMonthsToYear(GHCN::MERGE_AVG);
    
    cerr << "Computing " << avgNyear_g <<"-year moving averages for " 
	 << argv[igh+optind] << endl;
    ghcn[igh]->ComputeMovingAvg(avgNyear_g);

    cerr << endl << endl;
  
  }
  
  cerr << "Dumping results... " << endl<<endl<<endl;
  
  DumpSmoothedResults(ghcn, argc-optind);

  //
  // Get segfaults with explicit delete operations.
  // dunno why.... valgrind gives this app a clean
  // bill of health regarding illegal memory accesses.
  // for(igh=0; igh<argc-optind; argc++)
  // {
  //   delete ghcn[igh];
  // }
  // delete[] ghcn;
  
  
  return 0;
  
}
