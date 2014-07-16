from ROOT import TFile, TEfficiency, TCanvas, gStyle, gROOT
import sys

def plotSCurves( input, cbcChannelRange, outputFilename=None, title="" ) :
	"""
	Plots the s-curves contained in the given file. If input is a string it is
	treated as a filename and the root file at the given location loaded. Otherwise
	it is assumed to be a valid root file object.
	
	The cbcChannelRange is given as an array of arrays. The first entry is an array
	of channel numbers to plot for CBC 0, the second an array of channel numbers to
	plot for CBC 1 etcetera. If you don't want any channels for a CBC insert an
	empty array. For example
	@code
	cbcChannelRange=[ [], range(0,254) ]
	@endcode
	will plot nothing for CBC 0 and all of the CBC 1 channels.
	
	If outputFilename is provided then the plot is saved there, using root's
	standard filetype from the extension calculation (e.g. if it ends in ".png"
	it will be a PNG file).
	
	The canvas is returned.

	@author Mark Grimes (mark.grimes@bristol.ac.uk) with a lot of help from Emyr
	Clement to get rid of the stat boxes.
	@date 02/Feb/2014
	"""
	if input.__class__=="random string".__class__ :
		# input appears to be a string so assume it is a filename
		inputFile=TFile.Open(input)
	else :
		# input is not a string, so assume it is a valid root file object
		inputFile=input
		
	canvas=TCanvas()
	drawOption=""
	for cbcIndex in range( 0, len(cbcChannelRange) ) :
		for channelNumber in cbcChannelRange[cbcIndex] :
			path="/CBC %02d/Strip %03d"%(cbcIndex, channelNumber)
			efficiency=inputFile.Get(path)
			if efficiency!=None :
				efficiency.SetTitle(title)
				efficiency.Draw(drawOption)
				# Make sure everything after the first one has "same"
				drawOption="same"

			else : print "Couldn't get histogram '"+path+"'"

	# Now loop back over and remove the statboxes with the fit parameters.
	# I couldn't do this before because the PaintedGraphs are not always available
	# until I call Update on the canvas
	canvas.Update()
	listOfPrimitives=canvas.GetListOfPrimitives()
	for index in range(0,listOfPrimitives.GetSize()) :
		primitive=listOfPrimitives[index]
		if primitive.ClassName()=="TEfficiency" :
			paintedHistogram=primitive.GetPaintedGraph()
			statBox=paintedHistogram.GetListOfFunctions().FindObject('stats')
			# Haven't figured out how to delete this, so clear the text and make the
			# box invisible.
			statBox.Clear()
			statBox.SetFillStyle(0)
			statBox.SetBorderSize(0)
			statBox.SetOptFit(0000)
			statBox.Clear('')

	canvas.Update()
	if outputFilename!=None : canvas.SaveAs( outputFilename )

if __name__=="__main__" :
	# Only want to allow one argument for the filename, but I also want the user
	# to be able to specify the "-b" option to enable batch mode.
	if len(sys.argv)==2 : inputFilename=sys.argv[1]
	elif len(sys.argv)==3 and sys.argv[1]=="-b": inputFilename=sys.argv[2]
	else : raise Exception('Incorrect number of arguments. The histogram filename should be the only argument')
	
	inputFile=TFile.Open(inputFilename)
	outputDirectory="/home/phmag/CMSSW_5_3_4/src/SLHCUpgradeTracker/CBCAnalysis/gui/output/"
	plotSCurves( inputFile, [ range(0,254), [] ], outputDirectory+"histogramsForCBC0.png", "CBC 0" )
	plotSCurves( inputFile, [ [], range(0,254) ], outputDirectory+"histogramsForCBC1.png", "CBC 1" )
	
