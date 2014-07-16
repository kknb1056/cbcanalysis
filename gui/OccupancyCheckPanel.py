# Pyjamas panel to instigate a quick 100 event run and report back the occupancies.
#
# @author Mark Grimes (mark.grimes@bristol.ac.uk)
# @date 17/Jan/2014

from pyjamas.ui.VerticalPanel import VerticalPanel
from pyjamas.ui.HorizontalPanel import HorizontalPanel
from pyjamas.ui.HTML import HTML
from pyjamas.ui.Grid import Grid
from pyjamas.ui.Button import Button

from ErrorMessage import ErrorMessage
from GlibRPCService import GlibRPCService
from DataRunManager import DataRunManager

class OccupancyCheckView :
	"""
	A class that takes of the purely UI part of the OccupanceCheckPanel.
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 08/Feb/2014
	"""
	def __init__( self ) :
		self.mainPanel = VerticalPanel()
		self.echo = HTML('Initiating')
		self.launchButton=Button("Update")
		controlPanel=HorizontalPanel()
		controlPanel.add(self.launchButton)
		controlPanel.add(self.echo)
		self.mainPanel.add(controlPanel)
		self.resultGrids={}
		self.resultLabels={}
	
	def createResultGrid( self, gridName ) :
		label=HTML('<br><b>'+gridName+'</b>')
		grid=Grid(17,17)
		# Create the column and row headers
		for index in range( 1, grid.getColumnCount() ) :
			grid.setWidget( 0, index, HTML('Cx%X'%(index-1)) )
		for index in range( 1, grid.getRowCount() ) :
			grid.setWidget( index, 0, HTML('C%Xx'%(index-1)) )
		self.mainPanel.add(label)
		self.mainPanel.add(grid)
		self.resultLabels[gridName]=label
		self.resultGrids[gridName]=grid

	def getPanel( self ) :
		return self.mainPanel

	def getUpdateButton( self ) :
		return self.launchButton

	def enable( self ) :
		self.launchButton.setEnabled(True)

	def disable( self ) :
		self.launchButton.setEnabled(False)

	def clearResults( self ) :
		for name in self.resultGrids.keys() :
			grid=self.resultGrids[name]
			# Loop over all the data cells and set them empty
			for column in range( 1, grid.getColumnCount() ) :
				for row in range( 1, grid.getRowCount() ) :
					grid.setWidget( row, column, HTML('') )

	def addResult( self, cbcName, occupancies ) :
		"""
		Display the occupancies for a CBC.
		cbcName - string describing naming which CBC it is
		occupancies - an array of length 254 where each entry is the occupancy for that channel
		"""
		if occupancies==None : return self.addError( cbcName )

		try :
			grid=self.resultGrids[cbcName]
		except NameError :
			self.createResultGrid( cbcName )
			grid=self.resultGrids[cbcName]

		# Might need to reset the label if the was an error earlier
		label=self.resultLabels[cbcName]
		label.setHTML( '<br><b>'+cbcName+'</b>' )
		
		row=1
		column=1
		for index in range(0,len(occupancies)) :
			# Work out RGB components so that it is completely red when occupancy is zero, and green when one
			red=255.0*(1.0-occupancies[index])
			green=255.0*occupancies[index]
			blue=0
			grid.setWidget( row, column, HTML('<div style="background-color:#%02X%02X%02X'%(red,green,blue)+'">%1.2f'%occupancies[index]+'</div>') )
			column+=1
			if column%17 == 0 :
				column=1
				row+=1

	def addError( self, cbcName ) :
		"""
		Displays something to indicate that there was an error for the given CBC
		"""
		try :
			label=self.resultLabels[cbcName]
		except NameError :
			self.createResultGrid( cbcName )
			label=self.resultLabels[cbcName]
		label.setHTML( '<br>Unable to get the results for <b>'+cbcName+'</b>' )

	def setEchoMessage( self, message ) :
		self.echo.setText( message )

class OccupancyCheckPanel :
	"""
	GUI panel that allows a user to start a simple 100 event run and displays the occupancies.
	This is a simple way of checking the current settings.

	Attempts to use the MVP (Model-View-Presenter) pattern, where the presenter is this
	class and the the view is OccupancyCheckView.

	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 05/Feb/2014
	"""
	def __init__( self ) :
		# This is the service that will be used to communicate with the DAQ software
		self.rpcService = GlibRPCService.instance()
		self.dataRunManager = DataRunManager.instance()
		self.view = OccupancyCheckView()
		# Ask the server what's connected
		self.rpcService.connectedCBCNames( None, self )
		# Tell the DataRunManager that I want to be told when anything starts or stops
		# taking data. When anything happens the onDataTakingEvent method will be called.
		self.dataRunManager.registerEventHandler( self )
		
		# Bind the Update button
		self.view.getUpdateButton().addClickListener(self)
		self.view.setEchoMessage("No data.")
			
	def onClick(self, sender):
		try:
			if sender==self.view.getUpdateButton() :
				self.view.setEchoMessage("Starting data run")
				self.dataRunManager.startOccupancyCheck()
		except Exception as error :
			ErrorMessage( "Exception thrown: "+str(error) )

	def onDataTakingEvent( self, eventCode, details ) :
		"""
		Method that receives updates from DataRunManager
		"""
		if eventCode==DataRunManager.DataTakingStartedEvent :
			self.view.setEchoMessage("Taking data... ")
			self.view.disable()
		elif eventCode==DataRunManager.DataTakingFinishedEvent :
			self.view.setEchoMessage("Data taking finished")
			self.update()
			self.view.enable()

	def onRemoteResponse(self, response, request_info) :
		"""
		The handler for successful responses to RPC calls.
		"""
		if request_info.method=="getOccupancies" : self._onGetOccupanciesResponse(response)
		elif request_info.method=="connectedCBCNames" : self._onConnectedCBCNamesResponse(response)
		else : ErrorMessage( "Received an unexpected response for method "+request_info.method )

	def onRemoteError(self, code, message, request_info):
		"""
		The method that gets called after an unsuccessful RPC call.
		"""
		ErrorMessage( "Unable to contact server: "+str(message) )
 
	def _onGetOccupanciesResponse( self, response ) :
		"""
		Handles the response to a RPC call. Separate method for code layout only.
		"""
		self.view.clearResults()
		for cbcName in response.keys() :
			self.view.addResult( cbcName, response[cbcName] )
		self.view.setEchoMessage("Results refreshed")

	def _onConnectedCBCNamesResponse( self, response ) :
		"""
		Handles the response to a RPC call. Separate method for code layout only.
		"""
		for name in response :
			self.view.createResultGrid(name) #e.g FE0CBC0
	
	def update( self ) :
		self.rpcService.getOccupancies( None, self );
		
	def getPanel( self ) :
		return self.view.getPanel()

