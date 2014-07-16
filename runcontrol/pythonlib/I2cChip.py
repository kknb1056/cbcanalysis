

class I2cRegister :
	"""
	Class to hold details about an I2C register for the CBC test stand.
	 
	Author Mark Grimes (mark.grimes@bristol.ac.uk)
	Date 08/Aug/2013
	"""
	def __init__(self,name,page,address,defaultValue,value) :
		self.name=name
		self.page=int(page,0)
		self.address=int(address,0)
		self.defaultValue=int(defaultValue,0)
		self.value=int(value,0)
	def __repr__(self) :
		return "<I2cRegister "+self.name+", "+hex(self.value)+">"
	def writeToFile(self,file) :
		file.write( self.name.ljust(32)+" "+("0x%02x"%self.page).ljust(8)+" "+("0x%02x"%self.address).ljust(8)+" "+("0x%02x"%self.defaultValue).ljust(8)+" "+("0x%02x"%self.value).ljust(8)+"\n" )

class I2cChip :
	"""
	Class to hold several instances of an I2cRegister.
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 08/Aug/2013
	"""
	def __init__( self, filename=None ) :
		self.registers=[]
		if filename!=None : self.loadFromFile(filename)

	def loadFromFile( self, filename ) :
		"""
		Loads information about register names, addresses and values from the provided text file.
		Overwrites any registers that were present before, but leaves ones not mentioned in the
		text file alone.
		"""
		inputFile = open(filename,'r')
		for line in inputFile.readlines() :
			# Take everything before any comments (comments start with either '#' or '*'
			# and continue until the end of the line).
			lineNoComments=line.split('#')[0]
			lineNoComments=lineNoComments.split('*')[0]
			if len(lineNoComments)>0:
				splitLine = lineNoComments.split()
				if len(splitLine) != 5 : raise Exception("I2C file appears to be in an incorrect format. Line '"+line+"' should split into 5 columns")
				newRegister = I2cRegister( splitLine[0], splitLine[1], splitLine[2], splitLine[3], splitLine[4] )
				self.addRegister( newRegister )
		

	def addRegister(self,register) :
		self.registers.append(register)

	def getRegister(self,registerName) :
		"""
		Returns the Register instance with the given name
		"""
		for register in self.registers :
			if register.name==registerName : return register
		# If control got this far then no register was found
		return None
	
	def getValues(self,registerNames=None) :
		"""
		Returns the value of each register in {"<name>": <value>, ...} tuple form. If an array of
		registerNames is specified only those are returned.
		"""
		returnValue = {}
		for register in self.registers :
			addThisValue=True
			if registerNames!=None :
				if registerNames.count(register.name)==0 : addThisValue=False
			if addThisValue : returnValue[register.name]=register.value
		return returnValue
	
	def setChannelTrim( self, channelNumber, value ) :
		"""
		Set the register with the name "Channel<channelNumber+1>" to the supplied value.
		
		Note that the argument starts counting from zero, whereas the register string
		name starts counting from 1. So calling setChannelTrim( 23, <value> ) will
		change the register named "Channel024". This is because all other code regarding
		channels starts counting from zero.
		"""
		# I don't know of the channel numbers are padded with zeros in the register
		# name, so I'll try a few possibilities
		possibleNames = [ "Channel%d"%(channelNumber+1), "Channel%02d"%(channelNumber+1), "Channel%03d"%(channelNumber+1) ]
		for name in possibleNames :
			register = self.getRegister(name)
		if register==None :
			raise Exception( "Nothing known about channel "+str(channelNumber) )
		register.value=value

	def getChannelTrim( self, channelNumber ) :
		"""
		Returns the value in register "Channel<channelNumber+1>".
		
		Note that the argument starts counting from zero, whereas the register string
		name starts counting from 1. So calling getChannelTrim( 23 ) will return the
		value of the register named "Channel024". This is because all other code regarding
		channels starts counting from zero.
		"""
		# I don't know of the channel numbers are padded with zeros in the register
		# name, so I'll try a few possibilities
		possibleNames = [ "Channel%d"%(channelNumber+1), "Channel%02d"%(channelNumber+1), "Channel%03d"%(channelNumber+1) ]
		for name in possibleNames :
			register = self.getRegister(name)
		if register==None :
			raise Exception( "Nothing known about channel "+str(channelNumber) )
		return register.value
	
	def writeToFilename( self, filename, registerNames=None ) :
		"""
		Writes all currently held values to the given filename. If registerNames is specified only those
		registers are saved.
		"""
		file = open( filename, 'w+' )
		for register in self.registers :
			shouldWrite=True
			if registerNames != None :
				if registerNames.count(register.name)==0 : shouldWrite=False
			if shouldWrite : register.writeToFile(file)
		file.close()

	def writeTrimsToFilename( self, filename ) :
		file = open( filename, 'w+' )
		for register in self.registers :
			if register.name[0:7]=='Channel' :register.writeToFile(file)
		file.close()
