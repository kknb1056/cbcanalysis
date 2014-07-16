"""
@brief Hardcode filepaths and system variables

This is an attempt to put all system dependant variables in one place. All
scripts (once converted) will source this script and use the definitions in
here. That way there will only be one central place with hard coded constants
to get the scripts to run.

You will need to modify this file for the paths on your system.

@author Mark Grimes (mark.grimes@bristol.ac.uk)
@date 11/Feb/2014
"""

def getEnvironmentVariables() :
	CMSSW_BASE="/home/xtaldaq/CBCAnalyzer/CMSSW_5_3_4"
	CMSSW_RELEASE_BASE="/home/xtaldaq/cmssw/slc5_amd64_gcc462/cms/cmssw/CMSSW_5_3_4"
	SCRAM_ARCH="slc5_amd64_gcc462"
	XDAQ_ROOT="/opt/xdaq"
	XDAQ_DOCUMENT_ROOT="/opt/xdaq/htdocs"
	XDAQ_OS="linux"
	XDAQ_PLATFORM="x86"
	ROOTSYS="/home/xtaldaq/cmssw/slc5_amd64_gcc462/lcg/root/5.32.00-cms17/"
	SCRATCH="/tmp"
	USER="xtaldaq"  # This is the user that XDAQ will start the processes as
	# These are added before and after LD_LIBRARY_PATH, respectively
	prefix_LD_LIBRARY_PATH=""
	suffix_LD_LIBRARY_PATH="/home/xtaldaq/cmssw/slc5_amd64_gcc462/external/gcc/4.6.2/lib64:/home/xtaldaq/cmssw/slc5_amd64_gcc462/external/gcc/4.6.2/lib"
	# Not used as an environment variables themselves, but used to set LD_LIBRARY_PATH
	cactus_root="/opt/cactus"

	# Only the lines above should be changed.

	environmentVariables={}
	environmentVariables['CMSSW_BASE']=CMSSW_BASE
	environmentVariables['CMSSW_RELEASE_BASE']=CMSSW_RELEASE_BASE
	environmentVariables['SCRAM_ARCH']=SCRAM_ARCH
	environmentVariables['XDAQ_ROOT']=XDAQ_ROOT
	environmentVariables['XDAQ_DOCUMENT_ROOT']=XDAQ_DOCUMENT_ROOT
	environmentVariables['XDAQ_OS']=XDAQ_OS
	environmentVariables['XDAQ_PLATFORM']=XDAQ_PLATFORM
	environmentVariables['ROOTSYS']=ROOTSYS
	environmentVariables['SCRATCH']=SCRATCH
	environmentVariables['USER']=USER
	environmentVariables['PATH']=XDAQ_ROOT+"/bin:"+cactus_root+"/bin:"+CMSSW_BASE+"/bin/"+SCRAM_ARCH+":"+CMSSW_RELEASE_BASE+"/bin/"+SCRAM_ARCH
	environmentVariables['LD_LIBRARY_PATH']=prefix_LD_LIBRARY_PATH+":"+XDAQ_ROOT+"/lib:"+cactus_root+"/lib:"+CMSSW_BASE+"/lib/"+SCRAM_ARCH+":"+CMSSW_BASE+"/external/"+SCRAM_ARCH+"/lib:"+CMSSW_RELEASE_BASE+"/external/"+SCRAM_ARCH+"/lib:"+CMSSW_RELEASE_BASE+"/lib/"+SCRAM_ARCH+":"+suffix_LD_LIBRARY_PATH
	

	return environmentVariables