  /*
 *  Cohort.cpp
 *
 * Purpose: call TEM core processes at community (cohort)-level within a common grid
 *
 * History:
 *   June 28, 2011, by F.-M. Yuan:
 *     (1) Recoding based on DOS-TEM's code;
 *     (2) DVM concepts added
 *
 * Important Notes:
 *   (1) There are two sets of 'ed', 'bd': one set is for 'vegetation'
 *       with multiple PFTs; and another set is the integrated for all.
 *       These two is not same for PFTs, but same for 'ground'
 *
 */

#include <string>
#include <map>

#include <netcdfcpp.h>

#include <boost/assign/list_of.hpp> // for 'list_of()'

#include "../TEMLogger.h"
#include "../TEMUtilityFunctions.h"

#include "Cohort.h"

extern src::severity_logger< severity_level > glg;

Cohort::Cohort() {
  BOOST_LOG_SEV(glg, info) << "Cohort constructor; instantiating a cohort object.";
}

Cohort::Cohort(int y, int x, ModelData* modeldatapointer):
    y(y), x(x), md(modeldatapointer) {

  BOOST_LOG_SEV(glg, info) << "Cohort constructor NEW STYLE!";
  
  BOOST_LOG_SEV(glg, info) << "Looking up and setting lat/lon for cohort...";
  std::pair<float, float> latlon = temutil::get_latlon(modeldatapointer->input+"proj_climate.nc", y, x);
  this->lat = latlon.first;
  this->lon = latlon.second;

  BOOST_LOG_SEV(glg, info) << "Make a CohortData...";
  this->cd = CohortData(); // empty? / uninitialized? / undefined? values...
  
  this->cd.cmttype = temutil::get_veg_class(modeldatapointer->input+"vegetation.nc", y, x);
  this->cd.drainage_type = temutil::get_drainage_class(modeldatapointer->input+"drainage.nc", y, x);

  BOOST_LOG_SEV(glg, info) << "Next, we build a CohortLookup object, properly configured with config directory and community type.";
  this->chtlu = CohortLookup( modeldatapointer->parameter_dir, temutil::cmtnum2str(cd.cmttype) );
  
  BOOST_LOG_SEV(glg, info) << "Create the vegetation object...";
  this->veg = Vegetation(this->cd.cmttype, modeldatapointer);
  this->soilbgc = Soil_Bgc();

  // might need to set the cd* and the ed* ???

  BOOST_LOG_SEV(glg, debug) << "Setup the NEW STYLE CLIMATE OBJECT ...";
  // FIX: Historic? Projected?? how to handle both??
  // Maybe:
  //this->hist_climate = Climate(modeldatapointer->hist_climate, y, x);
  //this->proj_climate = Climate(modeldatapointer->proj_climate, y, x);
  this->climate = Climate(modeldatapointer->input+"proj_climate.nc", y, x);

  this->soilenv = Soil_Env();
  
  // this seems to set a lot of pointers...
  // THis might need to be the last step in creating a cohort...
  // after all the other components are ready...
  this->initSubmodules();
  
  // using initSubmodules should obviate the need for this:
  // hack...
  //CohortData* cdp = &(this->cd);
  this->veg.setCohortData( &(this->cd) );

  
/*

  // domain
  Ground ground;

  // processes
  Vegetation_Env vegenv[NUM_PFT];
  Snow_Env snowenv;
  Soil_Env soilenv;
  SoilParent_Env solprntenv;

  Vegetation_Bgc vegbgc[NUM_PFT];

  WildFire fire;

  // output
  //OutRetrive outbuffer;

  // data
  EnvData ed[NUM_PFT];
  BgcData bd[NUM_PFT];
  EnvData * edall;
  BgcData * bdall;

  //FirData * fd;   // this for all PFTs and their soil

  //ModelData * md;
  //RegionData * rd;
  //GridData * gd;

  RestartData resid;    //for input

  Integrator vegintegrator[NUM_PFT];
  Integrator solintegrator;

*/

}

Cohort::~Cohort() {
};

// initialization of pointers used in modules called here
void Cohort::initSubmodules() {

  // ecosystem domain
  veg.setCohortData(&cd);
  veg.setCohortLookup(&chtlu);
  ground.setCohortLookup(&chtlu);

  // vegetation module pointers
  for (int i=0; i<NUM_PFT; i++) {
    veg.setEnvData(i, &ed[i]);
    veg.setBgcData(i, &bd[i]);
    vegenv[i].setCohortLookup(&chtlu);
    vegenv[i].setEnvData(&ed[i]);
    vegenv[i].setFirData(fd);
    vegbgc[i].setCohortLookup(&chtlu);
    vegbgc[i].setCohortData(&cd);
    vegbgc[i].setEnvData(&ed[i]);
    vegbgc[i].setBgcData(&bd[i]);
  }

  //snow-soil module pointers
  snowenv.setGround(&ground);
  snowenv.setCohortLookup(&chtlu);
  snowenv.setCohortData(&cd);
  snowenv.setEnvData(edall);
  soilenv.setGround(&ground);
  soilenv.setCohortLookup(&chtlu);
  soilenv.setCohortData(&cd);
  soilenv.setEnvData(edall);
  solprntenv.setGround(&ground);
  solprntenv.setEnvData(edall);
  soilbgc.setGround(&ground);
  soilbgc.setCohortLookup(&chtlu);
  soilbgc.setCohortData(&cd);
  soilbgc.setEnvData(edall);
  soilbgc.setBgcData(bdall);
  soilbgc.setFirData(fd);
  //fire module pointers
  fire.setCohortLookup(&chtlu);
  fire.setCohortData(&cd);
  fire.setAllEnvBgcData(edall, bdall);

  for (int i=0; i<NUM_PFT; i++) {
    fire.setBgcData(&bd[i], i);
  }

  fire.setFirData(fd);

  //BGC states change integration module pointers
  for (int i=0; i<NUM_PFT; i++) {
    vegintegrator[i].setBgcData(&bd[i]);
    vegintegrator[i].setVegetation_Bgc(&vegbgc[i]);
  }

  solintegrator.setBgcData(bdall);
  solintegrator.setSoil_Bgc(&soilbgc);
  // Output data pointers
  outbuffer.setDimensionData(&cd);
  outbuffer.setProcessData(-1, edall, bdall);

  for (int ip=0; ip<NUM_PFT; ip++) {
    outbuffer.setProcessData(ip, &ed[ip], &bd[ip]);
  }

  outbuffer.setFireData(fd);
};

//The following 'set...' functions allow initialized data pointers
//  outside be used here
void Cohort::setModelData(ModelData* mdp) {
  md = mdp;
};

