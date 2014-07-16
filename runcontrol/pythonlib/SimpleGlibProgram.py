import XDAQTools
from GlibStreamerApplication import GlibStreamerApplication
from GlibSupervisorApplication import GlibSupervisorApplication

class SimpleGlibProgram( XDAQTools.Program ) :
	def __init__( self, xdaqConfigFilename ) :
		super(SimpleGlibProgram,self).__init__( xdaqConfigFilename )
		self._extendStreamerAndSupervisor()
		
	def _extendStreamerAndSupervisor( self ) :
		# The super class constructor will create all of the Context and Application instances.
		# After that I need to run through each Application and find which ones are the
		# GlibSupervisor and GlibStreamer. Once I find them, I'll change the class type to my
		# Application subclasses defined above, and keep a note of which ones they are.
		for context in self.contexts :
			for application in context.applications :
				if application.className=="GlibStreamer" :
					application.__class__=GlibStreamerApplication # Change the class type to my extension
					application.__init__() # Call the constructor. A check is made to not reinitialise the base.
					self.streamer=application # Make a note so I can access it easily later
				elif application.className=="GlibSupervisor" :
					application.__class__=GlibSupervisorApplication # Change the class type to my extension
					application.__init__() # Call the constructor. A check is made to not reinitialise the base.
					self.supervisor=application # Make a note so I can access it easily later

	def reloadXDAQConfig( self ) :
		del self.streamer
		del self.supervisor
		super(SimpleGlibProgram,self).reloadXDAQConfig()
		self._extendStreamerAndSupervisor()
	
	def initialiseCBCs( self ) :
		"""
		Initialises the state of the connected CBCs by checking which FMCs are connected.
		This means that the XDAQ process for the GlibSupervisor has to be started and
		initialised. If the process previously wasn't running it will be killed afterwards.
		"""
		if self.supervisor._connectedCBCsHaveBeenInitialised : return

		try :
			currentState=self.supervisor.getState()
			supervisorContext=None
			if currentState=="<uncontactable>" :
				# Need to find the context for the supervisor and start it. I don't
				# know which one it is though so I'll have to search through them.
				for context in self.contexts :
					for application in context.applications :
						if application==self.supervisor : supervisorContext=context
				supervisorContext.startProcess( ignoreIfCurrentlyRunning=True )
				supervisorContext.waitUntilProcessStarted()
				currentState=self.supervisor.getState()
	
			if currentState=="Initial" :
				self.supervisor.sendCommand( "Initialise" )
				self.supervisor.waitForState( "Halted" )
	
			self.supervisor._initConnectedCBCs()
	
			# If I had to start the process then I'll kill it so that it was in the state
			# it was in beforehand.
			if supervisorContext!=None : supervisorContext.killProcess()
		except :
			# If the XDAQ processes weren't running beforehand, make sure they're stopped
			# before propagating the error.
			if currentState=="<uncontactable>" : supervisorContext.killProcess()
			raise
			
		
	def initialise( self, triggerRate=None, numberOfEvents=100, timeout=5.0 ) :
		"""
		Starts the initialise process. If "timeout" is positive then control will block
		until all the applications have reached the required state, or until "timeout"
		seconds have passed.
		"""
		self.supervisor.sendCommand( "Initialise" )
		if timeout>0 : self.supervisor.waitForState( "Halted", timeout )
		
		# Now that everything is initialised, I'll set the parameters of the GlibSupervisor and
		# GlibStreamer to what I want to be the defaults. I'll set them here rather than at the
		# start of configure(..) so that the user can go into the web interface and make additional
		# changes on top. These settings aren't actually sent to the board until the streamer
		# and supervisor are sent the "Configure" command, so the user can make additional changes
		# and then call the configure(..) method.
		self.supervisor.setConfigureParameters(triggerRate)
		self.streamer.setConfigureParameters(numberOfEvents)

	def setOutputFilename( self, filename ) :
		self.streamer.setOutputFilename( filename )
		
	def setAndSendI2c( self, registerNameValueTuple, chipNames=None ) :
		self.supervisor.setI2c( registerNameValueTuple, chipNames )
		self.supervisor.sendI2c( registerNameValueTuple.keys(), chipNames )
	
	def saveI2c( self, directoryName ) :
		self.supervisor.saveI2c( directoryName )

	def loadI2c( self, directoryName ) :
		self.supervisor.loadI2c( directoryName )

	def configure( self, timeout=5.0 ) :
		self.supervisor.sendCommand( "Configure" )
		self.streamer.sendCommand( "configure" )
		
		if timeout>0 : self.supervisor.waitForState( "Configured", timeout )
		# Configuring the GlibSupervisor resets all I2C registers, so reset whatever
		# my settings are.
		self.supervisor.sendI2c()

	def stop( self, timeout=5.0 ) :
		self.streamer.sendCommand( "stop" )
		self.supervisor.sendCommand( "Stop" )

		if timeout>0 : self.supervisor.waitForState( "Configured", timeout )

	def enable( self, timeout=5.0 ) :
		self.supervisor.sendCommand( "Enable" )
		self.streamer.sendCommand( "start" )

		if timeout>0 : self.supervisor.waitForState( "Enabled", timeout )

	def halt( self, timeout=5.0 ) :
		self.supervisor.sendCommand( "Halt" )
		self.streamer.sendCommand( "halt" )

		if timeout>0 : self.supervisor.waitForState( "Halted", timeout )

	def pause( self, timeout=5.0 ) :
		self.streamer.sendCommand( "stop" )

	def play( self, timeout=5.0 ) :
		self.streamer.sendCommand( "start" )
		
