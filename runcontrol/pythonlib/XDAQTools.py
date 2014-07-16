"""
Tools to control XDAQ through python.

It's probably important to point out the following concepts.

Application - A single XDAQ module. This can be communicated with and queried for its state
              etcetera, or told to change state.
Context     - At a system level this is a single XDAQ process. This can have multiple applications
              running inside it. When it is started all the applications start, and when
              it is killed all the applications die too. Note that the individual applications
              need to be told to change state individually though. A given DAQ setup may have
              multiple contexts running on different machines, or running on the same machine
              but on different ports.
Program     - Basically encompasses everything. Your full XDAQ program, which has a list of
              all the contexts that it comprises, which in turn have a list of applications
              which they comprise. Has some utility methods to tell all contexts to do something,
              or all applications to do something.

@author Mark Grimes (mark.grimes@bristol.ac.uk) but originally influenced by xdglib
"""

import xml.etree.ElementTree as ElementTree
import httplib
import urllib
import time
import os
import re

class ETElementExtension( ElementTree._ElementInterface ) :
	"""
	Extension to the ElementTree element interface that adds methods to get children with
	a particular name, regardless of the xml namespace.
	
	Usage would be of the form:
	@code
	response=ElementTree.fromstring( <your xml> )
	response.__class__=ETElementExtension
	status=response.getchildnamed("Body").getchildnamed("getJobStatusResponse").getchildnamed("status").text
	@endcode
	
	Author Mark Grimes (mark.grimes@bristol.ac.uk)
	Date 28/Aug/2013
	"""
	def __init__( self, baseclass ) :
		self=baseclass
	def getchildrennamed( self, name ) :
		returnValue=[]
		for child in self.getchildren():
			splitByNamespace=child.tag.split("}")
			if len(splitByNamespace)==1 : tagname=splitByNamespace[0]
			elif len(splitByNamespace)==2 : tagname=splitByNamespace[1]
			if name==tagname :
				child.__class__=ETElementExtension
				returnValue.append( child )
		return returnValue
	def getchildnamed( self, name ) :
		""" Returns the first child with the supplied name (or None), ignoring xml namespaces. """
		result=self.getchildrennamed( name )
		if len(result)==0 : return None
		else : return result[0]

def sendSoapMessage( host, port, soapBody, className=None, instance=None, lid=10 ):
	"""
	Sends a soap message with the body provided to the host and port provided.
	No checking is provided that the soapBody provided is valid.
	
	If a className and instance are provided they are sent in the SOAPAction header. If either one is "None"
	then the lid is used (default is 10). If port is "None" then it defaults to 9999. This is because the
	runcontrol application has lid (local ID) of 10 in the root XDAQ daemon that listens on port 9999. 

	Author Mark Grimes (mark.grimes@bristol.ac.uk) but heavily copied from a file called xdglib.py
	Date 16/Sep/2013
	"""
	message = """<?xml version="1.0" encoding="UTF-8"?>
	<SOAP-ENV:Envelope
	 SOAP-ENV:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	 xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/">
	<SOAP-ENV:Header>
	</SOAP-ENV:Header>
	<SOAP-ENV:Body>"""
	message += soapBody
	message += """</SOAP-ENV:Body>
	</SOAP-ENV:Envelope>"""
	
	if className==None or instance==None:	
		headers = {"Content-Type":"text/xml", "charset":"utf-8","Content-Description":"SOAP Message", "SOAPAction":"urn:xdaq-application:lid="+str(lid)}
	else:
		headers = {"Content-Type":"text/xml", "charset":"utf-8","Content-Description":"SOAP Message", "SOAPAction":"urn:xdaq-application:class="+className+",instance="+str(instance)}

	if port==None : port=9999 # The port that the xdaq daemon listens on
	listeningUrl=host+":"+str(port)

	connection = httplib.HTTPConnection( listeningUrl )
	connection.request("POST", urllib.quote("/cgi-bin/query"), ElementTree.tostring( ElementTree.XML(message) ), headers )
	response = connection.getresponse()
	if (response.status != 200):
		connection.close()
		raise Exception( "Unable to send soap message because: "+str(response.status)+" - "+response.reason )
	data = response.read()
	connection.close()
	return data