void Cohort::setProcessData(EnvData * alledp, BgcData * allbdp, FirData *fdp) {
  edall = alledp;
  bdall = allbdp;
  fd = fdp;
  bdall->cd = &cd;
  edall->cd = &cd;

  for (int i=0; i<NUM_PFT; i++) {
    bd[i].cd = &cd;
    ed[i].cd = &cd;
  }
};

//re-initializing for a new community of all PFTs sharing same
//  atm/snow-soil domains within a grid
void Cohort::initStatePar() {

  // 7/10/2015 EXPERIMENT. Seems to help with soil temperature, but TDeep still comes out nan
  edall->update_from_climate(this->climate, 0, 0);
  for (int ipft = 0; ipft < NUM_PFT; ++ipft) {
    this->ed[ipft].update_from_climate(this->climate, 0, 0);
  }


  //
  if (md->initmode>=3) {
    cd.yrsdist = resid.yrsdist;
  }

  // FOR VEGETATION
  //vegetation dimension/structure
  veg.initializeParameter(); // set values from chtlu...

  if(md->initmode<3) {     // from 'chtlu' or 'sitein'
    veg.initializeState(); // <==== mostly set values from chtlu...
  } else {     // initmode  >=3: restart
    veg.initializeState5restart(&resid);
  }

  // pft needs to be initialized individually for 'envmodule' and 'bgcmodule'
  for (int ip=0; ip<NUM_PFT; ip++) {
    vegenv[ip].ipft = ip;
    vegbgc[ip].ipft = ip;
    //set-up paramters for vegetation processes
    vegenv[ip].initializeParameter();
    vegbgc[ip].initializeParameter();

    if(md->initmode<3) {
      vegbgc[ip].initializeState();
      vegenv[ip].initializeState();
    } else {
      vegbgc[ip].initializeState5restart(&resid);
      vegenv[ip].initializeState5restart(&resid);
    }
  }

  // initialize dimension/structure for snow-soil
  // first read in the default initial parameter for snow/soil
  ground.initParameter(); // doesn't seem to touch any Layers...

  snowenv.initializeParameter();
  soilenv.initializeParameter();
  soilbgc.initializeParameter();

  if(md->initmode < 3) {   //lookup or sitein
    ground.initDimension();   //read-in snow/soil structure from 'chtlu', does not appear to touch Layer objects...?

    // reset the soil texture data from grid-level soil.nc, rather than 'chtlu',
    // Note that the mineral layer structure is already defined above
//    if (md->runmode.compare("multi") == 0) {
//      float z = 0;
//
//      for (int i = 0; i < ground.mineral.num; i++) {
//        z += ground.mineral.dz[i];
//
//        if (z <= 0.30) {   //assuming the grid top-soil texture is for top 30 cm
//          BOOST_LOG_SEV(glg, err) << "NOT IMPLEMENTED YET!!! Setting mineral texture...";
//          //ground.mineral.texture[i] = gd->topsoil;
//        } else {
//          BOOST_LOG_SEV(glg, err) << "NOT IMPLEMENTED YET!!! Setting mineral texture...";
//          //ground.mineral.texture[i] = gd->botsoil;
//        }
//      }
//    }

    //then if we have sitein.nc, as specified. In this way, if sitein.nc may
    //  not provide all data, then model will still be able to use the default.
    if(md->initmode == 2) { //from sitein.nc specified as md->initialfile
//        setSiteStates(&sitein);
    }

    // set-up the snow-soil-soilparent structure
    //snow updated daily, while soil dimension at monthly
    BOOST_LOG_SEV(glg, debug) << "RIGHT BEFORE Ground initLayerStructure()" << ground.layer_report_string();
    ground.initLayerStructure(&cd.d_snow, &cd.m_soil);
    BOOST_LOG_SEV(glg, debug) << "RIGHT AFTER Ground initLayerStructure()" << ground.layer_report_string();

    cd.d_soil = cd.m_soil;

    //initializing snow/soil/soilparent env state
    //  conditions after layerStructure done
    snowenv.initializeNewSnowState(); //Note: ONE initial snow layer as new snow
    BOOST_LOG_SEV(glg, debug) << "RIGHT AFTER snowenv.initNewSnowState()" << ground.layer_report_string();

    soilenv.initializeState();
    BOOST_LOG_SEV(glg, debug) << "RIGHT AFTER soilenv.initializeState()" << ground.layer_report_string();

    solprntenv.initializeState();
    BOOST_LOG_SEV(glg, debug) << "RIGHT AFTER solprntenv.initializeState()" << ground.layer_report_string();

    // initializing soil bgc state conditions
    soilbgc.initializeState();
    BOOST_LOG_SEV(glg, debug) << "RIGHT AFTER Ground soilbgc.initializeState()" << ground.layer_report_string();

  } else {    // initmode>=3: restart
    // set-up the snow-soil structure from restart data
    //snow updated daily, while soil dimension at monthly
    ground.initLayerStructure5restart(&cd.d_snow, &cd.m_soil, &resid);
    cd.d_soil = cd.m_soil;
    // initializing snow/soil env state conditions from restart data
    snowenv.initializeState5restart(&resid);
    soilenv.initializeState5restart(&resid);
    solprntenv.initializeState5restart(&resid);
    // initializing soil bgc state conditions from restart data
    soilbgc.initializeState5restart(&resid);
  }

  //integrating the individual 'bd' initial conditions into
  //  'bdall' initial conditions, if veg involved
  getBd4allveg_monthly();
  // fire processes
  fd->init();

  if(md->initmode<3) {
    fire.initializeState();
  } else {
    fire.initializeState5restart(&resid);
  }

  fire.initializeParameter();
  
  BOOST_LOG_SEV(glg, debug) << "Done with Cohort::initStatepar()!  " << ground.layer_report_string();

};

void Cohort::prepareAllDrivingData() {
  // climate monthly data for all atm years

  //fire driving data (if input) for all years
  if (!md->get_friderived() && !md->runeq) {
    fire.prepareDrivingData();
  }
};

// climate daily data for one year
void Cohort::prepareDayDrivingData(const int & yrindx, const int & usedatmyr) {
  //default climate/co2 setting
  bool changeclm = true;
  bool changeco2 = true;

  if (md->runeq) {
    changeclm = false;
    changeco2 = false;
  } else if (md->runsp) {
    changeco2 = false;
  }

  // preparing ONE year daily driving data
  if (true/*timer->eqend*/) {
    //run the model after eq stage, climate and co2
    //  driver controlled by setting in control file.
    if (md->changeclimate == 1) {
      changeclm = true;
    } else if (md->changeclimate == -1) {
      changeclm = false;
    }

    if (md->changeco2 == 1) {
      changeco2 = true;
    } else if (md->changeco2 == -1) {
      changeco2 = false;
    }

  } else {
    //run the model at eq stage, climate and co2
    //  driver not controlled by setting in control file.
  }
};

