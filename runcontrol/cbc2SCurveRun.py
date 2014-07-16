"""
@brief Example of how to do an SCurve run.

There are two main components: daqProgram which controls the data aquasition by
communicating with XDAQ; and analysisControl which controls the data analysis
by communicating with the C++ code.

The two are pretty much completely separate. To date I haven't been able to get
the full DAQ chain running so that the C++ code can run online, hence why the
code is run by communicating to a server. When analysisControl is created, if
the C++ server is not running it will be started.

Since they're both separate, they need to be told individually what to do. Each
one is told the current threshold, then daqProgram is told where to save the file.
analysisControl is then told to analyse the output file. This process is repeated
for however many thresholds are required, and then at the end analysisControl is
told to save the histograms to an output file.

Updated 06/Feb/2014 so that the functionality is done in a subclass of
threading.Thread. This script doesn't actually use the extra functionality but it
is available to the gui if importing this script.

@author Mark Grimes (mark.grimes@bristol.ac.uk)
@date 06/Jan/2014
"""

import time
import threading

class SCurveRun(threading.Thread) :
	"""
	Thread that will take an s-curve run. Once an instance has been constructed, call start()
	to start in a separate thread. For most scripting purposes this isn't what you want; to run
	in the current thread call run().
	
	If running in a separate thread you can set the "quit" member to true and the program will
	stop whatever it's doing and finish.
	
	The statusCallback object provided in the constructor is a way for the thread to update you
	about how far through it is. It should be an object with two methods:
	@code
		currentStatus( fractionComplete, statusString )
		finished()
	@endcode
	currentStatus is called periodically to update you of the current status. The parameter
	fractionComplete will be a number between 0 and 1 indicating how far through processing
	the thread is. The parameter statusString is a string giving some brief information about
	what's going on. The finished method is called as the last thing before the thread
	terminates.
	You can set statusCallback to None if you don't want any progress updates.
	
	daqProgram should be an instance of pythonlib.SimpleGlibProgram and analysisControl should
	be an instance of pythonlib.AnalyserControl. Note that the analysisControl is not reset
	beforehand so that you can add data on top of other data. If you have any incompatible data
	(e.g. data taken with different settings) you should call reset on it before passing to the
	constructor.
	
	thresholds is an array of numbers between 0 and 255 indicating the thresholds to test.
	
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 03/Feb/2014
	"""
	def __init__( self, statusCallback, daqProgram, analysisControl, thresholds, temporaryOutputFilename="/tmp/cbc2SCurveRun_OutputFile.dat" ) :
		super(SCurveRun,self).__init__()
		self.statusCallback=statusCallback
		self.daqProgram=daqProgram
		self.analysisControl=analysisControl
		self.thresholds=thresholds
		self.temporaryOutputFilename=temporaryOutputFilename
		self.quit=False

	def run( self ) :
		try :
			self.daqProgram.startAllProcesses( forceRestart=True ) # forceRestart will kill the XDAQ processes first if they're running
			self.daqProgram.waitUntilAllProcessesStarted();
			self.daqProgram.initialise()
			
			# Keep a record of what the initial threshold values are and return to those afterwards
			previousThresholds=self.daqProgram.supervisor.I2CRegisterValues( chipNames=None, registerNames=['VCth'] )

			self.daqProgram.setOutputFilename( self.temporaryOutputFilename )
			
			self.daqProgram.configure()
			
			for index in range(0,len(self.thresholds)) :
				threshold=self.thresholds[index]
				# If the user wants to be updated about progress, tell them how far through we are
				if self.statusCallback!=None :
					self.statusCallback.currentStatus( float(index)/float(len(self.thresholds)), "Taking data at threshold "+str(threshold) )
		
				self.daqProgram.setAndSendI2c( { "VCth" : threshold } )
				self.analysisControl.setThreshold( threshold )
			
				self.daqProgram.play()
				while (not self.quit) and self.daqProgram.streamer.acquisitionState()=="Running":
					time.sleep(2)
				
				self.daqProgram.pause()
				if self.quit : break
				self.analysisControl.analyseFile( self.temporaryOutputFilename )
			
			# Now return the thresholds to what they were previously
			for cbcName in previousThresholds.keys() :
				# Set the threshold back for this chip (this is held in memory and not yet sent to the board)
				self.daqProgram.supervisor.setI2c( previousThresholds[cbcName], [cbcName] )
			# Send all of the I2C parameters to the board
			self.daqProgram.supervisor.sendI2c()

			self.daqProgram.killAllProcesses()
			self.daqProgram.waitUntilAllProcessesKilled();
			
			# If the user wants to be update tell them we've finished.
			if self.statusCallback!=None : self.statusCallback.finished()
		except :
			# If anything ever goes wrong, shut down the XDAQ processes before propagating the exception
			self.daqProgram.killAllProcesses()
			raise


if __name__ == '__main__':
	from pythonlib.SimpleGlibProgram import SimpleGlibProgram
	from pythonlib.AnalyserControl import AnalyserControl

	# Create an object to print the current status to the screen. This will
	# be passed to the SCurveRun instance which will call these methods.
	class PrintStatus(object) :
		def currentStatus( self, fractionComplete, statusString ) :
			print "%3d%% - %s"%(int(fractionComplete*100+0.5),statusString)
		def finished( self ) :
			print "Finished"
	
	daqProgram = SimpleGlibProgram( "GlibSuper.xml" )
	# Temporary hack. At the moment I can't get this to run as myself so I'll start
	# as the xtaldaq user
	daqProgram.contexts[0].forcedEnvironmentVariables["USER"]="xtaldaq"
	daqProgram.contexts[0].forcedEnvironmentVariables["SCRATCH"]="/tmp"
	analysisControl = AnalyserControl( "127.0.0.1", "50000" )
	analysisControl.reset() # I might be connecting to an already running controller
	
	cbc2SCurveRun=SCurveRun( PrintStatus(), daqProgram, analysisControl, range(100,150) )
	# I have no interest in running this in a separate thread (that's mostly for gui
	# stuff) so I'll just call the run method directly. If I wanted to start it in a
	# separate thread I'd call "start" instead.
	cbc2SCurveRun.run()
	
	analysisControl.saveHistograms( "/tmp/histograms.root" )
	print "Histograms saved to '/tmp/histograms.root'"
