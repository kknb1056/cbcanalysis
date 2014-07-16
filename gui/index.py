# Client example from Amund Tveit's blog
# http://amundblog.blogspot.co.at/2008/12/ajax-with-python-combining-pyjs-and.html

# note: ui and JSONService were not prefixed with pyjamas, but that's needed
from pyjamas import Window
from pyjamas.ui.RootPanel import RootPanel
from pyjamas.ui.DockPanel import DockPanel
from pyjamas.ui.DisclosurePanel import DisclosurePanel
from pyjamas.ui.TextArea import TextArea
from pyjamas.ui.Label import Label
from pyjamas.ui.Button import Button
from pyjamas.ui.HTML import HTML
from pyjamas.ui.VerticalPanel import VerticalPanel
from pyjamas.ui.HorizontalPanel import HorizontalPanel
from pyjamas.ui.FlowPanel import FlowPanel
from pyjamas.ui.ListBox import ListBox
from pyjamas.ui.TextBox import TextBox
from pyjamas.JSONService import JSONProxy
from pyjamas.ui.CSS import StyleSheetCssFile
from pyjamas.ui.DialogBox import DialogBox
from pyjamas.ui import HasHorizontalAlignment

from I2CPanel import I2CPanel
from OccupancyCheckPanel import OccupancyCheckPanel
from SCurveRunPanel import SCurveRunPanel
from CalibrateChannelTrimsPanel import CalibrateChannelTrimsPanel
from DataRunManager import DataRunManager
from ErrorMessage import ErrorMessage

class Client:
	def onModuleLoad(self):
		#Window.setTitle("CBC Test Stand")
		StyleSheetCssFile("styleSheet.css")
		
		self.TEXT_WAITING = "Waiting for response..."
		self.TEXT_ERROR = "Server Error"

		self.status=Label()
		
		# This is the remote service
		self.I2CPanel=I2CPanel()
		self.SCurveRunPanel=SCurveRunPanel()
		self.OccupancyCheckPanel=OccupancyCheckPanel()
		self.CalibrateChannelTrimsPanel=CalibrateChannelTrimsPanel()

		# mainPanel will have all of the working stuff in it
		self.mainPanel=DockPanel()
		#self.mainPanel.setSpacing(10)
		titleBar=HorizontalPanel()
		titleBar.add( HTML(r"CBC Test Stand (v1.1)", StyleName="titleStyle") )
		self.stopTakingDataButton=Button("Stop taking data")
		self.stopTakingDataButton.addClickListener(self)
		self.dataTakingPercentage=HTML('0%')
		self.dataTakingStatus=HTML('Initiating...')
		titleBar.add(self.dataTakingPercentage)
		titleBar.add(self.dataTakingStatus)
		titleBar.add(self.stopTakingDataButton)
		titleBar.setCellHorizontalAlignment( self.dataTakingStatus, HasHorizontalAlignment.ALIGN_RIGHT )
		titleBar.setCellHorizontalAlignment( self.dataTakingPercentage, HasHorizontalAlignment.ALIGN_RIGHT )
		titleBar.setCellHorizontalAlignment( self.stopTakingDataButton, HasHorizontalAlignment.ALIGN_RIGHT )
		titleBar.setWidth("100%")
		self.mainPanel.add( titleBar, DockPanel.NORTH )
		selectionPanel=VerticalPanel()
		
		# Register to get updates about the status of data taking, so that
		# I can update the information in the title bar
		self.dataRunManager=DataRunManager.instance()
		self.dataRunManager.registerEventHandler( self )
		
		self.activePanelButton=None
		self.activePanel=None

		self.registersButton=Label("I2C Registers", StyleName="areaStyle")
		self.occupanciesButton=Label("Test Occupancies", StyleName="areaStyle")
		self.scurveButton=Label("S-Curve Run", StyleName="areaStyle")
		self.calibrateTrimsButton=Label("Calibrate Trims", StyleName="areaStyle")

		self.registersButton.addClickListener( self )
		self.occupanciesButton.addClickListener( self )
		self.scurveButton.addClickListener( self )
		self.calibrateTrimsButton.addClickListener( self )
		
		selectionPanel.add( self.registersButton )
		selectionPanel.add( self.occupanciesButton )
		selectionPanel.add( self.scurveButton )
		selectionPanel.add( self.calibrateTrimsButton )

		self.mainPanel.add( selectionPanel, DockPanel.WEST )
		
		self.mainPanel.add( self.status, DockPanel.SOUTH )
		RootPanel().add(self.mainPanel)

		self.setNewMainPanel( self.registersButton )
		
	def onDataTakingEvent( self, eventCode, details ) :
		"""
		Method that receives updates from DataRunManager
		"""
		if eventCode==DataRunManager.DataTakingStartedEvent :
			self.stopTakingDataButton.setEnabled(True)
			self.dataTakingPercentage.setText("0%")
			self.dataTakingStatus.setText( "Starting run..." )
		elif eventCode==DataRunManager.DataTakingFinishedEvent :
			self.stopTakingDataButton.setEnabled(False)
			self.dataTakingPercentage.setText("")
			self.dataTakingStatus.setText( "Not taking data" )
		elif eventCode==DataRunManager.DataTakingStatusEvent :
			self.stopTakingDataButton.setEnabled(True)
			self.dataTakingPercentage.setText("%3d%%"%int(details['fractionComplete']*100+0.5) )
			self.dataTakingStatus.setText( details['statusString'] )

	def onClick(self, sender):
		# (data, response_class): if the latter is 'self', then
		# the response is handled by the self.onRemoteResponse() method
		try:
			if sender == self.stopTakingDataButton :
				self.dataRunManager.stopTakingData()
			else :
				# I don't have any other buttons so it must be a panel change
				self.setNewMainPanel( sender )

		except Exception as error:
			self.status.setText("Client exception was thrown: '"+str(error.__class__)+"'='"+str(error)+"'")

	def setNewMainPanel( self, panelButton ) :
		if panelButton == self.activePanelButton : return # already the active panel so no need to do anything
		
		# Remove the "selected" style from the current button
		if self.activePanelButton!=None : self.activePanelButton.setStyleName( "areaStyle" )
		
		# Set the "selected" style on the new one
		self.activePanelButton=panelButton
		self.activePanelButton.setStyleName( "selectedAreaStyle" )
		
		# Clear the main panel
		if self.activePanel!=None :
			self.mainPanel.remove( self.activePanel.getPanel() )
		
		# Figure out what the new main panel should be
		if panelButton==self.registersButton : self.activePanel=self.I2CPanel
		elif panelButton==self.scurveButton : self.activePanel=self.SCurveRunPanel
		elif panelButton==self.occupanciesButton : self.activePanel=self.OccupancyCheckPanel
		elif panelButton==self.calibrateTrimsButton : self.activePanel=self.CalibrateChannelTrimsPanel
		
		# Set the new main panel
		self.activePanel.getPanel().setStyleName( "selectedAreaStyle" )
		self.activePanel.getPanel().setWidth("100%")
		self.mainPanel.add( self.activePanel.getPanel(), DockPanel.CENTER )

	def onRemoteResponse(self, response, request_info):
		self.status.setText(response)

	def onRemoteError(self, code, message, request_info):
		ErrorMessage( "Unable to contact server" )


if __name__ == "__main__" :
	app = Client()
	app.onModuleLoad()