def getActiveJobIDs( host="127.0.0.1", port=9999 ) :
	"""
	Returns a list of the job IDs for all active XDAQ processes. This is given as array of
	tuples that list the job ID and the pid for each process. These are in the tuple as "jid"
	and "pid" respectively.
	"""
	# I can't find a SOAP request anywhere that will tell me the job ID for a given context.
	# The only place I've been able to find this information is in the jobcontrol webpage.
	# I'll have to request that page and filter out the information I want.
	listeningUrl=host+":"+str(port)
	connection = httplib.HTTPConnection( listeningUrl )
	connection.request("GET", "/urn:xdaq-application:lid=10/", {}, {} )
	response = connection.getresponse()
	if (response.status != 200):
		connection.close()
		raise Exception( "Unable to send soap message because: "+str(response.status)+" - "+response.reason )
	data = response.read()
	# I now have the full HTML for the jobcontrol default webpage. The job IDs are buried inside a table
	# somewhere inside this. I'll do a regular expression search for the format of the table cell. This
	# command will return any cell contents that immediately follows the HTML '<td bgcolor="#F0F0D0">'.
	potentialJobIDs=re.findall('(?<=\<td bgcolor="#F0F0D0"\>)\w+',data)
	# I now have a list of entries in the job status table. Some of these will be job IDs and some will be
	# other data (e.g. user, status). This list always seems to have the data in the same place, but to be
	# safe I'll just query the job status for each entry. When it's not a job ID I'll just get an error
	# message. Besides, I don't know which job IDs are for still active jobs. This information will be in
	# the response to the getJobStatus message.
	activeJobIDs = []
	for jobID in potentialJobIDs :
		# Ask for information about the job. If it isn't actually a job ID an exception will be thrown
		# which I'll catch and ignore.
		response=ElementTree.fromstring( sendSoapMessage( host, str(port), '<xdaq:getJobStatus jid="'+jobID+'" xmlns:xdaq="urn:xdaq-soap:3.0" />', lid=10 ) )
		try:
			response.__class__=ETElementExtension
			status=response.getchildnamed("Body").getchildnamed("getJobStatusResponse").getchildnamed("status").text
			pid=response.getchildnamed("Body").getchildnamed("getJobStatusResponse").getchildnamed("pid").text
			# Active jobs seem to be given the status "0"
			if status=="0" : activeJobIDs.append( { "jid":jobID, "pid":pid } )
		except:
			pass
	return activeJobIDs

	

def sendSoapStartCommand( host, port, configFilename, forcedEnvironmentVariables={} ):
	# These are the environment variables that must be provided. I'm pretty sure I can
	# trim this down, but that's a job for a later date.
	requiredEnvironmentVariableNames=['USER',
		'XDAQ_ROOT',
		'XDAQ_OS',
		'XDAQ_PLATFORM',
		'XDAQ_DOCUMENT_ROOT',
		'ROOTSYS',
		'LD_LIBRARY_PATH',
		'SCRATCH',
		'CMSSW_BASE',
		'CMSSW_RELEASE_BASE']
	environmentVariables={}

	# Try and set all of the required variables from anything given by forcedEnvironmentVariables.
	# If it's not present, try and set from the current environment. If that doesn't work, fail.
	for variableName in requiredEnvironmentVariableNames:
		try:
			environmentVariables[variableName]=forcedEnvironmentVariables[variableName]
		except KeyError:
			variable=os.getenv(variableName)

			if variable==None: raise Exception("Environment variable "+variableName+" has not been set and is not available from the current environment")
			environmentVariables[variableName]=variable
	
	# I should now have filled all of the required variables, but forcedEnvironmentVariables might contain
	# something extra that the user wants added.
	for variableName in forcedEnvironmentVariables:
		# Doesn't matter if I overwrite anything from the previous step, because they will be the same.
		environmentVariables[variableName]=forcedEnvironmentVariables[variableName]
	
	
	# Now I have all of the environment variables figured out I can craft the message body
	soapBody = '<xdaq:startXdaqExe execPath="'+environmentVariables['XDAQ_ROOT']+'/bin/xdaq.exe" user="'+environmentVariables["USER"]+'" argv="-p '+str(port)+' -l INFO" xmlns:xdaq="urn:xdaq-soap:3.0" >\n'
	soapBody += '<EnvironmentVariable '
	


	for key in environmentVariables:
		soapBody+=key+'="'+environmentVariables[key]+'" '
	soapBody += """/>
	<ConfigFile>
	<![CDATA[\n"""
		
	configFile=open(configFilename)
	for line in configFile.readlines():
		soapBody += line
	  
	soapBody += """]]>
	</ConfigFile>
	</xdaq:startXdaqExe>"""
	
	return sendSoapMessage( host, None, soapBody )

		