void Cohort::updateMonthly(const int & yrcnt, const int & currmind,
                           const int & dinmcurr) {

  BOOST_LOG_SEV(glg, debug) << "Cohort::updateMonthly. Year: "
                            << yrcnt << " Month: " << currmind << " dinmcurr: "
                            << dinmcurr;

  //
  if(currmind==0) {
    cd.beginOfYear();
  }

  BOOST_LOG_SEV(glg, debug) << "Clean up before a month starts.";
  cd.beginOfMonth();
  if(md->get_envmodule()) {
  
    BOOST_LOG_SEV(glg, debug) << "RIGHT BEFORE updateMonthlyEnv()" << ground.layer_report_string();

  
    BOOST_LOG_SEV(glg, debug) << "Run the environmental module - updates water/thermal processes to get (bio)physical conditions.";
    updateMonthly_Env(currmind, dinmcurr);
    
    BOOST_LOG_SEV(glg, debug) << "RIGHT AFTER updateMonthlyEnv() yr:"<<yrcnt<<" m:"<<currmind<<" "<< ground.layer_report_string();

  }

  BOOST_LOG_SEV(glg, debug) << "Update the current dimension/structure of veg-snow/soil column (domain).";
  updateMonthly_DIMveg(currmind, md->get_dvmmodule());
  updateMonthly_DIMgrd(currmind, md->get_dslmodule());

  if(md->get_bgcmodule()) {
    BOOST_LOG_SEV(glg, debug) << "Run the BGC processes to get the C/N fluxes.";
    updateMonthly_Bgc(currmind);
  }

  if(md->get_dsbmodule()) {
    BOOST_LOG_SEV(glg, debug) << "Run the disturbance model.";
    updateMonthly_Fir(yrcnt, currmind);
  }

  BOOST_LOG_SEV(glg, debug) << "Clean up at the end of the month";
  cd.endOfMonth();

  if(currmind == 11) {
    BOOST_LOG_SEV(glg, debug) << "Clean up at end of year.";

    cd.endOfYear();
  }

  BOOST_LOG_SEV(glg, debug) << "TODO: ouput some data!";
//  if (md->outRegn) {
//    BOOST_LOG_SEV(glg, debug) << "Output all data for multiple cohorts.";
//    outbuffer.updateRegnOutputBuffer(currmind);
//  }
//
//  // always output the restart data (monthly)
//  BOOST_LOG_SEV(glg, debug) << "Output monthly restart data.";
//  outbuffer.updateRestartOutputBuffer();
};

