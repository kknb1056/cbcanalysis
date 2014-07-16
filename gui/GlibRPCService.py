from pyjamas.JSONService import JSONProxy

class GlibRPCService(JSONProxy):
	"""
	@brief Singleton class to make Remote Procedure Calls to the server controlling the Glib.
	
	Implemented as a singleton. To get an instance call GlibRPCService.instance()
	
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 08/Feb/2014
	"""
	_onlyInstance=None
	
	@staticmethod
	def instance() : # static method so no "self" parameter
		if GlibRPCService._onlyInstance==None :
			GlibRPCService._onlyInstance=GlibRPCService()
		return GlibRPCService._onlyInstance
	
	def __init__(self):
		# Since this is a singleton, throw an exception if the constructor is called
		# explicitly.
		if GlibRPCService._onlyInstance!=None :
			raise Exception( "GlibRPCService is a singleton class. Call 'GlibRPCService.instance()' to get the only running instance" )
		JSONProxy.__init__(self, "services/GlibControlProxy.py", ["getStates","connectedCBCNames",
			"I2CRegisterValues","setI2CRegisterValues","saveI2cRegisterValues","loadI2cRegisterValues",
			"startProcesses","killProcesses","boardIsReachable",
			"stopTakingData","startSCurveRun","startOccupancyCheck","startTrimCalibration",
			"getDataTakingStatus","getOccupancies","createHistogram","saveHistograms"] )
