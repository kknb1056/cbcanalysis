"""
@brief Example script to calibrate the channel trims on a CBC2

@author Mark Grimes (mark.grimes@bristol.ac.uk)
@date 23/Jan/2014
"""
import threading

from pythonlib.SimpleGlibProgram import SimpleGlibProgram
from pythonlib.AnalyserControl import AnalyserControl
from cbc2SCurveRun import SCurveRun



def simpleLinearFit( xCoords, yCoords ) :
	"""
	Simple linear fit. Didn't want to bring in an external dependency for something so simple.
	"""
	if len(xCoords)!=len(yCoords) : raise Exception( "simpleLinearFit - the xCoords and yCoords arrays need to be the same size")
	if len(xCoords)<2 : raise Exception( "simpleLinearFit - you need at least two datapoints")
	xyBar=0.0
	xBar=0.0
	yBar=0.0
	xSquaredBar=0.0
	for index in range(0,len(xCoords)) :
		xyBar+=(xCoords[index]*yCoords[index])
		xBar+=xCoords[index]
		yBar+=yCoords[index]
		xSquaredBar+=(xCoords[index]*xCoords[index]);
	xyBar/=len(xCoords)
	xBar/=len(xCoords)
	yBar/=len(xCoords)
	xSquaredBar/=len(xCoords)
	
	slope=(xyBar-xBar*yBar)/(xSquaredBar-xBar*xBar);
	intercept=yBar-slope*xBar;
	return {'slope':slope,'intercept':intercept}

def fitPreviousResults( target ) :
	xCoords=[]
	yCoords=[]
	for previousResult in target['previousResults'] :
		xCoords.append( previousResult['trim'] )
		yCoords.append( previousResult['mean'] )
	# If there is only one point then add (0,0)
	if len(xCoords)==1 :
		xCoords.append(0)
		yCoords.append(0)
	return simpleLinearFit( xCoords, yCoords )