/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
//Environment Module Calling at monthly time-step, but involving daily time-step loop
/////////////////////////////////////////////////////////
void Cohort::updateMonthly_Env(const int & currmind, const int & dinmcurr) {
//Yuan: note that the Veg-Env module calling is for a few PFTs within ONE cohort
//  1) ed calling is done for each PFTs within the module
//  2) Env-module calling is done for one PFT, so needs loop for vegetation-relevant processes
  // (i) the n factor for soil temperature calculation from Tair
  edall->d_soid.nfactor = 1;
  // Yuan: the following has temporarily commentted out - a lot of trouble
  /*  if(currmind>=5 && currmind<=9){  //for warm season: Yuan: this will make a BIG jump of soil temperature at 5/9
      if(cd.ifdeciwoody){      //deciduous woody community type
        edall->d_soid.nfactor = 0.94;
      }
      if(cd.ifconiwoody) {
        if(fd->ysf <veg.vegdimpar.matureagemx){
          edall->d_soid.nfactor = 1.1 -(fd->ysf)/veg.vegdimpar.matureagemx * (1.1 -0.66);
        }else{
          edall->d_soid.nfactor =0.66;
          }
      }
    }
  */

  // (ii)Initialize the yearly/monthly accumulators, which are accumulating at the end of month/day in 'ed'
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.d_veg.vegcov[ip] > 0.0) { // PROBLEM: looks like cd.d_veg.vegcov is not initialized yet??? (-77777)
      if(currmind==0) {
        ed[ip].atm_beginOfYear();
        ed[ip].veg_beginOfYear();
        ed[ip].grnd_beginOfYear();
      }

      ed[ip].atm_beginOfMonth();
      ed[ip].veg_beginOfMonth();
      ed[ip].grnd_beginOfMonth();
    }
  }

  //
  if(currmind==0) { // zero all (most?) of the EnvData.y_atms values...
    edall->atm_beginOfYear();
    edall->veg_beginOfYear();
    edall->grnd_beginOfYear();
  }

  edall->atm_beginOfMonth();
  edall->veg_beginOfMonth();
  edall->grnd_beginOfMonth();

  // (iii) daily light/water processes at plant canopy
  double tdrv, daylength;

  for(int id = 0; id < dinmcurr; id++) {

    cd.day = id + 1; // apparently cd uses one based indexing...?

    int doy = temutil::day_of_year(currmind, id);
    daylength = temutil::length_of_day(this->lat, doy);

    /* We are in a Cohort here.
       We use our Atmosphere obj to update our EnvData objects's daily
       climate arrays.

       Atmosphere carries daily data for a whole year in arrays shaped [12][31]

       EnvData carries single values for temp, co2, prec, rainfall, snowfall,
       nirr, par, and vapor pressure.

       ISSUE: At this point, Atmosphere must have already setup its daily
              structures.

    */

    //get the daily atm drivers and the data is in 'edall'
    //atm.updateDailyAtm(currmind, id);
    this->edall->update_from_climate(climate, currmind, id);

    //Initialize some daily variables for 'ground'
    cd.beginOfDay();
    edall->grnd_beginOfDay();
    //'edall' in 'atm' must be assgined to that in 'ed' for each PFT
    assignAtmEd2pfts_daily();

    for (int ip=0; ip<NUM_PFT; ip++) {
      if (cd.d_veg.vegcov[ip] > 0.0) {
        if (cd.d_veg.nonvascular <= 0) {   // for vascular plants
          // get the soil moisture controling factor on plant transpiration
          double frootfr[MAX_SOI_LAY];

          for (int i=0; i<MAX_SOI_LAY; i++) {
            frootfr[i] = cd.m_soil.frootfrac[i][ip];
          }

          soilenv.getSoilTransFactor(ed[ip].d_soid.fbtran,
                                     ground.fstsoill, frootfr);
          ed[ip].d_vegd.btran = 0.;

          for (int il=0; il<MAX_SOI_LAY; il++) {
            ed[ip].d_vegd.btran+=ed[ip].d_soid.fbtran[il];
          }
        } else {//for non-vascular plants - needs further algorithm development
          double rh = ed[ip].d_atmd.vp/ed[ip].d_atmd.svp;

          if ( rh >= 0.60 || ed[ip].d_soid.sws[0]>0.60) {
            ed[ip].d_vegd.btran = 1.;
          } else {
            ed[ip].d_vegd.btran = 0.;
          }
        }

        // calculate vegetation light/water dynamics at daily timestep
        vegenv[ip].updateRadiation(cd.m_veg.lai[ip], cd.m_veg.fpc[ip]);
        vegenv[ip].updateWaterBalance(daylength, cd.m_veg.lai[ip], cd.m_veg.fpc[ip]);
      }
    }

    // integrating daily 'veg' portion in 'ed' of all PFTs for 'edall'
    getEd4allveg_daily();
    /*
        if (cd.year==1 && doy==37){
          cout<<"checking";
        }
    //*/
    tdrv = edall->d_atms.ta;
    //Snow-soil Env-module: ground/soil temperatur e- moisture
    //  dynamics at daily timestep
    //note: hydrology is done separately for snow and soil,
    //      but thermal process is done as a continuous column
    //      so, thermal process (including phase changing) is carried
    //      out before hydrological process

    //snow-soil temperature, including snow-melting and
    //  soil water phase changing
    soilenv.updateDailyGroundT(tdrv, daylength);
    //snow water/thickness changing - must be done after 'T' because of melting
    snowenv.updateDailyM(tdrv);
    //get the new bottom drainage layer and its depth,
    //  which needed for soil moisture calculation
    ground.setDrainL(ground.lstsoill, edall->d_soid.ald,
                     edall->d_sois.watertab);
    soilenv.updateDailySM();  //soil moisture

    // save the variables to daily 'edall' (Note: not PFT specified)
    soilenv.retrieveDailyTM(ground.toplayer, ground.lstsoill);

    //assuming rock layer's temperature equal to that of lstsoill
    solprntenv.retrieveDailyTM(ground.lstsoill);

    //sharing the 'ground' portion in 'edall' with each pft 'ed'
    assignGroundEd2pfts_daily();

    getEd4land_daily();  // integrating 'veg' and 'ground' into 'land'

    //update Snow structure at daily timestep (for soil structure
    //  at yearly timestep in ::updateMonthly_DIMgrd)
    ground.retrieveSnowDimension(&cd.d_snow);

    cd.endOfDay(dinmcurr); //this must be done first, because it's needed below

    //accumulate daily vars into monthly for 'ed' of each PFT
    for (int ip=0; ip<NUM_PFT; ip++) {
      if (cd.d_veg.vegcov[ip] > 0.0) {
        ed[ip].atm_endOfDay(dinmcurr);
        ed[ip].veg_endOfDay(dinmcurr);
        ed[ip].grnd_endOfDay(dinmcurr, doy);

        // accumulate yearly vars at the last day of a month
        if(id==dinmcurr-1) {
          ed[ip].atm_endOfMonth();
          ed[ip].veg_endOfMonth(currmind);
          ed[ip].grnd_endOfMonth();
        }
      }
    }

    //accumulate daily vars into monthly for 'ed' of all pfts
    edall->atm_endOfDay(dinmcurr);
    edall->veg_endOfDay(dinmcurr); //be sure 'getEd4allpfts_daily' called above
    edall->grnd_endOfDay(dinmcurr, doy);

    // accumulate yearly vars at the last day of a month for all pfts
    if(id==dinmcurr-1) {
      edall->atm_endOfMonth();
      edall->veg_endOfMonth(currmind);
      edall->grnd_endOfMonth();
    }

    ////////////////////////////
    //output data store for daily - because the output is implemented monthly
    if (md->outSiteDay) {
      outbuffer.assignSiteDlyOutputBuffer_Env(cd.d_snow, -1, id);   // '-1' indicates for all-pft integrated datasets

      for (int ip=0; ip<NUM_PFT; ip++) {
        if (cd.d_veg.vegcov[ip]>0.0) {
          outbuffer.assignSiteDlyOutputBuffer_Env(cd.d_snow, ip, id);
        }
      }
    }
  } // end of day loop in a month
};

///////////////////////////////////////////////////////////////////////////////////////////
// Biogeochemical Module Calling at monthly timestep
///////////////////////////////////////////////////////////////////////////////////////////
void Cohort::updateMonthly_Bgc(const int & currmind) {
  //
  if(currmind==0) {
    for (int ip=0; ip<NUM_PFT; ip++) {
      if (cd.m_veg.vegcov[ip]>0.) {
        bd[ip].veg_beginOfYear();
        bd[ip].soil_beginOfYear();
        bd[ip].land_beginOfYear();
      }
    }

    bdall->veg_beginOfYear();
    bdall->soil_beginOfYear();
    bdall->land_beginOfYear();
  }

  // vegetation BGC module calling
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.m_veg.vegcov[ip]>0.) {
      vegbgc[ip].prepareIntegration(md->get_nfeed());
      vegintegrator[ip].updateMonthlyVbgc();
      vegbgc[ip].afterIntegration();
      bd[ip].veg_endOfMonth(); // yearly data accumulation

      if(currmind==11) {
        vegbgc[ip].adapt_c2n_ratio_with_co2(ed->y_l2a.eet, ed->y_l2a.pet, 0.0, ed->y_atms.co2);
        //const double & yreet, const double & yrpet, const double & initco2, const double & currentco2
        bd[ip].veg_endOfYear();
      }
    }
  }

  getBd4allveg_monthly(); // integrating the monthly pfts' 'bd' to allveg 'bdall'
  bdall->veg_endOfMonth(); // yearly data accumulation

  if(currmind==11) {
    bdall->veg_endOfYear();
  }

///////////////////////////////////////////////////////////////////////////////////////////////////
  // soil BGC module calling
  soilbgc.prepareIntegration(md->get_nfeed(), md->get_avlnflg(),
                             md->get_baseline());
  solintegrator.updateMonthlySbgc(MAX_SOI_LAY);
  soilbgc.afterIntegration();
  bdall->soil_endOfMonth();   // yearly data accumulation
  bdall->land_endOfMonth();

  //sharing the 'ground' portion in 'bdall' with each pft 'bd'
  assignSoilBd2pfts_monthly();
};

