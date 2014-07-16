import httplib, urllib, json, time

class AnalyserControl :
	"""
	Class to interact with the C++ analysis program. This tells the program what to do by sending it
	HTTP requests.
	"""
	def __init__ ( self, host, port, startServerIfNotRunning=True, environmentVariables=None ):
		"""
		Connects to the instance running on the given host and port. If the instance is not running
		and startServerIfNotRunning is True, the server executable will be spawned on that port.
		If environmentVariables is not None, then that dictionary will be used as the environment
		variables to spawn the executable. If it's None then the current environment is used. If
		the server is already running then supplying environmentVariables has no effect.
		"""
		self.connection=httplib.HTTPConnection( host+":"+str(port) )
		# Make sure the connection is closed, because all the other methods assume
		# it's in that state. Presumably the connection will have failed at that
		# stage anyway.
		self.connection.close()

		try :
			# Try and connect to see if the process is running. If it isn't, I'll have
			# to start it. Remove any data that has been left over from a previous run.
			#self.httpGetRequest( "reset" )
			self.httpGetRequest( "version" )
		except:
			if startServerIfNotRunning :
				import subprocess
				print "AnalyserControl: server isn't running. Going to start on port "+str(port)+"."
				devnull=open("/dev/null")
				subprocess.Popen( ['standaloneCBCAnalyser','50000'], stdout=devnull, env=environmentVariables )
				time.sleep(1) # Sleep for a second to allow the new process to open the port
			else :
				raise Exception( "The standaloneCBCAnalyser server is not running. Either start it manually or retry with startServerIfNotRunning set to True." )
		

	def httpGetRequest( self, resource, parameters={}, returnResponse=False ) :
		"""
		For some reason httplib doesn't send the parameters as part of the URL. Not sure if it's
		supposed to but I thought it was. My mini http server running in C++ can't decode these
		(at the moment) so I'll add the parameters to the URL by hand.
		
		If returnResponse is True, the actual response from the server is returned. Otherwise
		True or False is returned depending on whether the server returned success or not.
		"""
		try:
			self.connection.connect()
			headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
			self.connection.request( "GET", "/"+urllib.quote(resource)+"?"+urllib.urlencode(parameters), {}, headers )
			response = self.connection.getresponse()
			if returnResponse :
				message=response.read()
				self.connection.close()
				return message
			else :
				self.connection.close()
				if response.status==200 : return True
				else : return False
		except :
			# Make sure the connection is closed before leaving this method, no
			# matter what the circumstances are. Otherwise the connection might
			# block next time I try to use it
			self.connection.close()
			# It's now okay to throw the original exception
			raise

	def analyseFile( self, filename ) :
		return self.httpGetRequest( "analyseFile", { "filename" : filename } )

	def saveHistograms( self, filename ) :
		return self.httpGetRequest( "saveHistograms", { "filename" : filename } )

	def saveHistogramPicture( self, outputFilename, channels ) :
		return self.httpGetRequest( "saveHistogramPicture", { "filename": outputFilename, 'channels': channels } )

	def setThreshold( self, threshold ) :
		return self.httpGetRequest( "setThreshold", { "value" : str(threshold) } )
	
	def reset( self ) :
		return self.httpGetRequest( "reset" )
	
	def restoreFromRootFile( self, filename ) :
		return self.httpGetRequest( "restoreFromRootFile", { "filename" : filename } )
	
	def fitParameters( self ) :
		jsonString=self.httpGetRequest( "fitParameters", returnResponse=True )
		analysisResult=json.loads( jsonString )
		# The CBC code indexes CBCs differently. I'll rearrange this structure to match
		# the python code.
		result={}
		for cbcName in analysisResult.keys() :
			if cbcName=='CBC 00' : pythonCbcName='FE0CBC0'
			elif cbcName=='CBC 01' : pythonCbcName='FE0CBC1'
			elif cbcName=='CBC 02' : pythonCbcName='FE1CBC0'
			elif cbcName=='CBC 03' : pythonCbcName='FE1CBC1'
			else : pythonCbcName=cbcName
			# The C++ code doesn't know if a CBC is connected or not, since unconnected CBCs
			# show up in the DAQ dumps as all on channels. I need to check whether these are
			# actually connected.
			arrayOfChannels=[]
			for channelNumber in range(0,254) :
				channelName="Channel %03d"%channelNumber
				try:
					arrayOfChannels.append( analysisResult[cbcName][channelName] )
				except:
					# The C++ code might not now about the channel and not return it. I need
					# the array to be the correct size though so I'll just add 'None'.
					arrayOfChannels.append( None )
			result[pythonCbcName]=arrayOfChannels

		return result

	def occupancies( self ) :
		jsonString=self.httpGetRequest( "occupancies", returnResponse=True )
		analysisResult=json.loads( jsonString )
		# The CBC code indexes CBCs differently. I'll rearrange this structure to match
		# the python code.
		result={}
		for cbcName in analysisResult.keys() :
			if cbcName=='CBC 00' : pythonCbcName='FE0CBC0'
			elif cbcName=='CBC 01' : pythonCbcName='FE0CBC1'
			elif cbcName=='CBC 02' : pythonCbcName='FE1CBC0'
			elif cbcName=='CBC 03' : pythonCbcName='FE1CBC1'
			else : pythonCbcName=cbcName
			# The C++ code doesn't know if a CBC is connected or not, since unconnected CBCs
			# show up in the DAQ dumps as all on channels. I need to check whether these are
			# actually connected.
			arrayOfChannels=[]
			for channelNumber in range(0,254) :
				channelName="Channel %03d"%channelNumber
				try:
					arrayOfChannels.append( analysisResult[cbcName][channelName] )
				except KeyError:
					# The C++ code might not now about the channel and not return it. I need
					# the array to be the correct size though so I'll just add 'None'.
					arrayOfChannels.append( -1 )
			result[pythonCbcName]=arrayOfChannels

		return result
