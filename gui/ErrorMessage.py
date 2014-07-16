from pyjamas import Window
from pyjamas.ui.Button import Button
from pyjamas.ui.HTML import HTML
from pyjamas.ui.VerticalPanel import VerticalPanel
from pyjamas.ui.DialogBox import DialogBox


class ErrorMessage :
	"""
	Simple class to show a message that blocks the screen until the user
	clicks the "Close" button.
	
	Note that this gets its style from the CSS styles ".gwt-DialogBox .Caption",
	".gwt-DialogBox .Contents" and ".gwt-PopupPanelGlass"
	
	@author Mark Grimes (mark.grimes@bristol.ac.uk)
	@date 17/Jan/2014
	"""
	def __init__( self, message, messageTitle="Error" ) :
		self.dialog = DialogBox(glass=True)
		self.dialog.setHTML('<b>'+messageTitle+'</b>')
		dialogContents=VerticalPanel( StyleName="Contents", Spacing=4 )
		dialogContents.add( HTML(message) )
		dialogContents.add( Button('Close',getattr(self, "onClose")) )
		self.dialog.setWidget(dialogContents)
		left = (Window.getClientWidth() - 200) / 2 + Window.getScrollLeft()
		top = (Window.getClientHeight() - 100) / 2 + Window.getScrollTop()
		self.dialog.setPopupPosition(left, top)
		self.dialog.show()

	def onClose( self, event ):
		self.dialog.hide()