//fire disturbance module calling
/////////////////////////////////////////////////////////////////////////////////
void Cohort::updateMonthly_Fir(const int & yrind, const int & currmind) {
  if(currmind ==0) {
    fd->beginOfYear();
    fire.getOccur(yrind, md->get_friderived());
  }

  if (yrind==fire.oneyear && currmind==fire.onemonth) {
    //fire, C burning for each PFT, and C/N pools updated
    //  through 'bd', but not soil structure
    //soil root fraction also updated through 'cd'
    fire.burn();

    // summarize burned veg C/N of individual 'bd' for each PFT above
    for (int ip=0; ip<NUM_PFT; ip++) {
      if (cd.m_veg.vegcov[ip]>0.) {
        for (int i=0; i<NUM_PFT_PART; i++) {
          bdall->m_vegs.c[i]    += bd[ip].m_vegs.c[i];
          bdall->m_vegs.strn[i] += bd[ip].m_vegs.strn[i];
        }

        bdall->m_vegs.labn    += bd[ip].m_vegs.labn;
        bdall->m_vegs.call    += bd[ip].m_vegs.call;
        bdall->m_vegs.strnall += bd[ip].m_vegs.strnall;
        bdall->m_vegs.nall    += bd[ip].m_vegs.nall;
        bdall->m_vegs.deadc   += bd[ip].m_vegs.deadc;
        bdall->m_vegs.deadn   += bd[ip].m_vegs.deadn;
      }
    }

    //assign the updated soil C/N pools during firing
    //  to double-linked layer matrix in 'ground'
    soilbgc.assignCarbonBd2LayerMonthly();
    //then, adjusting soil structure after fire burning
    //  (Don't do this prior to the previous calling)
    ground.adjustSoilAfterburn();
    // and finally save the data back to 'bdall'
    soilbgc.assignCarbonLayer2BdMonthly();
    // update all other pft's 'bd'
    assignSoilBd2pfts_monthly();
    // update 'cd'
    cd.yrsdist = 0.;
    ground.retrieveSnowDimension(&cd.d_snow);
    ground.retrieveSoilDimension(&cd.m_soil);
    cd.d_soil = cd.m_soil;
    cd.y_soil = cd.m_soil;
    getSoilFineRootFrac_Monthly();
  }
};

/////////////////////////////////////////////////////////////////////////////////
//   Dynamical Vegetation Module (DVM) calling
////////////////////////////////////////////////////////////////////////////////
void Cohort::updateMonthly_DIMveg(const int & currmind, const bool & dvmmodule) {
  //switch for using LAI read-in (false) or dynamically with vegC
  // the read-in LAI is through the 'chtlu->envlai[12]', i.e., a 12 monthly-LAI
  if (dvmmodule) {
    veg.updateLAI5vegc = md->updatelai;
  } else {
    veg.updateLAI5vegc = false;
  }

  // vegetation standing age
  // tentatively set to a common age from 'ysf' - year since fire -
  //   should have more varability based on PFT types
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.m_veg.vegcov[ip]>0.) {
      cd.m_veg.vegage[ip] = cd.yrsdist;

      if (cd.m_veg.vegage[ip]<=0) {
        cd.m_vegd.foliagemx[ip] = 0.;
      }
    }
  }

  // update monthly phenological variables (factors used for GPP), and LAI
  veg.phenology(currmind);
  veg.updateLai(currmind); // this must be done after phenology
  //LAI updated above for each PFT, but FPC
  //   (foliage percent cover) may need adjustment
  veg.updateFpc();
  veg.updateVegcov();
  veg.updateFrootfrac();
};

/////////////////////////////////////////////////////////////////////////////////
//   Dynamical Soil Layer Module (DSL)
////////////////////////////////////////////////////////////////////////////////
void Cohort::updateMonthly_DIMgrd(const int & currmind, const bool & dslmodule) {
  // re-call the 'bdall' soil C contents and assign them to the double-linked layer matrix
  soilbgc.assignCarbonBd2LayerMonthly();

  //only update the thickness at begin of year, since it is a slow process
  if(dslmodule && currmind==0) {
    // calculate the OSL layer thickness from C contents
    ground.updateOslThickness5Carbon(ground.fstsoill);

    // above callings didn't modify the layer matrix structure in case that
    // some layers may be getting too thick or too thin due to C content
    // dynamics then, re-do layer division or combination is necessary
    // for better thermal/hydrological simulation
    if (cd.hasnonvascular && ground.moss.type<=0) {  //
      double prvpft = 0.;

      for (int ip=0; ip<NUM_PFT; ip++) {
        if (cd.m_veg.nonvascular[ip]!=I_vascular) {
          if (cd.m_veg.vegcov[ip]>prvpft) {
            ground.moss.type = cd.d_veg.nonvascular[ip];
          }
        }
      }
    }

    ground.redivideSoilLayers();
    // and save the bgc data in double-linked structure back to 'bdall'
    soilbgc.assignCarbonLayer2BdMonthly();
  }

  // update soil dimension
  ground.retrieveSoilDimension(&cd.m_soil);
  getSoilFineRootFrac_Monthly();
  cd.d_soil = cd.m_soil;      //soil dimension remains constant in a month
  // update all soil 'bd' to each pft
  assignSoilBd2pfts_monthly();
}

//////////////////////////////////////////////////////////////////////////////////////
// adjusting fine root fraction in soil
void Cohort::getSoilFineRootFrac_Monthly() {
  double mossthick = cd.m_soil.mossthick;
  double totfrootc = 0.;   //fine root C summed for all PFTs

  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.m_veg.vegcov[ip]>0.) {
      double layertop, layerbot;
      // covert PFT 10-layer root fraction to acculative ones for interpolating
      double cumrootfrac[MAX_ROT_LAY];
      cumrootfrac[0] = cd.m_veg.frootfrac[0][ip];

      for (int il=1; il<MAX_ROT_LAY; il++) {
        cumrootfrac[il] = cumrootfrac[il-1]+cd.m_veg.frootfrac[il][ip];
      }

      // calculate soil fine root fraction from PFT's 10-rootlayer structure
      // note: at this point, soil fine root fraction ACTUALLY
      //         is root biomass C distribution along soil profile
      for (int il=0; il<cd.m_soil.numsl; il++) {
        if (cd.m_soil.type[il]>0) {   // non-moss soil layers only
          layertop = cd.m_soil.z[il] - mossthick;
          layerbot = cd.m_soil.z[il]+cd.m_soil.dz[il]-mossthick;
          cd.m_soil.frootfrac[il][ip]
            = assignSoilLayerRootFrac(layertop, layerbot,
                                      cumrootfrac, ROOTTHICK);  //fraction
          cd.m_soil.frootfrac[il][ip] *= bd[ip].m_vegs.c[I_root];  //root C
          totfrootc += cd.m_soil.frootfrac[il][ip];
        }
      }
    }
  }

  // soil fine root fraction - adjusted by both vertical distribution
  //   and root biomass of all PFTs
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.m_veg.vegcov[ip]>0.) {
      for (int il=0; il<cd.m_soil.numsl; il++) {
        if (cd.m_soil.type[il]>0 && cd.m_soil.frootfrac[il][ip]>0.) {// non-moss soil layers only
          cd.m_soil.frootfrac[il][ip] /= totfrootc;
        } else {
          cd.m_soil.frootfrac[il][ip] = 0.;
        }
      }
    }
  }
};

