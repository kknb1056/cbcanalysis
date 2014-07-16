#!/usr/local/bin/python

# Script that sits and listens on a Unix socket for jsonrpc commands.
#
# This script is started the first time the CBC test stand makes a Remote Procedure
# Call and stays running. This allows the RPC service to appear as though it has
# persistent state, even though apache launches each request in a new process.
#
# The message format it is expecting is "<reply address>\n<request length>\n<jsonrpc request>"
# and it will reply to the socket listening on <reply address> with "<reply length>\n<jsonrpc reply>".
# In both cases the message length parameter is for the jsonrpc request/reply only
# and doesn't include the length of the reply address or length specifier.
#
# **N.B.** If you make any changes to this script then you will have to kill the
# running process for it to take effect. The running process is most likely running
# under the user "apache". So you'll have to e.g.:
# @code
#     ps -u apache
#     <find the pid of the "python2.6" process>
#     sudo kill <pid>
# @endcode
#
# Don't kill with SIGKILL (i.e. signal 9)! If you do that the socket is not closed
# properly and the apache process will think it's still running, and won't start it
# when a request comes in. If you do, you'll have to manually remove the socket.
#
# @author Mark Grimes (mark.grimes@bristol.ac.uk)
# @date 17/Jan/2014


import sys, os, inspect, socket, time, signal
from CGIHandlerFromStrings import CGIHandlerFromStrings

import cPickle as pickle

# The "os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))" part of
# this line gets the directory of this file. I then look three parents up to get the directory
# of the CBCAnalysis installation.
INSTALLATION_PATH = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe()))), os.pardir, os.pardir))
sys.path.append( os.path.join( INSTALLATION_PATH, "runcontrol" ) )

try :
	from environmentVariables import getEnvironmentVariables
except ImportError :
	print "No runcontrol/environmentVariables.py file found. Using the defaults from runcontrol/environmentVariables_default.py"
	from environmentVariables_default import getEnvironmentVariables
from pythonlib.SimpleGlibProgram import SimpleGlibProgram
from pythonlib.AnalyserControl import AnalyserControl
from cbc2SCurveRun import SCurveRun
from cbc2OccupancyCheck import OccupancyCheck
from cbc2CalibrateChannelTrims import CalibrateChannelTrims

from pythonlib.I2cChip import I2cRegister
from pythonlib.I2cChip import I2cChip

