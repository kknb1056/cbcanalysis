from pyjamas.Timer import Timer

from GlibRPCService import GlibRPCService
from ErrorMessage import ErrorMessage

class AlreadyTakingDataError(Exception) :
	"""
	Exception raised when a request is made to start taking data, but a run is
	already in progress.
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 08/Feb/2014
	"""
	def __init__( self, message=None ) :
		if message==None : message=""
		else : message=" "+message # Add a space between the main message and the additional one
		Exception.__init__(self, "Data taking is already in progress"+message )
	
class DataRunManager(object) :
	"""
	@brief Class to take care of all the data taking runs. Starts them and
	polls the RPC service to see when it's finished.
	
	Implemented as a singleton. Call the DataRunManager.instance() static
	method to get the only running instance.
	"""
	pollingTime=1000
	idlePollingTime=4000 # Longer polling time when I don't think I'm taking data.
	_onlyInstance=None
	# The following members are used as an enum to describe what the event is
	DataTakingStartedEvent=0
	DataTakingFinishedEvent=1
	DataTakingStatusEvent=3
	
	@staticmethod
	def instance() : # static method so no "self" parameter
		if DataRunManager._onlyInstance==None :
			DataRunManager._onlyInstance=DataRunManager()
		return DataRunManager._onlyInstance
	
	def __init__(self) :
		# Since this is a singleton, throw an exception if the constructor is called
		# explicitly.
		if DataRunManager._onlyInstance!=None :
			raise Exception( "DataRunManager is a singleton class. Call 'DataRunManager.instance()' to get the only running instance" )
		self.rpcService=GlibRPCService.instance()
		self.pollingTimer=Timer( notify=self.pollRPCService )
		self.eventHandlers = []
		self.fractionComplete = 1
		self.statusString = "Not taking data"
		self.firstRun = True
		# I'll poll once immediately on startup just in case the user is connecting to an
		# already running service. The code that handles the response to this starts the
		# timer running.
		self.pollRPCService()

	def onRemoteResponse(self, response, request_info):
		"""
		The method that gets called after a successful RPC call.
		"""
		#ErrorMessage( "Response to method '"+request_info.method+"'='"+str(response)+"'" )
		if request_info.method=="getDataTakingStatus" : self._onGetDataTakingStatusResponse(response)
		elif request_info.method=="startSCurveRun" : self._startRunResponse(response,"S-curve run")
		elif request_info.method=="startOccupancyCheck" : self._startRunResponse(response,"Occupancy check")
		elif request_info.method=="startTrimCalibration" : self._startRunResponse(response,"Trim calibration")
		elif request_info.method=="stopTakingData" : pass
		else : ErrorMessage( "Received an unexpected response for method "+request_info.method )
	
	def onRemoteError(self, code, message, request_info):
		"""
		The method that gets called after an unsuccessful RPC call.
		"""
		ErrorMessage( "Unable to contact server: "+str(message) )

	def _onGetDataTakingStatusResponse( self, response ) :
		"""
		Handles the response to a getDataTakingStatus RPC call. Separate method for code layout only.
		"""
		newFraction=response["fractionComplete"]
		newStatus=response["statusString"]
		statusHasChanged=False
		if (self.fractionComplete!=newFraction) or (self.statusString!=newStatus) :
			statusHasChanged=True
			if self.firstRun :
				# If something is already running on initialisation, tell everything
				for handler in self.eventHandlers :
					handler.onDataTakingEvent( DataRunManager.DataTakingStartedEvent, None )
		self.firstRun=False
		self.fractionComplete=newFraction
		self.statusString=newStatus
		# only want to inform the listening classes if there is a change in the status
		if statusHasChanged :
			if self.fractionComplete>=1 :
				eventCode=DataRunManager.DataTakingFinishedEvent
				details=None
			else :
				eventCode=DataRunManager.DataTakingStatusEvent
				details={"fractionComplete":self.fractionComplete,"statusString":self.statusString}
			# Inform all the registered handlers what is going on
			for handler in self.eventHandlers :
				handler.onDataTakingEvent( eventCode, details )

		if self.fractionComplete<1 :
			# If data hasn't finished then set the timer to fire again.
			self.pollingTimer.schedule( DataRunManager.pollingTime )
		else :
			# I'll constantly poll to make sure it's not taking data, in case something/someone
			# else connects and tells the RPC service to do something. I'll use a longer time
			# though.
			if self.idlePollingTime>0 : self.pollingTimer.schedule( DataRunManager.idlePollingTime )

	def _startRunResponse( self, reponse, runType ) :
		"""
		Handles the response to a RPC call. Separate method for code layout only.
		"""
		self.statusString=runType+" started"
		# Start polling the RPC service to see how the run is going
		self.pollingTimer.schedule( DataRunManager.pollingTime )
		# inform all registered handlers that data taking has started
		for handler in self.eventHandlers :
			handler.onDataTakingEvent( DataRunManager.DataTakingStartedEvent, None )

	def pollRPCService( self ) :
		"""
		Method that polls the RPC service to see what the state of data taking is.
		This method is bound to a Timer so that it will periodically be called.
		"""
		self.rpcService.getDataTakingStatus( None, self )

	def registerEventHandler( self, handler ) :
		"""
		Registers an event handler that will be notified when certain things happen, e.g.
		data taking started, data taking finished.
		"""
		self.eventHandlers.append( handler )
	
	def startSCurveRun( self, thresholds ) :
		if self.fractionComplete!=1 : raise AlreadyTakingDataError()
		
		self.fractionComplete=0
		self.statusString="Initiating s-curve run"
		self.rpcService.startSCurveRun( thresholds, self )
	
	def startOccupancyCheck( self ) :
		if self.fractionComplete!=1 : raise AlreadyTakingDataError()
		
		self.fractionComplete=0
		self.statusString="Initiating occupancy check"
		self.rpcService.startOccupancyCheck( None, self )

	def startTrimCalibration( self,  midPointTarget, maxLoops ) :
		if self.fractionComplete!=1 : raise AlreadyTakingDataError()
		
		self.fractionComplete=0
		self.statusString="Initiating trim calibration"
		self.rpcService.startTrimCalibration( {'midPointTarget':midPointTarget,'maxLoops':maxLoops}, self )
		
	def stopTakingData( self ) :
		self.rpcService.stopTakingData( None, self )