double Cohort::assignSoilLayerRootFrac(const double & topz, const double & botz,
                                       const double cumrootfrac[MAX_ROT_LAY],
                                       const double dzrotlay[MAX_ROT_LAY]) {
  // determine soil layer's location in the root layer matrix
  int indxtop = -1;
  int indxbot = -1;
  double zrotlay[MAX_ROT_LAY];  // root layer top
  zrotlay[0] = 0.;

  for (int i=1; i<MAX_ROT_LAY; i++) {
    zrotlay[i] = zrotlay[i-1]+dzrotlay[i];

    if (topz >= zrotlay[i-1] && topz<zrotlay[i]) {
      indxtop = i-1;
    }

    if ((botz >= zrotlay[i-1] && botz<zrotlay[i]) ||
        (i==MAX_ROT_LAY-1 && botz>=zrotlay[i])) {
      indxbot = i-1;
      break;
    }
  }

  //calculating fine root fraction in a layer by linearly interpolation
  double frfrac=0.;
  double sumfractop = 0.0;
  double sumfracbot = 0.0;

  if (indxtop >= 0. && indxbot>=0) {
    if (indxtop == 0) {
      sumfractop = cumrootfrac[indxtop]/dzrotlay[indxtop]*topz;
    } else {
      sumfractop = (cumrootfrac[indxtop]-cumrootfrac[indxtop-1])
                   / dzrotlay[indxtop]
                   * (topz-zrotlay[indxtop])+cumrootfrac[indxtop-1];
    }

    if (indxbot == 0) {
      sumfracbot = cumrootfrac[indxbot]/dzrotlay[indxbot]*botz;
    } else {
      sumfracbot = (cumrootfrac[indxbot]-cumrootfrac[indxbot-1])
                   / dzrotlay[indxbot]
                   * (botz-zrotlay[indxbot])+cumrootfrac[indxbot-1];
    }

    frfrac = sumfracbot - sumfractop;

    if (frfrac<0.0) {
      frfrac = 0.0;
    }
  } else {
    frfrac = 0.;
  }

  return frfrac;
};


/////////////////////////////////////////////////////////////////////
// The following are for 'ed', 'bd' data integration from individual PFTs to all Vegetation
// Or, assign the 'atm' and 'ground' to each PFT
////////////////////////////////////////////////////////////////////////////////////////////

// assign 'atm' portion in 'edall' to all PFT's 'ed' at daily
//   (monthly/yearly not needed, which can be done in 'ed')
void Cohort::assignAtmEd2pfts_daily() {
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.d_veg.vegcov[ip]>0.) {
      ed[ip].d_atms = edall->d_atms;
      ed[ip].d_atmd = edall->d_atmd;
      ed[ip].d_a2l  = edall->d_a2l;
    }
  }
}

// assign 'ground' portion in 'edall' to all PFT's 'ed'
void Cohort::assignGroundEd2pfts_daily() {
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.d_veg.vegcov[ip]>0.) {
      ed[ip].d_snws = edall->d_snws;
      ed[ip].d_sois = edall->d_sois;
      ed[ip].d_snwd = edall->d_snwd;
      ed[ip].d_soid = edall->d_soid;
      ed[ip].d_soi2l  = edall->d_soi2l;
      ed[ip].d_soi2a  = edall->d_soi2a;
      ed[ip].d_snw2a  = edall->d_snw2a;
      ed[ip].d_snw2soi= edall->d_snw2soi;
      ed[ip].monthsfrozen  = edall->monthsfrozen;
      ed[ip].rtfrozendays  = edall->rtfrozendays;
      ed[ip].rtunfrozendays= edall->rtunfrozendays;
    }
  }
}

// integrating (fpc weighted) 'soid.fbtran' in each 'ed' to 'edall'
void Cohort::getSoilTransfactor4all_daily() {
  for (int il=0; il<MAX_SOI_LAY; il++) {
    edall->d_soid.fbtran[il] = 0.;

    for (int ip=0; ip<NUM_PFT; ip++) {
      if (cd.d_veg.vegcov[ip]>0.) {
        edall->d_soid.fbtran[il] += ed[ip].d_soid.fbtran[il];
      }
    }
  }
}

// integrating (fpc weighted) 'veg' portion in 'edall' to all PFT's 'ed'
void Cohort::getEd4allveg_daily() {
  edall->d_vegs.rwater  = 0.;
  edall->d_vegs.snow    = 0.;
  edall->d_vegd.rc      = 0.;
  edall->d_vegd.cc      = 0.;
  edall->d_vegd.btran   = 0.;
  edall->d_vegd.m_ppfd  = 0.;
  edall->d_vegd.m_vpd   = 0.;
  edall->d_a2v.rnfl     = 0.;
  edall->d_a2v.rinter   = 0.;
  edall->d_a2v.snfl     = 0.;
  edall->d_a2v.sinter   = 0.;
  edall->d_a2v.swdown   = 0.;
  edall->d_a2v.swinter  = 0.;
  edall->d_a2v.pardown  = 0.;
  edall->d_a2v.parabsorb= 0.;
  edall->d_v2a.swrefl   = 0.;
  edall->d_v2a.evap     = 0.;
  edall->d_v2a.tran     = 0.;
  edall->d_v2a.evap_pet = 0.;
  edall->d_v2a.tran_pet = 0.;
  edall->d_v2a.sublim   = 0.;
  edall->d_v2g.swthfl   = 0.;
  edall->d_v2g.rdrip    = 0.;
  edall->d_v2g.rthfl    = 0.;
  edall->d_v2g.sdrip    = 0.;
  edall->d_v2g.sthfl    = 0.;

  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.d_veg.vegcov[ip]>0.) {
      edall->d_vegs.rwater  += ed[ip].d_vegs.rwater;
      edall->d_vegs.snow    += ed[ip].d_vegs.snow;
      edall->d_vegd.rc      += ed[ip].d_vegd.rc;
      edall->d_vegd.cc      += ed[ip].d_vegd.cc;
      edall->d_vegd.btran   += ed[ip].d_vegd.btran;
      edall->d_vegd.m_ppfd  += ed[ip].d_vegd.m_ppfd;
      edall->d_vegd.m_vpd   += ed[ip].d_vegd.m_vpd;
      edall->d_a2v.rnfl     += ed[ip].d_a2v.rnfl;
      edall->d_a2v.rinter   += ed[ip].d_a2v.rinter;
      edall->d_a2v.snfl     += ed[ip].d_a2v.snfl;
      edall->d_a2v.sinter   += ed[ip].d_a2v.sinter;
      edall->d_a2v.swdown   += ed[ip].d_a2v.swdown;
      edall->d_a2v.swinter  += ed[ip].d_a2v.swinter;
      edall->d_a2v.pardown  += ed[ip].d_a2v.pardown;
      edall->d_a2v.parabsorb+= ed[ip].d_a2v.parabsorb;
      edall->d_v2a.swrefl   += ed[ip].d_v2a.swrefl;
      edall->d_v2a.evap     += ed[ip].d_v2a.evap;
      edall->d_v2a.tran     += ed[ip].d_v2a.tran;
      edall->d_v2a.evap_pet += ed[ip].d_v2a.evap_pet;
      edall->d_v2a.tran_pet += ed[ip].d_v2a.tran_pet;
      edall->d_v2a.sublim   += ed[ip].d_v2a.sublim;
      edall->d_v2g.swthfl   += ed[ip].d_v2g.swthfl;
      edall->d_v2g.rdrip    += ed[ip].d_v2g.rdrip;
      edall->d_v2g.rthfl    += ed[ip].d_v2g.rthfl;
      edall->d_v2g.sdrip    += ed[ip].d_v2g.sdrip;
      edall->d_v2g.sthfl    += ed[ip].d_v2g.sthfl;
    }
  }
}