class CalibrateChannelTrims(threading.Thread):
	def __init__( self, statusCallback, daqProgram, analysisControl, scanRange, midPointTarget, interimOutputFilename=None, maxLoops=10, channelsToCalibrate=None ) :
		super(CalibrateChannelTrims,self).__init__()
		self.statusCallback=statusCallback
		self.daqProgram=daqProgram
		self.analysisControl=analysisControl
		self.scanRange=scanRange
		self.midPointTarget=midPointTarget
		self.interimOutputFilename=interimOutputFilename
		self.maxLoops=maxLoops
		self.quit=False
		
		# Before making asking which CBCs are connected I have to initialise
		# the CBC information in the control program. This isn't in the __init__
		# because it requires starting the XDAQ process which might not be what the
		# user wants
		self.daqProgram.initialiseCBCs()
		# Set the targets for all of the channels
		self.targets=[]
		connectedCBCs=self.daqProgram.supervisor.connectedCBCNames()
		for index in range( 0, len(connectedCBCs) ) :
			cbcName=connectedCBCs[index]
			if channelsToCalibrate==None : # default is to calibrate all channels
				channels=range(0,254)
			else :
				try :
					channels=channelsToCalibrate[index]
				except IndexError :
					raise ValueError( "CalibrateChannelTrims - channelsToCalibrate was specified but it didn't have enough entries for the number of connected CBCs" )
				
			for channelNumber in channels :
				self.targets.append( {'cbcName':cbcName, 'channelNumber':channelNumber, 'target':self.midPointTarget, 'previousResults':[] } )

	def run( self ) :
		"""
		Tries to calibrate the channel trims on all the connected CBCs so that the midpoint
		of their s-curves sits on mitPointTarget.
		
		If interimOutputFilename is not 'None' then the files from each loop willbe saved to
		files and directories starting with that name.
		
		@author Mark Grimes (mark.grimes@bristol.ac.uk)
		@date 24/Jan/2014
		"""
		# I perform linear fits of the previous results to estimate what the trim to be.
		# For this to work I'll start off taking one run with very low trims and one with
		# high trims so that I have enough data points for the fit.
			
		# Take a run a quarter of the way along the range
		for target in self.targets :
			self.daqProgram.supervisor.setChannelTrim(target['channelNumber'],63,[target['cbcName']])
		if self.statusCallback!=None : self.statusCallback.currentStatus( 0.0, "Taking run with trims at 1/4" )
		self.loopDescription="Loop with low trims" # Set this for currentStatus() to report properly
		self.loop=-1 # little hack for currentStatus() to report the fractionComplete properly
		self.takeRun()
		if self.interimOutputFilename!=None : self.analysisControl.saveHistograms( self.interimOutputFilename+"-low.root" )

		# See if the user has decided to quit out of the thread.
		if self.quit :
			if self.statusCallback!=None : self.statusCallback.finished()
			return

		# Take a run a quarter from the end of the range
		for target in self.targets :
			self.daqProgram.supervisor.setChannelTrim(target['channelNumber'],191,[target['cbcName']])
		if self.statusCallback!=None : self.statusCallback.currentStatus( 1.0/float(self.maxLoops+2), "Taking run with trims at 3/4" )
		self.loopDescription="Loop with high trims" # Set this for currentStatus() to report properly
		self.loop=0 # little hack for currentStatus() to report the fractionComplete properly
		self.takeRun()
		if self.interimOutputFilename!=None : self.analysisControl.saveHistograms( self.interimOutputFilename+"-high.root" )
		
		# See if the user has decided to quit out of the thread.
		if self.quit :
			if self.statusCallback!=None : self.statusCallback.finished()
			return

		self.setNewTrims()
	
		scurvesAlign=False
		self.loop=0
		while (not scurvesAlign) and (not self.quit) :
			self.loop+=1
			self.loopDescription="Loop "+str(self.loop)
			if self.statusCallback!=None :
				self.statusCallback.currentStatus( float(self.loop+1)/float(self.maxLoops+2), "Starting loop "+str(self.loop)+". Channels still to calibrate="+str(len(self.targets)) )
			#for target in self.targets :
			#	print target
			self.takeRun()
			self.setNewTrims()
			if self.interimOutputFilename!=None : self.analysisControl.saveHistograms( self.interimOutputFilename+"-loop"+str(self.loop)+".root" )
			# Might as well save the trims as I go in case something goes wrong
			if self.interimOutputFilename!=None : self.daqProgram.supervisor.saveI2c( self.interimOutputFilename+"-loop"+str(self.loop) )
			# Decide whether to drop out or not
			if len(self.targets)==0 : scurvesAlign=True
			elif self.loop>self.maxLoops :
				# Create a message to let the user know which channels failed. Any channels
				# that have converged will have been taken out of the targets array.
				failMessage="Reached "+str(self.maxLoops)+" loops and the s-curves for channels "
				for failedTarget in self.targets :
					bestResult=self.setToBestPreviousResult( failedTarget )
					failMessage+=failedTarget['cbcName']+"["+str(failedTarget['channelNumber'])+"] (managed to calibrate to "+str(bestResult['mean'])+"), "
				failMessage+="failed to converge."
				print failMessage
				scurvesAlign=True

		# If the user wants to be update tell them we've finished.
		if self.statusCallback!=None : self.statusCallback.finished()

	def takeRun( self ) :
		self.analysisControl.reset()
		self.cbc2SCurveRun=SCurveRun( self, self.daqProgram, self.analysisControl, self.scanRange )
		self.cbc2SCurveRun.run()
		#analysisControl.restoreFromRootFile( "/tmp/calibrateChannelTrims-low.root" )
		fitParameters=self.analysisControl.fitParameters()
		for target in self.targets :
			channelNumber=target['channelNumber']
			cbcName=target['cbcName']
			target['previousResults'].append(
				{'trim':self.daqProgram.supervisor.getChannelTrim(channelNumber,[cbcName]),
				'mean':fitParameters[cbcName][channelNumber]['mean'] }
			)

	def channelsLeftToCalibrate( self ) :
		"""
		Method for the user to call to see how many channels are still left
		to calibrate.
		"""
		return len(self.targets)

	def currentStatus( self, fractionComplete, statusString ) :
		"""
		Method that SCurveRun will call to inform of me of its progress.
		"""
		# Pass this message on to the listener that was registered when this
		# class was created. Take account that 100% from s-curve is just the
		# end of this current loop.
		self.statusCallback.currentStatus( float(self.loop+1+fractionComplete)/float(self.maxLoops+2), self.loopDescription+": "+statusString )
		# Use this chance to quit out of the s-curve run if the user has set quit to True
		self.cbc2SCurveRun.quit=self.quit

	def finished( self ) :
		"""
		Method that SCurveRun will call to inform of me when its finished
		the current run.
		"""
		# This just signifies the end of the current run, so inform the
		# registered listener.
		self.statusCallback.currentStatus( float(self.loop+2)/float(self.maxLoops+2), self.loopDescription+" finished" )

	def setToBestPreviousResult( self, target ) :
		closestdifferenceFromTarget=9999
		newTrim=-1
		newMean=-1
		for previousResult in target['previousResults'] :
			differenceFromTarget=abs( target['target']-previousResult['mean'] )
			if differenceFromTarget<closestdifferenceFromTarget :
				newTrim=previousResult['trim'] # This previous result is better so use that
				closestdifferenceFromTarget=differenceFromTarget
				newMean=previousResult['mean'] # store this so that I can return it for info
		if newTrim==-1 : raise Exception( "Unable to find a previous trim result")
		self.daqProgram.supervisor.setChannelTrim(target['channelNumber'],newTrim,[target['cbcName']])
		# Return the trim and the mean you get from it in case the caller is intersted
		return {'trim':newTrim,'mean':newMean}
		
	def setNewTrims( self ) :
		# This will be filled with things to take out of the targets array,
		# either because the target has been met or because it looks like
		# a dead channel.
		targetsToRemove=[]
		
		for target in self.targets :
			# First see if the last iteration was successful
			if int(target['previousResults'][-1]['mean']+0.5)==target['target'] : # Add .5 to round properly
				targetsToRemove.append(target) # Found the trim, so no longer consider this
				continue
			# Look at all the previous results and try a linear fit to estimate
			# a trim that will give the relationship between trim and mean.
			fit=fitPreviousResults( target )
			# If the fit is exactly flat then the channel is probably dead
			# I'll take it out of the targets, but I can't remove it mid-loop.
			if fit['slope']==0 :
				targetsToRemove.append( target )
				continue
			#print "Slope="+str(fit['slope'])+" intercept="+str(fit['intercept'])
			# invert y=mx+c to get x=(y-c)/m
			newTrim=int( (target['target']-fit['intercept'])/fit['slope'] + 0.5 ) # add .5 to round properly
			if newTrim<0 : newTrim=0
			elif newTrim>255 : newTrim=255
			# Look through the previous results to make sure I haven't tried
			# this trim before.
			newTrimHasAlreadyBeenTried=True # Set to true to get it in the loop for the first time
			modificationDirection=0 # I'll set this to -1 if decreasing the trim, +1 if increasing, 0 if undefined
			while newTrimHasAlreadyBeenTried :
				newTrimHasAlreadyBeenTried=False
				for previousResult in target['previousResults'] :
					if previousResult['trim']==newTrim :
						if previousResult['mean']>target['target'] : directionToChange=1
						else : directionToChange=-1
						# If modificationDirection is not zero then I've changed the trim at least
						# once. If the direction to change this time is different to the previous
						# one then I'm not able to find a matching trim. Just set to the best match
						# and remove it from the list of channels to calibrate
						if modificationDirection!=0 and modificationDirection!=directionToChange :
							targetsToRemove.append(target)
							newTrim=-1 # Set this to something invalid so that I can check it later
							break
						else :
							modificationDirection=directionToChange
							newTrim+=modificationDirection
							newTrimHasAlreadyBeenTried=True
				
			#print "Setting trim for ["+target['cbcName']+"]["+str(target['channelNumber'])+"]="+str(newTrim)
			if newTrim!=-1 : self.daqProgram.supervisor.setChannelTrim(target['channelNumber'],newTrim,[target['cbcName']])
			else : self.setToBestPreviousResult(target)
			
		# Now I've finished looping over targets I can safely take
		# out the entries I want to.
		for item in targetsToRemove :
			self.targets.remove( item )

