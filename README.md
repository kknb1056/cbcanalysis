cbcanalysis
===========

CMSSW code to analyse data for the CBC, and python scripts for the xdaq readout.
The directory structure is setup so that it can be included in CMSSW under "SLHCUpgradeTracker". To include in CMSSW:

    cd $CMSSW_BASE/src
    git clone git@github.com:mark-grimes/cbcanalysis.git SLHCUpgradeTracker/CBCAnalysis

Some directories are hard coded however, I can't figure out how to get XDAQ to use environment variables for paths in the configuration file. You'll need to change runcontrol/analysisTest.xml to reflect wherever your CMSSW_BASE is. Look for the line where the variable "parameterSet" is set for the FUEventProcessor.