// integrating 'veg' and 'ground' portion in 'edall' as 'land (community)'
// Note: this 'l2a' is monthly/yearly integrated in
//         'ed->atm_endofDay/_endofMonth'
void Cohort::getEd4land_daily() {
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.d_veg.vegcov[ip]>0.) {
      ed[ip].d_l2a.eet = ed[ip].d_v2a.evap + ed[ip].d_v2a.sublim
                         + ed[ip].d_v2a.tran +ed[ip].d_snw2a.sublim
                         + ed[ip].d_soi2a.evap;

      ed[ip].d_l2a.pet = ed[ip].d_v2a.evap_pet + ed[ip].d_v2a.sublim
                         + ed[ip].d_v2a.tran_pet + ed[ip].d_snw2a.sublim
                         + ed[ip].d_soi2a.evap_pet;
    }
  }

  //
  edall->d_l2a.eet = edall->d_v2a.evap + edall->d_v2a.sublim + edall->d_v2a.tran
                     + edall->d_snw2a.sublim + edall->d_soi2a.evap;

  edall->d_l2a.pet = edall->d_v2a.evap_pet + edall->d_v2a.sublim
                     + edall->d_v2a.tran_pet + edall->d_snw2a.sublim
                     + edall->d_soi2a.evap_pet;
}

// assign 'ground' portion in 'bdall' to each PFT's 'bd'
void Cohort::assignSoilBd2pfts_monthly() {
  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.m_veg.vegcov[ip]>0.) {
      bd[ip].m_sois   = bdall->m_sois;
      bd[ip].m_soid   = bdall->m_soid;
      bd[ip].m_soi2l  = bdall->m_soi2l;
      bd[ip].m_soi2a  = bdall->m_soi2a;
      bd[ip].m_a2soi  = bdall->m_a2soi;
      bd[ip].m_soi2soi= bdall->m_soi2soi;
      // monthly update annual accumulators
      bd[ip].y_sois   = bdall->y_sois;
      bd[ip].y_soid   = bdall->y_soid;
      bd[ip].y_soi2l  = bdall->y_soi2l;
      bd[ip].y_soi2a  = bdall->y_soi2a;
      bd[ip].y_a2soi  = bdall->y_a2soi;
      bd[ip].y_soi2soi= bdall->y_soi2soi;

      for (int il=0; il<MAX_SOI_LAY; il++) {
        bd[ip].prvltrfcnque[il] = bdall->prvltrfcnque[il];
      }
    }
  }
}