class Context(object) :
	"""
	Class representing XDAQ Contexts, including all of the Applications in the Context.
	
	Author Mark Grimes (mark.grimes@bristol.ac.uk)
	Date 28/Aug/2013
	"""
	def __init__( self, elementTreeNode, configFilename ) :
		self.forcedEnvironmentVariables={"XDAQ_ELOG":"SET",
			"PYTHONHOME":"/usr/lib64/python2.4",
			"PYTHONPATH":"/usr/lib64/python2.4:/home/xtaldaq/cmssw/slc5_amd64_gcc462/cms/cmssw/CMSSW_5_3_4/src:/home/xtaldaq/cmssw/slc5_amd64_gcc462/cms/cmssw/CMSSW_5_3_4/cfipython/slc5_amd64_gcc462",
			"SCRATCH":"/tmp",
			"POOL_OUTMSG_LEVEL":"4",
			"POOL_STORAGESVC_DB_AGE_LIMIT":"10",
			"XDAQ_ROOT":"/opt/xdaq",
			"XDAQ_DOCUMENT_ROOT":"/opt/xdaq/htdocs"}

		self.applications = []
		self.configFilename = configFilename
		self.jobid = -1
		# Split off anything that is part of the xml namespace (in ElementTree this
		# is encapsulated in curly braces).
		splitByNamespace=elementTreeNode.tag.split("}")
		if len(splitByNamespace)==1 : tagname=splitByNamespace[0]
		elif len(splitByNamespace)==2 : tagname=splitByNamespace[1]
		else : raise Exception( "Couldn't split off the namespace in '"+elementTreeNode.tag+"'" )
		if tagname!="Context" : raise Exception( "Not a Context node" )
		# Loop over the items and record the url and port
		for item in elementTreeNode.items() :
			if item[0]=="url" : currentURL=item[1]
		if currentURL==None : raise Exception( "Couldn't get the URL for this context" )
		self.host=currentURL.split(":")[-2].split("/")[-1] # Get everything after "http://" and before the port (i.e. last ":" separator)
		self.port=currentURL.split(":")[-1]
		# Now loop over all of the children and look for Application nodes
		for child in elementTreeNode.getchildren() :
			# Split off the namespace as before
			splitByNamespace=child.tag.split("}")
			if len(splitByNamespace)==1 : tagname=splitByNamespace[0]
			elif len(splitByNamespace)==2 : tagname=splitByNamespace[1]
			else : raise Exception( "Couldn't split off the namespace in '"+child.tag+"'" )
			if tagname=="Application" :
				# Loop over the items and record the url and port
				for item in child.items() :
					if item[0]=="class" : className=item[1]
					elif item[0]=="id" : id=item[1]
					elif item[0]=="instance" : instance=item[1]
				# Now try and create the Application object and add it to the return value
				newApplication=Application( self.host, int(self.port), className, int(instance), int(id) )
				self.applications.append( newApplication )

	def __repr__(self) :
		return "<XDAQ Context "+self.host+", "+str(self.port)+", "+str(self.jobid)+">"

	def reattach( self, activeJobIDs=None ) :
		"""
		Attempts to work out the job ID for this context if the XDAQ process is already running. The
		activeJobIDs parameter should be the result of a call to getActiveJobIDs(). It it is "None"
		then getActiveJobIDs will be called automatically. This parameter is just to save time when
		reattaching multiple contexts - call getActiveJobIDs once and pass the result to each one
		rather than letting each one call it.
		"""
		if activeJobIDs==None : activeJobIDs=getActiveJobIDs( self.host )
		# If there is a XDAQ process running on the port specified for this context, I can query
		# it for the process ID (pid). If that matches an entry in activeJobIDs then I can set the
		# jobid to the one given by that entry.
		mypid = None
		self.jobid = -1
		try :
			response=ElementTree.fromstring( sendSoapMessage( self.host, self.port, '<xdaq:ParameterQuery xmlns:xdaq="urn:xdaq-soap:3.0" />', lid=0 ) )
			response.__class__=ETElementExtension
			mypid=response.getchildnamed("Body").getchildnamed("ParameterQueryResponse").getchildnamed("properties").getchildnamed("descriptor").getchildnamed("properties").getchildnamed("pid").text
		except :
			pass
		if mypid == None : return False
		# Now I have the pid for this process, I'll look through the list to see if it matches an entry
		for jobDescription in activeJobIDs :
			if mypid == jobDescription["pid"] : self.jobid=jobDescription["jid"]
		if self.jobid == -1 : raise Exception( "Couldn't find pid for "+str(self)+" in the list of active processes" )

		
	def startProcess( self, ignoreIfCurrentlyRunning=False, forceRestart=False, forceStart=False ) :
		"""
		Starts the XDAQ process for this context. If this representation thinks the process is already
		running one of four things can happen depending on the values of ignoreIfCurrentlyRunning,
		forceRestart and forceStart.
		
		(These go in order - the first one that matches is what happens)
		
		ignoreIfCurrentlyRunning is True - The function returns silently.
		forceRestart is True             - An attempt is made to kill the currently running process before
		                                   starting it again.
		forceStart is True               - The process is started anyway. 
		all parameters are False         - An exception is thrown. 
		"""
		if self.jobid != -1 :
			if ignoreIfCurrentlyRunning :
				return
			elif forceRestart :
				self.killProcess()
				self.waitUntilProcessKilled()
			else :
				if not forceStart : raise Exception("Context "+str(self)+" appears to already be running. Try again with either forceRestart or forceStart set to True.")

		self.jobid=-1
		response=ElementTree.fromstring( sendSoapStartCommand( self.host, self.port, self.configFilename, self.forcedEnvironmentVariables ) )
		try:
			response.__class__=ETElementExtension
			self.jobid = response.getchildnamed("Body").getchildnamed("jidResponse").getchildnamed("jid").text
		except:
			raise Exception( "Couldn't start process. Response was: "+ElementTree.tostring(response) )
		
	def killProcess(self) :
		if self.jobid==-1 :
			return False
		response=ElementTree.fromstring( sendSoapMessage( self.host, None, '<xdaq:killExec user="xtaldaq" jid="'+self.jobid+'" xmlns:xdaq="urn:xdaq-soap:3.0" />' ) )
		try:
			response.__class__=ETElementExtension
			reply=response.getchildnamed("Body").getchildnamed("getStateResponse").getchildnamed("reply").text
			if reply=='no job killed.' : return False
			elif reply=='killed by JID' :
				self.jobid=-1
				return True
		except:
			raise Exception( "Couldn't kill process. Response was: "+ElementTree.tostring(response) )

	def waitUntilProcessStarted( self, timeout=30.0 ) :
		"""
		Blocks until the process has started and all applications are contactable, or throws an exception if
		"timeout" seconds have passed.
		"""
		timeoutEndTime=time.time()+timeout;
		while True :
			allAplicationsStarted=True
			for application in self.applications:
				if application.getState()=="<uncontactable>": allAplicationsStarted=False
				if allAplicationsStarted: return
				if timeoutEndTime<time.time() : raise Exception("Context "+repr(self)+" did not start all applications within "+str(timeout)+" seconds.")
				time.sleep(0.5)

	def waitUntilProcessKilled( self, timeout=10.0 ) :
		"""
		Blocks until the process has stopped and all applications are uncontactable, or throws an exception if
		"timeout" seconds have passed.
		"""
		timeoutEndTime=time.time()+timeout;
		while True :
			allAplicationsStopped=True
			for application in self.applications:
				if application.getState()!="<uncontactable>": allAplicationsStopped=False
				if allAplicationsStopped: return
				if timeoutEndTime<time.time() : raise Exception("Context "+repr(self)+" did not kill all applications within "+str(timeout)+" seconds.")
				time.sleep(0.5)

