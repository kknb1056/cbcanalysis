import os, inspect
# The "os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))" part of
# this line gets the directory of this file. I then look three parents up to get the directory
# of the CBCAnalysis installation.
INSTALLATION_PATH = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe()))), os.pardir, os.pardir))

import XDAQTools, re
from I2cChip import I2cChip

class GlibSupervisorApplication( XDAQTools.Application ) :
	def __init__( self, host=None, port=None, className=None, instance=None, I2cRegisterDirectory=INSTALLATION_PATH+"/runcontrol/i2c" ) :
		# Because there's a chance I might reassign the class of a base Application instance to this
		# class, I'll check and see if the base has been initialised before calling the super class
		# constructor.
		if not hasattr( self, "host" ) : super(GlibSupervisorApplication,self).__init__( host, port, className, instance )

		self._directoryForI2C=I2cRegisterDirectory
		# I2C parameters have to be saved to a file, and then the GlibSupervisor told to send the
		# file to the board. This is the temporary directory I'll use to store the files.
		# Have a lot of problems if I don't have permission to write to this directory, so try
		# and make the directory unique to the user.
		try: usernameSuffix="_"+os.environ['USER']
		except KeyError: usernameSuffix=""
		self.tempDirectory="/tmp/cbcTestStandTempFiles"+usernameSuffix+"/supervisor"
		try :
			os.makedirs( self.tempDirectory )
		except OSError as error:
			# Acceptable if the directory already exists, but any other error is a problem
			if error.args[1] != 'File exists' : raise
		
		# Note that because of the way the GlibSupervisor is coded, if some of these are missing
		# the supervisor will crash. So always make sure all of these are included in the POST
		# request, even if they're already set at the required values.
		self.parameters = {
			'user_wb_ttc_fmc_regs_pc_commands_TRIGGER_SEL':'off',  # turn off triggering from TTC
			'user_wb_ttc_fmc_regs_pc_commands_INT_TRIGGER_FREQ':5, # 4 corresponds to 16Hz. Look on the webconfig to see the other values
			'user_wb_ttc_fmc_regs_pc_commands2_FE0_masked':'off',
			'user_wb_ttc_fmc_regs_pc_commands2_FE1_masked':'off',
			'user_wb_ttc_fmc_regs_pc_commands_ACQ_MODE':'on',      # Continuous storage
			'user_wb_ttc_fmc_regs_pc_commands_CBC_DATA_GENE':'on', # External data
			'user_wb_ttc_fmc_regs_pc_commands2_negative_logic_CBC':'on'
		}
		self.saveParametersResource = "/urn:xdaq-application:lid="+str(self.id)+"/saveParameters"
		## Parameters and resource for reading an I2C address file
		self.readI2cParameters = { 'i2CFile':I2cRegisterDirectory+"/FE0CBC0.txt" }
		self.readI2cResource = "/urn:xdaq-application:lid="+str(self.id)+"/i2cRead"
		# The write I2C request takes no parameters, the filename has to be set
		# before hand with a read I2C request (not very RESTful but hey ho).
		self.writeI2cParameters = {}
		self.writeI2cResource = "/urn:xdaq-application:lid="+str(self.id)+"/i2cWriteFileValues"

		# I can only do the setup for the CBCs once this application has gone to the configured
		# state. I'll have to check this parameter in all methods and configure if it hasn't been
		# done yet.
		self._connectedCBCsHaveBeenInitialised=False
		# I expect this call to fail, but there's a chance the XDAQ process is already running
		# and this python representation of it is just re-connecting to it.
		try:
			self._initConnectedCBCs()
		except:
			pass

	def _initConnectedCBCs( self ) :
		"""
		Initialise information about the connected CBCs. This is only possible after this
		application has been sent the "Initialise" message, which means the XDAQ process
		must have already been started.
		"""
		# First need to know which FMCs are connected. The only place I've found
		# this information is available is in the main webpage, and it is only
		# available there after moving to the "Initialised" state. Otherwise I
		# could do this during __init__.
		
		try: 
			webpage = self.httpRequest( "GET", '/urn:xdaq-application:lid='+str(self.id), {}, True )
		except:
			# The XDAQ process probably hasn't been started. Replace the 'connection refused' error
			# with one that is more discriptive to the user.
			raise Exception("Couldn't connect to the XDAQ process to get information about the connected CBCs. Is the XDAQ process running and GlibSupervisor initialised?")
		# Perform a regular expression search on the HTML to see what the
		# state of each FMC is. I do this by looking at the alt text.
		fmcState=re.findall( """(?<=alt=')\w+(?='/> FMC 1<)""", webpage.fullMessage )
		if len(fmcState)!=1 : raise Exception( "Failed when querying which FMCs are connected. This information is only available after GlibSupervisor has been initialised.")
		elif fmcState[0]=='ON' : self.isFMC1Connected=True
		elif fmcState[0]=='OFF' : self.isFMC1Connected=False
		else : raise Exception( "Failed when querying which FMCs are connected. FMC 1 reported '"+fmcState[0]+"' which is not equal to either 'ON' or 'OFF'.")
		# Do the same for FMC 2
		fmcState=re.findall( """(?<=alt=')\w+(?='/> FMC 2<)""", webpage.fullMessage )
		if len(fmcState)!=1 : raise Exception( "Failed when querying which FMCs are connected. This information is only available after GlibSupervisor has been initialised.")
		elif fmcState[0]=='ON' : self.isFMC2Connected=True
		elif fmcState[0]=='OFF' : self.isFMC2Connected=False
		else : raise Exception( "Failed when querying which FMCs are connected. FMC 2 reported '"+fmcState[0]+"' which is not equal to either 'ON' or 'OFF'.")
		
		self._connectedCBCsHaveBeenInitialised=True
		
		self.i2cChips = {}
		if self.isFMC1Connected :
			self.i2cChips['FE1CBC0'] = I2cChip()
			self.i2cChips['FE1CBC1'] = I2cChip()
		if self.isFMC2Connected :
			self.i2cChips['FE0CBC0'] = I2cChip()
			self.i2cChips['FE0CBC1'] = I2cChip()

		# Try and load the default parameters
		failedChipNames = []
		for chipName in self.i2cChips.keys() :
			try :
				self.i2cChips[chipName].loadFromFile(self._directoryForI2C+'/'+chipName+'.txt')
			except :
				failedChipNames.append(chipName)

		if len(failedChipNames)!=0 : raise Exception( "Couldn't load values for chips: "+str(failedChipNames) )
		

	def connectedCBCNames(self) :
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		return self.i2cChips.keys()
	
	def I2CRegisterValues( self, chipNames=None, registerNames=None ) :
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		if chipNames==None : cbcNames=self.connectedCBCNames()
		else : cbcNames=chipNames
		returnValue = {}
		for name in cbcNames :
			returnValue[name]=self.i2cChips[name].getValues(registerNames)
		return returnValue


	def setConfigureParameters( self, triggerRate=None ) :
		"""
		Configure the parameters on the GLIB with the values required for data taking.
		You can optionally set a trigger rate in Hz which will be rounded down to the
		nearest power of 2.
		Note that these parameters aren't sent to the board unti the "Configure" state
		transition.
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		if triggerRate!=None :
			triggerRateCode = int( math.log( triggerRate, 2 ) )
			self.parameters['user_wb_ttc_fmc_regs_pc_commands_INT_TRIGGER_FREQ']=triggerRateCode
		response=self.httpRequest( "POST", self.saveParametersResource, self.parameters, False )
		if response.status!= 200 : raise Exception( "GlibSupervisor.configure got the response "+str(response.status)+" - "+response.reason )

	def setAllChannelTrims( self, value ) :
		"""
		Sets the trim for all channels. Note that this isn't written to the board until sendI2C is called
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		for channel in range(0,254) :
			self.setChannelTrim( channel, value )
	
	def setChannelTrim( self, channel, value, chipNames=None ) :
		"""
		Sets the trim for the specified channel. Note that this isn't written to the board until sendI2C is called.
		By default acts on all CBCs, but this can be limited by specifying an array for 'chipNames'.
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		if chipNames==None : chipNames=self.i2cChips.keys()
		for name in chipNames :
			self.i2cChips[name].setChannelTrim( channel, value )

	def getChannelTrim( self, channel, chipNames=None ) :
		"""
		Returns a map of the channel trim for each chip. If only one chip is requested
		returns the value instead of a map.
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		if chipNames==None : chipNames=self.i2cChips.keys()
		result = {}
		for name in chipNames :
			result[name]=self.i2cChips[name].getChannelTrim( channel )
		if len(chipNames)==1 : return result[chipNames[0]] # Don't bother with a map if it's only one chip
		else : return result

	def setI2c( self, registerNameValueTuple, chipNames=None ) :
		"""
		Sets the registers named by the keys in the tuple to the values in the tuple. By default
		does so for all connected CBCs, but this can be limited by specifying an array for 'chipNames'.
		
		Note this doesn't set the register on the chip, only in the internal representation. Changes
		will not be sent to the chip until sendI2c is called.
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		
		if chipNames==None : chipNames=self.i2cChips.keys()
		for name in chipNames :
			chip = self.i2cChips[name]
			for name in registerNameValueTuple :
				register = chip.getRegister(name)
				register.value = registerNameValueTuple[name]

	def sendI2c( self, registerNames=None, chipNames=None ) :
		"""
		Sends all of the I2C registers to the chips by writing to temporary files and asking the
		GlibSupervisor to send these files. By default does so for all connected CBCs, but this
		can be limited by specifying an array for 'chipNames'.
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		
		self.saveI2c( self.tempDirectory, registerNames, chipNames )
		self.sendI2cFilesFromDirectory( self.tempDirectory )

	def saveI2c( self, directoryName, registerNames=None, chipNames=None ) :
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		# First make sure there are no files left over from previous sends. Allow an error
		# of 'No such file or directory' but throw any other exceptions.
		for filename in ["FE0CBC0.txt","FE0CBC1.txt","FE1CBC0.txt","FE1CBC1.txt"] :
			try :
				os.remove( os.path.join(self.tempDirectory,filename) )
			except Exception as error :
				if error.args[1] != 'No such file or directory' : raise
		# make sure the directory exists
		try:
			os.makedirs( directoryName )
		except:
			pass
		
		if chipNames==None : chipNames=self.i2cChips.keys()
		for name in chipNames :
			self.i2cChips[name].writeToFilename( os.path.join(directoryName,name+".txt"), registerNames )

	def loadI2c( self, directoryName ) :
		for chipName in self.i2cChips.keys() :
			self.i2cChips[chipName].loadFromFile(directoryName+'/'+chipName+'.txt')

	def sendI2cFilesFromDirectory( self, directoryName ) :
		"""
		Asks the GlibSupervisor to send the files in the given directory to the CBC registers.
		There can be up to four CBCs connected - 2 on each of 2 FEs depending on how many FMCs
		are connected. The files in the directory have to be named "FE<number>CBC<number>.txt"
		with the numbers being either 0 or 1. This is hard coded into the GlibSupervisor.
		
		This method will look at the contents of the directory and tell the GlibSupervisor to
		load all the files that match the required filename style. So if you don't want to send
		anything to a particular CBC don't have the file in the directory.
		
		The directory can only be specified in a "read" call, so a read is sent first to set the
		directory name, then the write is sent.
		
		Note that this method completely bypasses the I2C representation held internally by this
		class.
		"""
		if not self._connectedCBCsHaveBeenInitialised : self._initConnectedCBCs()
		
		self.writeI2cParameters = {} # clear this of any previous entries
		# Note that for some reason, the GlibSupervisor calls FMC1 "FE1" and FMC2 "FE0".
		if self.isFMC1Connected :
			if os.path.isfile( directoryName+"/FE1CBC0.txt" ) : self.writeI2cParameters["chkFE1CBC0"]="on"
			if os.path.isfile( directoryName+"/FE1CBC1.txt" ) : self.writeI2cParameters["chkFE1CBC1"]="on"
		if self.isFMC2Connected :
			if os.path.isfile( directoryName+"/FE0CBC0.txt" ) : self.writeI2cParameters["chkFE0CBC0"]="on"
			if os.path.isfile( directoryName+"/FE0CBC1.txt" ) : self.writeI2cParameters["chkFE0CBC1"]="on"
		
		if len( self.writeI2cParameters ) == 0 :
			raise Exception( "GlibSupervisor.sendI2cFilesFromDirectory was given a directory name containing no files with the required names (e.g. \"FE0CBC0.txt\")" )

		# First need to set the directory name in the GlibSupervisor. The only way to do this is to
		# call a "read" first.
		self.readI2cParameters['i2CFiles']=directoryName
		response=self.httpRequest( "POST", self.readI2cResource, self.readI2cParameters, False )
		if response.status!= 200 : raise Exception( "GlibSupervisor.sendI2cFile during read got the response "+str(response.status)+" - "+response.reason )

		# Now the directory has been set I can tell GlibSupervisor to write the files in it
		response=self.httpRequest( "POST", self.writeI2cResource, self.writeI2cParameters, True )
		if response.status!= 200 : raise Exception( "GlibSupervisor.sendI2cFile during write got the response "+str(response.status)+" - "+response.reason )

