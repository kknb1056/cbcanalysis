import XDAQTools, re

class GlibStreamerApplication( XDAQTools.Application ) :
	def __init__( self, host=None, port=None, className=None, instance=None ) :
		# Because there's a chance I might reassign the class of a base Application instance to this
		# class, I'll check and see if the base has been initialised before calling the super class
		# constructor.
		if not hasattr( self, "host" ) : super(GlibStreamerApplication,self).__init__( host, port, className, instance )
		# Now do the stuff specific to this subclass
		self.parameters = {
			'destination':'/tmp/scriptedRun.dat',
			'sharedMem':'off',    # pass the data to the RU
			'memToFile':'on',   # don't dump to a file
			'nbAcq':100, # The number of events to take. 100 is an arbitrary testing value.
			'acqMode':1, # The data format. 1 means "Full debug"; 3 means "Old format"
			'zeroSuppressed':'off'
		}
		self.headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
		self.saveParametersResource = "/urn:xdaq-application:lid="+str(self.id)+"/validParam"
		self.forceStartResource = "/urn:xdaq-application:lid="+str(self.id)+"/forceStartXgi"

	def getState(self) :
		"""
		Override of base XDAQTools.Application because GlibStreamer doesn't report it's state
		in the answer to a "ParameterQuery". This is a massive hack because it's the only way
		I know to get the state.
		"""
		try:
			# The streamer doesn't have any soap commands to query to the state of the acquisition. The
			# only external way I can find to see if data is being recorded is to check the html status
			# page. There's not a specific status display, but depending on the information shown the
			# status can be inferred.
			response=self.httpRequest( "GET", "/urn:xdaq-application:lid="+str(self.id) )
		except :
			self.connection.close() # Just in case something left it open and this method is being polled
			return "<uncontactable>"		
		try:
			# The only way I've figured out how to get this information is by using some
			# hard coded knowledge about where it's stored
			streamerState=response.fullMessage.splitlines()[19].split('>')[1].split('<')[0]
			return streamerState
		except: return "<unknown>"

	def setConfigureParameters( self, numberOfEvents=None ) :
		"""
		Sets the parameters ready for configuration. Note that these aren't sent to the board until
		the "Configure" state transition.
		"""
		if numberOfEvents!=None : self.parameters['nbAcq']=numberOfEvents
		response=self.httpRequest( "POST", self.saveParametersResource, self.parameters, False )
		if response.status!= 200 : raise Exception( "GlibStreamer.configure got the response "+str(response.status)+" - "+response.reason )

	def setOutputFilename( self, filename ) :
		"""
		This sets the filename of the DAQ output dump. Note that this change doesn't take effect
		until the next "Configure" state change (I think).
		"""
		self.parameters['destination']=filename
		response=self.httpRequest( "POST", self.saveParametersResource, self.parameters, False )
		if response.status!= 200 : raise Exception( "GlibStreamer.setOutputFilename got the response "+str(response.status)+" - "+response.reason )

	def setNumberOfEvents( self, numberOfEvents ) :
		"""
		The number of events that will be taken. Note that this change doesn't take effect
		until the next "Configure" state change (I think).
		"""
		self.parameters['nbAcq']=numberOfEvents
		response=self.httpRequest( "POST", self.saveParametersResource, self.parameters, False )
		if response.status!= 200 : raise Exception( "GlibStreamer.setNumberOfEvents got the response "+str(response.status)+" - "+response.reason )
		
	def acquisitionState(self):
		"""
		Reports whether data is being taken or not. Note that this is different to the state the
		GlibStreamer is in - it will still report "Running" after all events have been taken until
		the next state change.
		
		The streamer doesn't have any soap commands to query to the state of the acquisition. The
		only external way I can find to see if data is being recorded is to check the html status
		page. There's not a specific status display, but depending on the information shown the
		status can be inferred. This is likely to break with any change to the GlibStreamer.
		"""
		try:
			response=self.httpRequest( "GET", "/urn:xdaq-application:lid="+str(self.id) )
		except:
			return "<uncontactable>"		
		try:			
			# The only way I've figured out how to get this information is by using some
			# hard coded knowledge what the webpage shows in different states.
			streamerStateLine=response.fullMessage.splitlines()[34]
			if streamerStateLine[0:45]=="<table><tr><td><form action='saveHtmlValues'>":
				# The table to modify parameters is showing which means data is not being taken
				return "Stopped"
			elif streamerStateLine[0:33]=='<form action="pauseAcquisition" >':
				return "Running"
			else: return "<unknown>"
		except:
			return "<unknown>"

	def acquisitionStateAndEvent(self):
		"""
		Same as acquisitionState(), but returns a two element array with the state as the first element
		and either the event number as the second, or 'None' if the acquisition is not running.
		"""
		try:
			response=self.httpRequest( "GET", "/urn:xdaq-application:lid="+str(self.id) )
		except:
			return ["<uncontactable>",None]
		try:			
			# The only way I've figured out how to get this information is by using some
			# hard coded knowledge what the webpage shows in different states.
			streamerStateLine=response.fullMessage.splitlines()[34]
			if streamerStateLine[0:45]=="<table><tr><td><form action='saveHtmlValues'>":
				# The table to modify parameters is showing which means data is not being taken
				return ["Stopped",None]
			elif streamerStateLine[0:33]=='<form action="pauseAcquisition" >':
				#try :
					# To get the event number, perform a regular expression search
				event=re.findall('(?<=Acquisition number: )\w+', response.fullMessage)[0]
				#except :
				#	event="error - couldn't get the event number"
				return ["Running",event]
			else: return ["<unknown>",None]
		except:
			raise
			return ["<unknown>",None]