class GlibControlService:
	"""
	Class that invokes the Glib control methods in response to JSON RPC calls.
	
	There should be no logic pertaining to the Glib here - this should solely
	pass on any commands that you want externally visible to the correct method
	in the python control library.
	
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 11/Jan/2014
	"""
	class _DataTakingStatusReceiver(object) :
		"""
		Simple class to receive the status notifications from the data taking
		thread. Sets parameters in the GlibControlService provided in the constructor,
		so intended to be created by GlibControlService passing "self".
		"""
		def __init__( self, parentControlService ) :
			self.parentControlService=parentControlService
		def currentStatus( self, fractionComplete, statusString ) :
			self.parentControlService.dataTakingFractionComplete=fractionComplete
			self.parentControlService.dataTakingStatusString=statusString
		def finished( self ) :
			self.parentControlService.dataTakingThread=None
			self.parentControlService.dataTakingFractionComplete=1
			self.parentControlService.dataTakingStatusString="Not taking data"

	def __init__(self):
		self.boardAddress = "192.168.0.175"
		self.program = SimpleGlibProgram( os.path.join( INSTALLATION_PATH, "runcontrol", "GlibSuper.xml" ) )
		
		# Need to specify the environment variables required to run XDAQ. To get them use the system
		# settings in runcontrol/environmentVariables.py. I can't get them from the current environment
		# because they might not be set for the current user (apache won't have any of them set).
		environmentVariables=getEnvironmentVariables()
		self.program.setEnvironmentVariables( environmentVariables )
		self.program.initialiseCBCs() # Do the necessary initialisation to get information about the CBCs

		# Need to provide the full executable path because this might not be in the path for different users
		# When the analyser is spawned it takes on the environment of this script, so I'll modify that directly
		self.analysisControl = AnalyserControl( "127.0.0.1", "50000", True, environmentVariables )
		self.analysisControl.reset()
		
		# Directory where the user can save their I2C files
		self.userI2cDirectory=INSTALLATION_PATH+"/runcontrol/user_i2c/"
		
		# The members below are for handling the thread that takes data
		self.dataTakingThread=None
		self.dataTakingFractionComplete=1
		self.dataTakingStatusString="Not taking data"
		
	def getStates(self, msg):
		"""
		Returns the states of all the active XDAQ applications as an array. Each element is
		itself a two element array of the application name and the state.
		"""
		try:
			results = []
			for context in self.program.contexts :
				for application in context.applications :
					results.append( [application.className,application.getState()] )
			return results
		except Exception as error:
			return "Exception: "+str(error)

	def connectedCBCNames(self, msg):
		"""
		Returns the names of the connected CBCs.
		"""
		return self.program.supervisor.connectedCBCNames()
	
	def I2CRegisterValues(self, msg):
		return self.program.supervisor.I2CRegisterValues(msg)
			
	def setI2CRegisterValues(self, msg):
		# Make sure I'm not currently taking data
		if self.dataTakingThread!=None : raise Exception("Currently taking data")

		chipNames = msg.keys()
		registerNameValueTuple = msg[chipNames[0]]


		return self.program.supervisor.setI2c( registerNameValueTuple, chipNames )
	
	def saveI2cRegisterValues(self, msg):
		self.program.supervisor.saveI2c( self.userI2cDirectory+msg )
		return msg
	
	def loadI2cRegisterValues(self, msg):
		self.program.supervisor.loadI2c( self.userI2cDirectory+msg )
		return msg
	
	def startProcesses(self, msg):
		"""
		Starts all of the XDAQ processes
		"""
		# Make sure I'm not currently taking data
		if self.dataTakingThread!=None : raise Exception("Currently taking data")
		
		try:
			self.program.startAllProcesses()
			return None
		except Exception as error:
			return "Exception: "+str(error)

	def killProcesses(self, msg):
		"""
		Kills all of the XDAQ processes
		"""
		# Make sure I'm not currently taking data
		if self.dataTakingThread!=None : raise Exception("Currently taking data")

		try:
			self.program.killAllProcesses()
			return None
		except Exception as error:
			return "Exception: "+str(error)
	
	def boardIsReachable( self, msg ):
		"""
		Pings the board to see if it is available
		"""
		# return true or false depending on whether the board can be pinged
		return testStandTools.ping( self.boardAddress )

	def stopTakingData( self, msg ) :
		"""
		Tell the data taking thread to stop whatever it's doing.
		"""
		if self.dataTakingThread!=None :
			self.dataTakingThread.quit=True
	
	def startSCurveRun( self, msg ) :
		"""
		Starts a new thread taking s-curve data
		"""
		# Make sure I'm not currently taking data
		if self.dataTakingThread!=None : raise Exception("Currently taking data")

		self.dataTakingFractionComplete=0
		self.dataTakingStatusString="Initiating s-curve run"

		thresholds=msg
		self.analysisControl.reset()
		self.dataTakingThread=SCurveRun( GlibControlService._DataTakingStatusReceiver(self), self.program, self.analysisControl, thresholds )
		self.dataTakingThread.start()

	def startOccupancyCheck( self, msg ) :
		"""
		Starts a new thread with a short 100 event run to check the occupancies for the current settings.
		"""
		# Make sure I'm not currently taking data
		if self.dataTakingThread!=None : raise Exception("Currently taking data")

		self.dataTakingFractionComplete=0
		self.dataTakingStatusString="Initiating occupancy check"

		self.analysisControl.reset()
		self.dataTakingThread=OccupancyCheck( GlibControlService._DataTakingStatusReceiver(self), self.program, self.analysisControl )
		self.dataTakingThread.start()

	def startTrimCalibration( self, msg ) :
		"""
		Starts a new thread that tries to calibrate the channel trims.
		"""
		# Make sure I'm not currently taking data
		if self.dataTakingThread!=None : raise Exception("Currently taking data")

		self.dataTakingFractionComplete=0
		self.dataTakingStatusString="Initiating trim calibration"

		self.analysisControl.reset()
		self.dataTakingThread=CalibrateChannelTrims( GlibControlService._DataTakingStatusReceiver(self), self.program, self.analysisControl, range(100,150), msg['midPointTarget'], maxLoops=msg['maxLoops'] )
		self.dataTakingThread.start()

	def getDataTakingStatus( self, msg ) :
		return {"fractionComplete":self.dataTakingFractionComplete,"statusString":self.dataTakingStatusString}
	
	def getOccupancies( self, msg ) :
		returnValue={}
		#self.analysisControl.analyseFile( "/tmp/cbc2SCurveRun_OutputFile.dat" )
		occupancies=self.analysisControl.occupancies()
		# The C++ code doesn't know which CBCs are connected. Dummy data is in the output files
		# for unconnected CBCs. I'll check which CBCs are connected and only return the data for
		# those.
		for cbcName in  self.program.supervisor.connectedCBCNames() :
			try :
				returnValue[cbcName]=occupancies[cbcName]
			except KeyError :
				# No data for that CBC
				returnValue[cbcName]=None

		return returnValue

	def createHistogram( self, msg ) :
		self.analysisControl.saveHistogramPicture( INSTALLATION_PATH+"/gui/output/"+msg['outputFilename'], msg['cbcChannelRange'] )

	def saveHistograms( self, msg ) :
		self.analysisControl.saveHistograms( msg )

