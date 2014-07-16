# Pyjamas panel to take several runs that try to modify the channel trims
# until the s-curves converge on one point.
#
# @author Mark Grimes (mark.grimes@bristol.ac.uk)
# @date 10/Feb/2014

from pyjamas.ui.HorizontalPanel import HorizontalPanel
from pyjamas.ui.VerticalPanel import VerticalPanel
from pyjamas.ui.TextBox import TextBox
from pyjamas.ui.Button import Button
from pyjamas.ui.HTML import HTML
from pyjamas.ui import HasHorizontalAlignment

from ErrorMessage import ErrorMessage
from DataRunManager import DataRunManager

class CalibrateChannelTrimsView(object) :
	"""
	Class that takes care of the purely UI parts of the trim calibration
	"""
	def __init__( self ) :
		numberOfLoopsPanel=HorizontalPanel()
		numberOfLoopsPanel.add( HTML("Maximum number of loops") )
		self.maximumNumberOfLoops=TextBox()
		self.maximumNumberOfLoops.setText(10)
		numberOfLoopsPanel.add( self.maximumNumberOfLoops )
		numberOfLoopsPanel.setCellHorizontalAlignment( self.maximumNumberOfLoops, HasHorizontalAlignment.ALIGN_RIGHT )
		numberOfLoopsPanel.setWidth("100%")
		
		aimPointPanel=HorizontalPanel()
		aimPointPanel.add( HTML("Aim point") )
		self.aimPoint=TextBox()
		self.aimPoint.setText(127)
		aimPointPanel.add( self.aimPoint )
		aimPointPanel.setCellHorizontalAlignment( self.aimPoint, HasHorizontalAlignment.ALIGN_RIGHT )
		aimPointPanel.setWidth("100%")

		self.start=Button("Start")
		
		self.echo=HTML("Initiating...")
		
		self.mainPanel = VerticalPanel()
		self.mainPanel.add( numberOfLoopsPanel )
		self.mainPanel.add( aimPointPanel )
		self.mainPanel.add( self.start )
		self.mainPanel.add( self.echo )

	def getMaxNumberOfLoops( self ) :
		return int(self.maximumNumberOfLoops.getText())

	def getAimPoint( self ) :
		return int(self.aimPoint.getText())

	def getStartButton( self ) :
		return self.start

	def setEchoText( self, text ) :
		self.echo.setText( text )

	def enable( self ) :
		self.start.setEnabled(True)

	def disable( self ) :
		self.start.setEnabled(False)

	def getPanel( self ) :
		return self.mainPanel

class CalibrateChannelTrimsPanel(object) :
	"""
	Class that takes care of the logic. The presenter in the MVP pattern.
	"""
	def __init__( self ) :
		self.view=CalibrateChannelTrimsView()
		# Bind to the start button
		self.view.getStartButton().addClickListener( self )
		# Register to receive notifications of when data taking starts
		self.dataRunManager = DataRunManager.instance()
		self.dataRunManager.registerEventHandler( self )

	def onDataTakingEvent( self, eventCode, details ) :
		"""
		Method that receives updates from DataRunManager
		"""
		if eventCode==DataRunManager.DataTakingStartedEvent :
			self.view.setEchoText("Taking data... ")
			self.view.disable()
		elif eventCode==DataRunManager.DataTakingFinishedEvent :
			self.view.setEchoText("Data taking finished")
			self.view.enable()
		elif eventCode==DataRunManager.DataTakingStatusEvent :
			self.view.setEchoText("%3d%% - "%int(details['fractionComplete']*100+0.5)+details['statusString'] )

	def onClick( self, sender ) :
		if sender==self.view.getStartButton() :
			self.dataRunManager.startTrimCalibration( self.view.getAimPoint(), self.view.getMaxNumberOfLoops() )

	def getPanel( self ) :
		return self.view.getPanel()
