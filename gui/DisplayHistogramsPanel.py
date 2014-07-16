from pyjamas.ui.HorizontalPanel import HorizontalPanel
from pyjamas.ui.VerticalPanel import VerticalPanel
from pyjamas.ui.ListBox import ListBox
from pyjamas.ui.Button import Button
from pyjamas.ui.Image import Image
from pyjamas.ui import HasHorizontalAlignment

from GlibRPCService import GlibRPCService
from DataRunManager import DataRunManager
from ErrorMessage import ErrorMessage

class DisplayHistogramsView(object) :
	"""
	@brief View in the MVP pattern for displaying histograms.

	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 09/Feb/2014
	"""
	def __init__( self ) :
		self.cbcList=ListBox(MultipleSelect=True, VisibleItemCount=4)
		self.channelList=ListBox(MultipleSelect=True, VisibleItemCount=20)
		self.updateButton=Button("Update")

		controls=VerticalPanel()
		controls.add(self.updateButton)
		controls.add(self.cbcList)
		controls.add(self.channelList)

		controls.setCellHorizontalAlignment( self.updateButton, HasHorizontalAlignment.ALIGN_CENTER )
		self.cbcList.setWidth("95%")
		self.channelList.setWidth("95%")

		self.cbcList.addItem( "waiting..." )
		for index in range(0,254) :
			self.channelList.addItem( "Channel %3d"%index )

		self.histogram = Image()
		self.mainPanel = HorizontalPanel()
		self.mainPanel.add( controls )
		self.mainPanel.add( self.histogram )
		self.histogram.setUrl( "defaultScurveHistogram.png" )

	def getPanel( self ) :
		return self.mainPanel

	def setAvailableCBCs( self, cbcNames ) :
		self.cbcList.clear()
		for name in cbcNames :
			self.cbcList.addItem( name )

	def enable( self ) :
		self.updateButton.setEnabled(True)
		self.cbcList.setEnabled(True)
		self.channelList.setEnabled(True)

	def disable( self ) :
		self.updateButton.setEnabled(False)
		self.cbcList.setEnabled(False)
		self.channelList.setEnabled(False)
	
	def getUpdateButton( self ) :
		return self.updateButton
	
	def getSelectedCBCChannels( self ) :
		"""
		Returns a dictionary of which channels are selected, with CBC name as a key and
		an array of the channels for that CBC as the value.
		"""
		# The way this view is currently set up, the selected channels have to be the same
		# for each selected CBC.
		selectedChannels=[]
		for index in range(self.channelList.getItemCount()) :
			if self.channelList.isItemSelected(index) :
				selectedChannels.append(index)
		returnValue={}
		for index in range(self.cbcList.getItemCount()) :
			if self.cbcList.isItemSelected(index) : returnValue[self.cbcList.getItemText(index)]=selectedChannels

		return returnValue

	def setImage( self, url ) :
		self.histogram.setUrl( url )

class DisplayHistogramsPanel(object) :
	"""
	@brief GUI panel that allows the user to select channels and display the histogram for those channels.

	This class tries to follow the MVP pattern (Model-View-Presenter), where this class
	is basically the presenter, and DisplayHistogramsView is the view.

	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 09/Feb/2014
	"""
	def __init__( self, inputRootFile="/tmp/histograms.root", outputFile="scurveHistogram.png" ) :
		self.inputRootFile=inputRootFile
		self.outputFile=outputFile
		self.rpcService = GlibRPCService.instance()
		self.view = DisplayHistogramsView()
		# Query the RPC service to see which CBCs are connected
		self.connectedCBCs=[]
		self.rpcService.connectedCBCNames( None, self )
		# Bind the update button so that I can initiate the RPC call
		self.view.getUpdateButton().addClickListener( self )
		# Ask to receive notifications when data is being taken so that I can disable the button
		DataRunManager.instance().registerEventHandler( self )

	def setInputRootFile( self, inputRootFile ) :
		self.inputRootFile=inputRootFile

	def setOutputFile( self, outputFile ) :
		self.outputFile=outputFile

	def onDataTakingEvent( self, eventCode, details ) :
		"""
		Method that receives updates from DataRunManager
		"""
		if eventCode==DataRunManager.DataTakingStartedEvent :
			self.view.disable()
		elif eventCode==DataRunManager.DataTakingFinishedEvent :
			self.view.enable()

	def onRemoteResponse(self, response, request_info):
		"""
		The handler for successful responses to RPC calls.
		"""
		if request_info.method=="connectedCBCNames" : self._onConnectedCBCNamesResponse(response)
		elif request_info.method=="createHistogram" : self._onCreateHistogramResponse(response)
		else : ErrorMessage( "Received an unexpected response for method "+request_info.method )

	def onRemoteError(self, code, message, request_info):
		"""
		The method that gets called after an unsuccessful RPC call.
		"""
		ErrorMessage( "Unable to contact server: "+str(message) )

	def _onConnectedCBCNamesResponse( self, response ) :
		self.connectedCBCs=response
		self.view.setAvailableCBCs( response )

	def _onCreateHistogramResponse( self, response ) :
		self.view.setImage(self.outputFile)
		self.view.enable()

	def onClick( self, sender ) :
		try:
			if sender==self.view.getUpdateButton() : self.updateHistogram()
		except Exception as error:
			ErrorMessage( "Client error: "+str(error) )

	def updateHistogram( self ) :
		if self.inputRootFile==None :
			ErrorMessage( "The source root file has not been set" )
			return
		selectedChannels=self.view.getSelectedCBCChannels()
		totalChannels=0
		for cbcName in selectedChannels.keys() :
			totalChannels+=len(selectedChannels[cbcName])
		if totalChannels==0 :
			ErrorMessage( "No channels are selected. Select some channels." )
			return
		parameters={}
		parameters['inputFilename']=self.inputRootFile
		parameters['outputFilename']=self.outputFile
		# Need to format the channels into the same format that the RPC service is expecting
		parameters['cbcChannelRange']=[]
		for cbcName in self.connectedCBCs :
			try :
				parameters['cbcChannelRange'].append( selectedChannels[cbcName] )
			except KeyError :
				# The RPC method expects an empty array for CBCs where you don't want channels
				parameters['cbcChannelRange'].append( [] )
		self.view.disable()
		self.rpcService.createHistogram( parameters, self )

	def getPanel( self ) :
		return self.view.getPanel()