if __name__ == '__main__':
	try :
		from environmentVariables import getEnvironmentVariables
	except ImportError :
		print "No runcontrol/environmentVariables.py file found. Using the defaults from runcontrol/environmentVariables_default.py"
		from environmentVariables_default import getEnvironmentVariables

	# Create an object to print the current status to the screen. This will
	# be passed to the SCurveRun instance which will call these methods.
	class PrintStatus(object) :
		def currentStatus( self, fractionComplete, statusString ) :
			print "%3d%% - %s"%(int(fractionComplete*100+0.5),statusString)
		def finished( self ) :
			print "Finished"

	daqProgram = SimpleGlibProgram( "GlibSuper.xml" )
	environmentVariables=getEnvironmentVariables()
	daqProgram.setEnvironmentVariables( environmentVariables )
	analysisControl = AnalyserControl( "127.0.0.1", "50000", True, environmentVariables )
	
	cbc2CalibrateChannelTrims=CalibrateChannelTrims( PrintStatus(), daqProgram, analysisControl, range(100,150), 127, "/tmp/calibrateChannelTrims" )
	# I have no interest in running this in a separate thread (that's mostly for gui
	# stuff) so I'll just call the run method directly. If I wanted to start it in a
	# separate thread I'd call "start" instead.
	cbc2CalibrateChannelTrims.run()
	
	print "Saving calibrated trims to '/tmp/calibratedTrims'"
	daqProgram.supervisor.saveI2c( "/tmp/calibratedTrims" )