// integrating (vegfrac weighted) 'veg' portion in 'bdall' to all PFT's 'bd'
void Cohort::getBd4allveg_monthly() {
  for (int i=0; i<NUM_PFT_PART; i++) {
    bdall->m_vegs.c[i]    = 0.;
    bdall->m_vegs.strn[i] = 0.;
    bdall->m_a2v.ingpp[i] = 0.;
    bdall->m_a2v.innpp[i] = 0.;
    bdall->m_a2v.gpp[i]   = 0.;
    bdall->m_a2v.npp[i]   = 0.;
    bdall->m_v2a.rg[i]    = 0.;
    bdall->m_v2a.rm[i]    = 0.;
    bdall->m_v2v.nmobil[i]  = 0.;
    bdall->m_v2v.nresorb[i] = 0.;
    bdall->m_v2soi.ltrfalc[i] = 0.; //excluding moss/lichen, for which 'litterfalling' really means moss/lichen death
    bdall->m_v2soi.ltrfaln[i] = 0.;
    bdall->m_soi2v.snuptake[i] = 0.;
  }

  bdall->m_vegs.call    = 0.;
  bdall->m_vegs.labn    = 0.;
  bdall->m_vegs.strnall = 0.;
  bdall->m_vegs.nall    = 0.;
  bdall->m_a2v.ingppall = 0.;
  bdall->m_a2v.innppall = 0.;
  bdall->m_a2v.gppall   = 0.;
  bdall->m_a2v.nppall   = 0.;
  bdall->m_v2a.rgall    = 0.;
  bdall->m_v2a.rmall    = 0.;
  bdall->m_v2soi.d2wdebrisc = 0.;
  bdall->m_v2soi.d2wdebrisn = 0.;
  bdall->m_v2soi.ltrfalcall = 0.;  // excluding moss/lichen
  bdall->m_v2soi.ltrfalnall = 0.;  // excluding moss/lichen
  bdall->m_v2soi.mossdeathc = 0.;
  bdall->m_v2soi.mossdeathn = 0.;
  bdall->m_v2v.nmobilall  = 0.;
  bdall->m_v2v.nresorball = 0.;
  bdall->m_soi2v.innuptake = 0.;

  for (int il=0; il<MAX_SOI_LAY; il++) {
    bdall->m_soi2v.nextract[il] = 0.;
  }

  bdall->m_soi2v.lnuptake   = 0.;
  bdall->m_soi2v.snuptakeall= 0.;

  for (int ip=0; ip<NUM_PFT; ip++) {
    if (cd.m_veg.vegcov[ip]>0.) {
      bdall->m_v2soi.d2wdebrisc += bd[ip].m_v2soi.d2wdebrisc;
      bdall->m_v2soi.d2wdebrisn += bd[ip].m_v2soi.d2wdebrisn;

      for (int i=0; i<NUM_PFT_PART; i++) {
        bdall->m_vegs.c[i]    += bd[ip].m_vegs.c[i];
        bdall->m_vegs.strn[i] += bd[ip].m_vegs.strn[i];
        bdall->m_a2v.ingpp[i] += bd[ip].m_a2v.ingpp[i];
        bdall->m_a2v.innpp[i] += bd[ip].m_a2v.innpp[i];
        bdall->m_a2v.gpp[i]   += bd[ip].m_a2v.gpp[i];
        bdall->m_a2v.npp[i]   += bd[ip].m_a2v.npp[i];
        bdall->m_v2a.rg[i]    += bd[ip].m_v2a.rg[i];
        bdall->m_v2a.rm[i]    += bd[ip].m_v2a.rm[i];
        bdall->m_v2v.nmobil[i] += bd[ip].m_v2v.nmobil[i];
        bdall->m_v2v.nresorb[i]+= bd[ip].m_v2v.nresorb[i];

        if (cd.m_veg.nonvascular[ip]==0) {
          bdall->m_v2soi.ltrfalc[i] += bd[ip].m_v2soi.ltrfalc[i];
          bdall->m_v2soi.ltrfaln[i] += bd[ip].m_v2soi.ltrfaln[i];
        }

        bdall->m_soi2v.snuptake[i] += bd[ip].m_soi2v.snuptake[i];
      }

      bdall->m_vegs.labn    += bd[ip].m_vegs.labn;
      bdall->m_vegs.call    += bd[ip].m_vegs.call;
      bdall->m_vegs.strnall += bd[ip].m_vegs.strnall;
      bdall->m_vegs.nall    += bd[ip].m_vegs.nall;
      bdall->m_a2v.ingppall += bd[ip].m_a2v.ingppall;
      bdall->m_a2v.innppall += bd[ip].m_a2v.innppall;
      bdall->m_a2v.gppall   += bd[ip].m_a2v.gppall;
      bdall->m_a2v.nppall   += bd[ip].m_a2v.nppall;
      bdall->m_v2a.rgall    += bd[ip].m_v2a.rgall;
      bdall->m_v2a.rmall    += bd[ip].m_v2a.rmall;

      if (cd.m_veg.nonvascular[ip]==0) {
        bdall->m_v2soi.ltrfalcall += bd[ip].m_v2soi.ltrfalcall;
        bdall->m_v2soi.ltrfalnall += bd[ip].m_v2soi.ltrfalnall;
      }

      if (cd.m_veg.nonvascular[ip]>0) {
        //NOTE: non-vascular plants' litterfalling
        //  (mortality) is for death moss layer C
        bdall->m_v2soi.mossdeathc += bd[ip].m_v2soi.mossdeathc;
        bdall->m_v2soi.mossdeathn += bd[ip].m_v2soi.mossdeathn;
      }

      bdall->m_v2v.nmobilall += bd[ip].m_v2v.nmobilall;
      bdall->m_v2v.nresorball += bd[ip].m_v2v.nresorball;
      bdall->m_soi2v.innuptake+= bd[ip].m_soi2v.innuptake;

      for (int il=0; il<cd.m_soil.numsl; il++) {
        bdall->m_soi2v.nextract[il] += bd[ip].m_soi2v.nextract[il];
      }

      bdall->m_soi2v.lnuptake += bd[ip].m_soi2v.lnuptake;
      bdall->m_soi2v.snuptakeall += bd[ip].m_soi2v.snuptakeall;
    } // end of 'vegcov[ip]>0'
  }

  // below litter-fall vertical distribution needed to integrate from each PFT's
  double sumrtltrfall = 0.;

  for (int il=0; il<cd.m_soil.numsl; il++) {
    bdall->m_v2soi.rtlfalfrac[il] = 0.;

    for (int ip=0; ip<NUM_PFT; ip++) {
      if (cd.m_veg.vegcov[ip]>0.) {
        bd[ip].m_v2soi.rtlfalfrac[il] = cd.m_soil.frootfrac[il][ip];
        bdall->m_v2soi.rtlfalfrac[il] += bd[ip].m_v2soi.rtlfalfrac[il]
                                         * bd[ip].m_v2soi.ltrfalc[I_root];
      }
    }

    sumrtltrfall +=bdall->m_v2soi.rtlfalfrac[il];
  }

  for (int il=0; il<cd.m_soil.numsl; il++) {
    if (sumrtltrfall>0) {
      bdall->m_v2soi.rtlfalfrac[il] /=sumrtltrfall;
    } else {
      bdall->m_v2soi.rtlfalfrac[il] = 0.;
    }
  }
}



//void Cohort::NEW_load_fire_from_file(int y, int x) {
//  BOOST_LOG_SEV(glg, debug) << "New-style fire data reading function...";
//  
//        //    // 1) for eq and sp we need a fire recurrance interval
//        //  // varies with space and *NOT* time
//        //  // generated by some statictical munging of the SNAP Fire History tif files
//        //  // will result in a single band tiff
//        //  int fri;
//        //
//        //  // 2) for tr and sc FRI is based on ALF outputs or historical records
//        //  // (for AK from 1952) year_of_burn - explicit replacement for FRI in tr and
//        //  // sc (maybe from ALF)
//        //  std::vector<int> fire_years;
//        //  std::vector<int> fire_sizes;
//  std::string fire_dataset = "scripts/new-fire-dataset.nc";
//  
//  BOOST_LOG_SEV(glg, info) << "Loading fire from file: " << fire_dataset;
//  BOOST_LOG_SEV(glg, info) << "Loading fire for (y, x) point: ("<< y <<","<< x <<").";
//
//  this->cd.fri = temutil::get_fri(fire_dataset, y, x);
//  
////  this->cd->fire_years = temutil::get_fire_years(fire_dataset, y, x);
////  
////  this->cd->fire_sizes = temutil::get_fire_sizes(fire_dataset, y, x);
//  
//}
//
//
//void Cohort::NEW_load_veg_class_from_file(int y, int x) {
//  BOOST_LOG_SEV(glg, debug) << "New-style veg data reading function.";
//
//  std::string veg_dataset = "scripts/new-veg-dataset.nc";
//
//  BOOST_LOG_SEV(glg, info) << "Loading vegetation from file: " << veg_dataset;
//  BOOST_LOG_SEV(glg, info) << "Loading vegetation for (y, x) point: (" << y <<","<< x <<").";
//
//  cd.vegtype[0] = temutil::get_veg_class(veg_dataset, y, x);
//  
//  // Not sure exactly how these are used yet...
//  // setting to to dummy values...
//  cd.vegyear[0] = -9999.0;
//  cd.vegfrac[0] = 1.0;
//
//}