class Application(object) :
	"""
	Class representing XDAQ Applications.
	
	Author Mark Grimes (mark.grimes@bristol.ac.uk)
	Date 28/Aug/2013
	"""
	def __init__(self, host, port, className, instance, id ) :
		self.host=host
		self.port=port
		self.className=className
		self.instance=instance
		self.id=id
		self.connection=httplib.HTTPConnection( self.host+":"+str(self.port) )
		# Make sure the connection is closed, because all the other methods assume
		# it's in that state. Presumably the connection will have failed at that
		# stage anyway.
		self.connection.close()

	def __repr__(self) :
		return "<XDAQ Application "+self.host+", "+str(self.port)+", "+self.className+", "+str(self.instance)+">"

	def sendCommand( self, command ) :
		return sendSoapMessage( self.host, self.port, '<xdaq:'+command+' xmlns:xdaq="urn:xdaq-soap:3.0"/>', self.className, self.instance )

	def getState(self) :
		try :
			result=ElementTree.fromstring( self.sendCommand('ParameterQuery') )
			result.__class__=ETElementExtension
			try :
				return result.getchildnamed("Body").getchildnamed("ParameterQueryResponse").getchildnamed("properties").getchildnamed("stateName").text
			except :
				return "<unknown>"
		except : return "<uncontactable>"

	def waitForState(self,state,timeout=5.0):
		"""
		Blocks until the state of the application has reached the one specified, or if "timeout" seconds
		have passed then an exception will be thrown. If timeout is negative then the application must
		already be in the desired state or the exception is thrown immediately.
		"""
		timeoutEndTime=time.time()+timeout;
		while True :
			if self.getState()==state : return
			if timeoutEndTime<time.time() : raise Exception("Application "+repr(self)+" did not reach state "+state+" within "+str(timeout)+" seconds.")
			time.sleep(0.2)
			
	def httpRequest( self, requestType, resource, parameters={}, storeMessage=True ) :
		"""
		Send an http request to the application to the resource specified, with
		optional parameters specified as a dictionary. "requestType" is the http
		type, e.g. "GET" or "POST".
		"""
		try :
			self.connection.connect()
			# I copied this from an example on stack overflow
			headers = {"Content-type": "application/x-www-form-urlencoded","Accept": "text/plain"}
			self.connection.request( requestType, urllib.quote(resource), urllib.urlencode(parameters), headers )
			response = self.connection.getresponse()
			if storeMessage:
				# I need to "read" the response message before the connection gets closed.
				# I'll store the message in a custom member of the response class that gets
				# returned to the user.
				response.fullMessage=response.read()
			self.connection.close()
			return response
		except :
			# Make sure the connection is closed before leaving this method, no
			# matter what the circumstances are. Otherwise the connection might
			# block next time I try to use it
			self.connection.close()
			# It's now okay to throw the original exception
			raise


