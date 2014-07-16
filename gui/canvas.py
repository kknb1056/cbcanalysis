from pyjamas.ui.Composite import Composite

class Canvas:

    def __init__(self, theCanvas):
        self.height = 400
        self.width = 400
        self.canvas = theCanvas
        self.controls = None

    def getControls(self):
        if self.controls is None:
            self.createControls()

        return self.controls


    def getName(self):
        return self.canvasName