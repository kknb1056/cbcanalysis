import FWCore.ParameterSet.Config as cms


# process declaration
process = cms.Process("HLT")

process.TFileService = cms.Service("TFileService",
	fileName = cms.string("/home/xtaldaq/testHistograms.root")
)

process.load("FWCore.MessageLogger.MessageLogger_cfi") 
process.MessageLogger = cms.Service("MessageLogger", 
	destinations = cms.untracked.vstring('cout','log4cplus'), 
	cout = cms.untracked.PSet(threshold = cms.untracked.string('WARNING')), 
	log4cplus = cms.untracked.PSet(
		INFO = cms.untracked.PSet( reportEvery = cms.untracked.int32(1) ),
		threshold = cms.untracked.string('WARNING')
	) 
)


process.source = cms.Source("DaqSource",
	readerPluginName = cms.untracked.string('FUShmReader'),
	evtsPerLS = cms.untracked.uint32(1000000)
)

#process.load("MarksAnalysers.CBCAnalyser.AnalyseCBCOutput_cfi")
process.AnalyseCBCOutput = cms.EDAnalyzer("AnalyseCBCOutput",
	trimFilename = cms.string("/tmp/i2CFileToSendToBoard.txt"),
	savedStateFilename = cms.untracked.string("/tmp/savedState.log"),
	commsServerHostname = cms.untracked.string("127.0.0.1"),
	commsServerPort = cms.untracked.string("4000"),
	debug = cms.untracked.bool(True)
)

process.DQM = cms.EDAnalyzer("OccupancyDQM",
	eventsToRecord = cms.uint32(100),
	commsServerHostname = cms.untracked.string("127.0.0.1"),
	commsServerPort = cms.untracked.string("4001")
)

process.analysisPath = cms.Path(
		process.DQM+
		process.AnalyseCBCOutput
	)


process.consumer = cms.OutputModule("ShmStreamConsumer",
	compression_level = cms.untracked.int32(1),
	use_compression = cms.untracked.bool(True)
	)

process.outpath = cms.EndPath( process.consumer )