class Program(object) :
	"""
	Class to control all XDAQ Contexts and Applications.
	
	Author Mark Grimes (mark.grimes@bristol.ac.uk)
	Date 29/Aug/2013
	"""
	def __init__( self, xdaqConfigFilename ) :
		self.xdaqConfigFilename = xdaqConfigFilename
		self._loadXDAQConfig()
		# See if the processes were left running by an earlier instance. If they
		# were try and reattach
		self.reattachAll()

	def _loadXDAQConfig( self ) :
		self.contexts = []
		tree=ElementTree.parse( self.xdaqConfigFilename )
		for node in tree.getroot().getchildren() :
			try :
				newContext=Context( node, self.xdaqConfigFilename )
				self.contexts.append( newContext )
			except Exception as error :
				# Some of these nodes might not be Contexts, so don't print any errors for those
				if( str(error)!="Not a Context node" ) :
					print "Unable to create context for node",str(node),"because",str(error)

	def reloadXDAQConfig( self ) :
		del self.contexts
		self._loadXDAQConfig()

	def setEnvironmentVariables( self, environmentVariables ) :
		"""
		Sets all the contexts to use the environment variables given when they start the
		XDAQ process. Takes a dictionary.
		"""
		# Do some quick error checking to make sure everything passed was a string
		if type(environmentVariables) is not dict : raise TypeError("setEnvironmentVariables was called with an object that is not a dictionary")
		for key in environmentVariables.keys() :
			if type(key) is not str : raise TypeError("setEnvironmentVariables was called with a dictionary that has a key that is not a string")
			if type(environmentVariables[key]) is not str : raise TypeError("setEnvironmentVariables was called with a dictionary that has a value that is not a string")
		# Input appears to be okay, so set the environment variables for all contexts
		for context in self.contexts:
			context.forcedEnvironmentVariables = environmentVariables

	def startAllProcesses( self, ignoreIfCurrentlyRunning=False, forceRestart=False, forceStart=False ) :
		"""
		Starts all of the processes. See the docs for startProcess for the meaning of the parameters.
		"""
		for context in self.contexts:
			context.startProcess( ignoreIfCurrentlyRunning, forceRestart, forceStart )

	def killAllProcesses( self ) :
		for context in self.contexts:
			context.killProcess()
			
	def reattachAll( self ) :
		activeJobIDs=getActiveJobIDs()
		for context in self.contexts:
			context.reattach( activeJobIDs )
			
	def waitUntilAllProcessesStarted( self, timeout=30.0 ) :
		startTime=time.time() # Since they don't run concurrently, I need to subtract previous waits
		for context in self.contexts:
			context.waitUntilProcessStarted( timeout-(time.time()-startTime) )
		# Add an arbitrary wait time because I've had cases where the process appears to be
		# running but the applications don't respond quite yet.
		time.sleep(2)

	def waitUntilAllProcessesKilled( self, timeout=10.0 ) :
		startTime=time.time() # Since they don't run concurrently, I need to subtract previous waits
		for context in self.contexts:
			context.waitUntilProcessKilled( timeout-(time.time()-startTime) )

	def sendAllCommand( self, command ) :
		for context in self.contexts :
			for application in context.applications :
				application.sendCommand( command )

	def printAllStates( self, hideComms=False ) :
		for context in self.contexts :
			firstApplication=True
			for application in context.applications :
				if (not hideComms) or application.className!="pt::atcp::PeerTransportATCP" :
					if firstApplication :
						contextString=context.host+":"+str(context.port)+" (job ID="+str(context.jobid)+")"
						firstApplication=False
					else : contextString=""
					print contextString.ljust(40)+application.className.ljust(30)+str(application.instance).rjust(4)+"   "+application.getState()

	def findAllMatchingApplications( self, className, instance=None ) :
		"""
		Returns an array of all the Applications that match the given className, and optional instance
		"""
		returnValue=[]
		for context in self.contexts :
			for application in context.applications :
				shouldAdd=False
				if application.className==className : shouldAdd=True
				if instance!=None :
					if instance!=application.instance : shouldAdd=False
				if shouldAdd : returnValue.append( application )
		return returnValue

	def sendAllMatchingApplicationsCommand( self, command, className, instance=None ) :
		matchingApps=self.findAllMatchingApplications( className, instance )
		for application in matchingApps :
			try:
				application.sendCommand( command )
			except:
				print "Unable to contact "+str(application)

	def waitAllMatchingApplicationsForState( self, state, timeout, className, instance=None ) :
		"""
		Blocks until all applications that match the specifics given reach the state given, or
		throws an exception if "timeout" seconds have elapsed.
		"""
		matchingApps=self.findAllMatchingApplications( className, instance )
		for application in matchingApps :
			try:
				application.waitForState( state, timeout )
			except Exception as error:
				print str(error)
