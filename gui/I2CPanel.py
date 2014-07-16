# Pyjamas panel to read and set I2C registers on multiple CBCs.
#
# The list of available CBCs will be on the left, when one is selected the values will be
# displayed on the right.
#
# @author Mark Grimes (mark.grimes@bristol.ac.uk)
# @date 15/Jan/2014

from pyjamas.ui.HorizontalPanel import HorizontalPanel
from pyjamas.ui.VerticalPanel import VerticalPanel
from pyjamas.ui.FlowPanel import FlowPanel
from pyjamas.ui.DisclosurePanel import DisclosurePanel
from pyjamas.ui.ListBox import ListBox
from pyjamas.ui.Label import Label
from pyjamas.ui.TextBox import TextBox
from ErrorMessage import ErrorMessage
from pyjamas.ui.CheckBox import CheckBox
from pyjamas.ui.Button import Button
from pyjamas.ui.HTML import HTML
from pyjamas.ui import HasHorizontalAlignment

from pyjamas.Timer import Timer
from datetime import datetime

from GlibRPCService import GlibRPCService

class I2CPanel :
	
	class saveStateListener :
		def __init__(self, panel) :
			self._saveStatePanel=panel	
			
		def onRemoteResponse(self, response, request_info):
			self._saveStatePanel.returnStatement.setText("File saved:" + response)
			
		def onRemoteError(self, code, message, request_info):
			ErrorMessage( "Unable to contact server" )
			
	
	class loadStateListener :
		def __init__(self, panel) :
			self._saveStatePanel=panel	
			
		def onRemoteResponse(self, response, request_info):
			self._saveStatePanel.returnStatement.setText("File loaded:" + response)
			self._saveStatePanel.rpcService.I2CRegisterValues( self._saveStatePanel.getTotalCBCs(), I2CPanel.ReadRegisterValueListener(self._saveStatePanel) ) #refresh of text boxes

		def onRemoteError(self, code, message, request_info):
			ErrorMessage( "Unable to contact server" )
	
	class ConnectedCBCListener :
		"""
		A class to listen for the response to the connectedCBCNames call
		"""
		def __init__(self, listBox) :
			self.listBox=listBox
			
		def onRemoteResponse(self, response, request_info):
			self.listBox.clear()
			if response == None:
				self.listBox.addItem("<none>")
				self.listBox.setEnabled( False )
			else :
				for name in response :
					self.listBox.addItem(name) #e.g FE0CBC0
				self.listBox.setEnabled(True)

		def onRemoteError(self, code, message, request_info):
			ErrorMessage( "Unable to contact server" )

	class ReadRegisterValueListener :
		"""
		A class to listen for the response to the call to get the register values
		"""
		def __init__(self, panel) :
			self._I2Cpanel=panel
			self.textBoxes=self._I2Cpanel.i2cValueEntries
			
		def onRemoteResponse(self, response, request_info):
			#Call is now made for all connected CBCs for better stability of status box- fb
			for cbcName in self._I2Cpanel.getActiveCBCs():
				if cbcName=='FE0CBC0':
					valuesTuple = response[response.keys()[0]]
				elif cbcName=='FE0CBC1':
					valuesTuple = response[response.keys()[1]]
			# For this chip loop over all the register and set the text box values
			for registerName in valuesTuple :
				box=self.textBoxes[registerName]
				status=self._I2Cpanel.statusValueEntries[registerName]
				box.setText( "0x%02x"%valuesTuple[registerName] )
				box.setStyleAttribute( "background-color", "#FFFFFF" )
				box.setEnabled( True )
			#for some reason pyjas likes these separated - fb	
			for registerName in valuesTuple:
				if response['FE0CBC0'][registerName]==response['FE0CBC1'][registerName]:
					self._I2Cpanel.statusValueEntries[registerName].setText("==")
				else:
					self._I2Cpanel.statusValueEntries[registerName].setText(" //")
		def onRemoteError(self, code, message, request_info):
			ErrorMessage( "Unable to contact server" )

	class RefreshListener :
		"""
		A class that will wait for a response before refreshing the status box
		"""
		def __init__(self, panel) :
			self._Refresh = panel
		
		def onRemoteResponse(self, response, request_info):
			self._Refresh.rpcService.I2CRegisterValues( self._Refresh.getTotalCBCs(), I2CPanel.ReadRegisterValueListener(self._Refresh) )	 #Live refresh of the status box
			

		def onRemoteError(self, code, message, request_info):
			ErrorMessage( "Unable to contact server" )

	def __init__( self ) :
		# This is the service that will be used to communicate with the DAQ software
		self.rpcService = GlibRPCService.instance()
		# The main panel that everythings will be insided
		self.mainPanel = HorizontalPanel()
		self.mainPanel.setSpacing(10)
		self.i2cValueEntries = {} # The input boxes for all the registers
		self.statusValueEntries = {} # Displays the status of the i2cValueEntries
		
		self.stateValueEntries = {} # For load/save state
		self.fileName = {} # File name of the i2c registers
		
		# This is the list of available CBC chips. It will be populated by a call to the
		# server later.
		self.cbcList=ListBox(MultipleSelect=True, VisibleItemCount=10)
		self.cbcList.addItem( "waiting..." ) # Default text until I hear from the server what's connected
		self.cbcList.setEnabled( False )
		self.cbcList.addChangeListener(self)
		self.rpcService.connectedCBCNames( None, I2CPanel.ConnectedCBCListener(self.cbcList) ) # Ask the server what's connected
		
		
		# This is the panel that will have the list of I2C registers. I'll split up the
		# registers into subjects to make them more manageable.
		self.mainSettings = DisclosurePanel("Main Control Registers")
		self.channelMasks = DisclosurePanel("Channel Masks")
		self.channelTrims = DisclosurePanel("Channel Trims")
		self.callSettings = VerticalPanel("Load/Save States")
		self.callSettings.setBorderWidth(1)

		self.callSettings.add(HTML("<center>Load/Save State</center>"))
		
		cbcListAndCallSettings=VerticalPanel()
		cbcListAndCallSettings.add(self.cbcList)
		cbcListAndCallSettings.add(self.callSettings)
		self.mainPanel.add(cbcListAndCallSettings)
		self.mainPanel.add(self.mainSettings)
		self.mainPanel.add(self.channelMasks)
		self.mainPanel.add(self.channelTrims)
		
		self.callSettings.add( self.createStatesPanel())
		self.mainSettings.add( self.createRegisterPanel(["FrontEndControl","TriggerLatency","HitDetectSLVS","Ipre1","Ipre2","Ipsf","Ipa","Ipaos","Vpafb","Icomp","Vpc","Vplus","VCth","TestPulsePot","SelTestPulseDel&ChanGroup","MiscTestPulseCtrl&AnalogMux","TestPulseChargePumpCurrent","TestPulseChargeMirrCascodeVolt","CwdWindow&Coincid","MiscStubLogic"]) )
		self.channelMasks.add( self.createRegisterPanel(["MaskChannelFrom008downto001","MaskChannelFrom016downto009","MaskChannelFrom024downto017","MaskChannelFrom032downto025","MaskChannelFrom040downto033","MaskChannelFrom048downto041","MaskChannelFrom056downto049","MaskChannelFrom064downto057","MaskChannelFrom072downto065","MaskChannelFrom080downto073","MaskChannelFrom088downto081","MaskChannelFrom096downto089","MaskChannelFrom104downto097","MaskChannelFrom112downto105","MaskChannelFrom120downto113","MaskChannelFrom128downto121","MaskChannelFrom136downto129","MaskChannelFrom144downto137","MaskChannelFrom152downto145","MaskChannelFrom160downto153","MaskChannelFrom168downto161","MaskChannelFrom176downto169","MaskChannelFrom184downto177","MaskChannelFrom192downto185","MaskChannelFrom200downto193","MaskChannelFrom208downto201","MaskChannelFrom216downto209","MaskChannelFrom224downto217","MaskChannelFrom232downto225","MaskChannelFrom240downto233","MaskChannelFrom248downto241","MaskChannelFrom254downto249"]) )
		self.channelTrims.add( self.createRegisterPanel(["Channel001","Channel002","Channel003","Channel004","Channel005","Channel006","Channel007","Channel008","Channel009","Channel010","Channel011","Channel012","Channel013","Channel014","Channel015","Channel016","Channel017","Channel018","Channel019","Channel020","Channel021","Channel022","Channel023","Channel024","Channel025","Channel026","Channel027","Channel028","Channel029","Channel030","Channel031","Channel032","Channel033","Channel034","Channel035","Channel036","Channel037","Channel038","Channel039","Channel040","Channel041","Channel042","Channel043","Channel044","Channel045","Channel046","Channel047","Channel048","Channel049","Channel050","Channel051","Channel052","Channel053","Channel054","Channel055","Channel056","Channel057","Channel058","Channel059","Channel060","Channel061","Channel062","Channel063","Channel064","Channel065","Channel066","Channel067","Channel068","Channel069","Channel070","Channel071","Channel072","Channel073","Channel074","Channel075","Channel076","Channel077","Channel078","Channel079","Channel080","Channel081","Channel082","Channel083","Channel084","Channel085","Channel086","Channel087","Channel088","Channel089","Channel090","Channel091","Channel092","Channel093","Channel094","Channel095","Channel096","Channel097","Channel098","Channel099","Channel100","Channel101","Channel102","Channel103","Channel104","Channel105","Channel106","Channel107","Channel108","Channel109","Channel110","Channel111","Channel112","Channel113","Channel114","Channel115","Channel116","Channel117","Channel118","Channel119","Channel120","Channel121","Channel122","Channel123","Channel124","Channel125","Channel126","Channel127","Channel128","Channel129","Channel130","Channel131","Channel132","Channel133","Channel134","Channel135","Channel136","Channel137","Channel138","Channel139","Channel140","Channel141","Channel142","Channel143","Channel144","Channel145","Channel146","Channel147","Channel148","Channel149","Channel150","Channel151","Channel152","Channel153","Channel154","Channel155","Channel156","Channel157","Channel158","Channel159","Channel160","Channel161","Channel162","Channel163","Channel164","Channel165","Channel166","Channel167","Channel168","Channel169","Channel170","Channel171","Channel172","Channel173","Channel174","Channel175","Channel176","Channel177","Channel178","Channel179","Channel180","Channel181","Channel182","Channel183","Channel184","Channel185","Channel186","Channel187","Channel188","Channel189","Channel190","Channel191","Channel192","Channel193","Channel194","Channel195","Channel196","Channel197","Channel198","Channel199","Channel200","Channel201","Channel202","Channel203","Channel204","Channel205","Channel206","Channel207","Channel208","Channel209","Channel210","Channel211","Channel212","Channel213","Channel214","Channel215","Channel216","Channel217","Channel218","Channel219","Channel220","Channel221","Channel222","Channel223","Channel224","Channel225","Channel226","Channel227","Channel228","Channel229","Channel230","Channel231","Channel232","Channel233","Channel234","Channel235","Channel236","Channel237","Channel238","Channel239","Channel240","Channel241","Channel242","Channel243","Channel244","Channel245","Channel246","Channel247","Channel248","Channel249","Channel250","Channel251","Channel252","Channel253","Channel254","ChannelDummy"]) )
		
		self.mainSettings.add()
		self.echo=Label()
		self.mainPanel.add(self.echo)

	def getPanel( self ) :
		return self.mainPanel
	
	def onChange( self, sender ) :
		
		if sender == self.cbcList :
			# Make a call to the RPC service to get the register values
			self.rpcService.I2CRegisterValues( self.getTotalCBCs(), I2CPanel.ReadRegisterValueListener(self) )#fb - sends all CBCs
			self.load.setEnabled(True)
			self.save.setEnabled(True)
		elif sender == self.save:
			msg = self.saveFileName.getText()
			self.rpcService.saveI2cRegisterValues(msg, I2CPanel.saveStateListener(self) )
		elif sender == self.load:
			msg = self.loadFileName.getText()
			self.rpcService.loadI2cRegisterValues(msg, I2CPanel.loadStateListener(self) )	
			self.rpcService.I2CRegisterValues( self.getTotalCBCs(), I2CPanel.ReadRegisterValueListener(self) )#
				
		# Sender must be a text box. Need to format the input.
		else : 
			try:
			# For some reason the '0x' at the start of the string is causing exceptions,
			# even though it works fine with interactive python. I'll take it off anyway.
				string=sender.getText()
				if( len(string)>=2 ) :
					if string[0]=='0' and string[1]=='x' : string=string[2:]
					value=int( string, 16 ) # convert as hex
					# Cap values at 255
					if value<0 : value=0
					if value>255 : value=255
					sender.setStyleAttribute( "background-color", "#FFFFFF" )
			# Convert to the same format as everything else
				sender.setText( "0x%02x"%value )
			# Send the change to the RPC service
				messageParameters = {}
				for cbcName in self.getActiveCBCs() :
					messageParameters[cbcName]={ sender.getTitle():value }
					self.rpcService.setI2CRegisterValues( messageParameters, I2CPanel.RefreshListener(self) )
			
			except ValueError:
				sender.setStyleAttribute( "background-color", "#FF3333" )		
		
		#self.echoSelection()
	
	def echoSelection(self): #fb - a good "print screen" method
		msg = " File saved: "
		for names in self.getCheckedStates():
			msg += names
		self.echo.setText(msg)	
			
	def getList(self):
		selectCBCs = []
		for i in range(self.cbcList.getItemCount()) :
			if self.cbcList.isItemSelected(i):
				selectedCBCs.append(self.cbcList.getItemText(i))
				
	def getTotalCBCs(self) : #fb
		totalCBCs = []
		for i in range(self.cbcList.getItemCount()) :
			totalCBCs.append(self.cbcList.getItemText(i))
		return totalCBCs
	
	def getSpecificCBC(self, i): #fb
		specificCBC = []
		specificCBC.append(self.cbcList.getItemText(i))
		return specificCBC
		

	def getActiveCBCs(self) :
		selectedCBCs = []
		for i in range(self.cbcList.getItemCount()) :
			if self.cbcList.isItemSelected(i):
				selectedCBCs.append(self.cbcList.getItemText(i))
		return selectedCBCs
	
	def getCheckedStates(self): # returns the checked boxes + filename
		selectedStates = []
		#for names in self.stateValueEntries:
			#if str(self.stateValueEntries[names].isChecked())=="True":
			#	selectedStates.append(names)
		selectedStates.append(self.saveFileName.getText())
		return selectedStates
		
	def createRegisterPanel( self, registerNames ) :
		"""
		Creates panels and buttons for everything given in registerNames, and returns the main panel.
		"""
		flowPanel=FlowPanel()
		for buttonName in registerNames :
			newPanel=HorizontalPanel()
			label=Label(buttonName)
			newPanel.add( label )
			newTextBox=TextBox()
			newTextBox.setEnabled(False)
			newTextBox.setWidth(80)
			statusBox=TextBox()
			statusBox.setEnabled(False)
			statusBox.setWidth(30)
			newPanel.add(newTextBox)
			newPanel.add(statusBox)
			newPanel.setCellHorizontalAlignment( newTextBox, HasHorizontalAlignment.ALIGN_RIGHT )
			newPanel.setCellHorizontalAlignment( statusBox, HasHorizontalAlignment.ALIGN_RIGHT )
			newPanel.setCellWidth( statusBox, "20px" )
			newPanel.setWidth("100%")
			#newPanel.setStyleName("areaStyle");
			#newPanel.setBorderWidth(5);
			
			newTextBox.setText("select chip...")
			newTextBox.addChangeListener(self)
			newTextBox.setTitle(buttonName) # This isn't displayed, but it's useful to have stored
			
			self.i2cValueEntries[buttonName]=newTextBox	
			
			self.statusValueEntries[buttonName]=statusBox
			statusBox.setTitle(buttonName)
			statusBox.setText("...")
			
			flowPanel.add(newPanel)


		return flowPanel
	
	def createStatesPanel(self):
		vertPanel=VerticalPanel()
		vertPanel.setSpacing(10)
		
		selectionNames = (["Main Control", "Masks", "Trims"])	
		registerSelection = VerticalPanel()
		
		for name in selectionNames :
			checkBox = CheckBox(name)
			checkBox.setTitle(name)
			self.stateValueEntries[name]=checkBox
			registerSelection.add(checkBox)
		
		#Tidy up 
		loadPanel = HorizontalPanel()
		loadFileTextBox = TextBox()
		loadFileTextBox.setText("MyI2cCfg")
		loadFileTextBox.setWidth(80)
		self.loadFileName = loadFileTextBox
		loadPanel.add(loadFileTextBox)
		self.load = Button("Load",getattr(self,"onChange")) #overrides default and sends it to onChange
		loadPanel.add(self.load)
		self.load.setEnabled(False)
		
		savePanel = HorizontalPanel()
		saveFileTextBox = TextBox()
		saveFileTextBox.setText("MyI2cCfg")
		saveFileTextBox.setWidth(80)
		self.saveFileName = saveFileTextBox
		savePanel.add(saveFileTextBox)
		self.save = Button("Save", getattr(self,"onChange"))
		savePanel.add(self.save)
		self.save.setEnabled(False)
		
		self.returnStatement = Label()
		
		vertPanel.add(registerSelection)
		vertPanel.add(loadPanel)
		vertPanel.add(savePanel)
		vertPanel.add(self.returnStatement)
		
		return vertPanel