if __name__ == '__main__':	

	listeningAddress="/tmp/CBCTestStand_rpc_server"
	
	if os.path.exists( listeningAddress ):
		os.remove( listeningAddress )
	
	#print "Opening socket..."
	server = socket.socket( socket.AF_UNIX, socket.SOCK_DGRAM )
	server.bind(listeningAddress)
	
	# Add a signal handler to remove the socket file if anyone sends a SIGTERM.
	# Ideally I would also do this if anyone sends a SIGKILL but SIGKILL can't
	# be caught.
	def signalHandler( signum, frame ) :
		server.close()
		os.remove( listeningAddress )
	signal.signal( signal.SIGTERM, signalHandler )
	
	try :
		logging=False
		myservice=CGIHandlerFromStrings(GlibControlService(),messageDelimiter="\n")
	
		#print "Listening..."
		while True:
			

			# First peek at the start of the message, i.e. look at it without removing it. I need to
			# see what size the message is so that I can then request using a correct buffer size.
			# The format I'm expecting is the process ID, then a space, then the message length, then
			# another space, then the message. The message length given doesn't include the extra bits
			# of information.
			packetSize=1024 # The size of the chunks I receive on the pipe
			datagram = server.recv( packetSize, socket.MSG_PEEK ) # Look but don't remove
			firstNewlinePosition=datagram.find('\n')
			secondNewlinePosition=datagram.find('\n',firstNewlinePosition+1)
			addressToReply=datagram[0:firstNewlinePosition]
			dataLength=int(datagram[firstNewlinePosition+1:secondNewlinePosition])
			messageLength=dataLength+secondNewlinePosition+1
			while packetSize < messageLength : packetSize=packetSize*2 # keep as a power of 2
			# Now that I have the correct packet size, I can get the full message and remove
			# it from the queue.
			datagram = server.recv( packetSize )
			message=datagram[secondNewlinePosition+1:]

			# I should have the full RPC message, so I can pass it on to the service to work
			# out what to do with it.
			response=myservice.handle( message )

			if logging :
				logFile=open('/tmp/serverDumpFile.log','a')
				logFile.write("REQUEST was:'"+datagram+"'\n")
				logFile.write("addressToReply was:'"+addressToReply+"'\n")
				logFile.write("messageLength was:'"+str(messageLength)+"'\n")
				logFile.write("RESPONSE is:'"+response+"'\n")
				logFile.flush()
				
			try :
				client = socket.socket( socket.AF_UNIX, socket.SOCK_DGRAM )
				client.connect( addressToReply )
				# First send the size of the response, then a space, then the actual response
				client.send( str(len(response))+'\n'+response )
				client.close()
				if logging: logFile.write('Respone has been written\n')
			except Exception as error:
				if logging : logFile.write("Exception: "+str(error)+str(error.args))
				print "Exception: "+str(error)+str(error.args)

			if logging : logFile.close()
		
		#print "-" * 20
		#print "Shutting down..."
		server.close()
		os.remove( listeningAddress )
	except :
		server.close()
		os.remove( listeningAddress )
		raise
	
