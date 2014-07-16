#ifndef SLHCUpgradeTracker_CBCAnalysis_plugins_AnalyseCBCOutput_h
#define SLHCUpgradeTracker_CBCAnalysis_plugins_AnalyseCBCOutput_h

#include <fstream>
#include <atomic>
#include <FWCore/Framework/interface/Frameworkfwd.h>
#include <FWCore/Framework/interface/EDAnalyzer.h>
#include "SLHCUpgradeTracker/CBCAnalysis/interface/SCurve.h"
#include "SLHCUpgradeTracker/CBCAnalysis/interface/HttpServer.h"

//
// Forward declarations
//
class TH1;

namespace cbcanalyser
{
	/** @brief Analyser to look over the CBC output written by the GlibStreamer XDAQ plugin.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 08/May2013
	 */
	class AnalyseCBCOutput : public edm::EDAnalyzer, public httpserver::HttpServer::IRequestHandler
	{
	public:
		explicit AnalyseCBCOutput( const edm::ParameterSet& config );
		AnalyseCBCOutput( const cbcanalyser::AnalyseCBCOutput& otherAnalyser ) = delete;
		AnalyseCBCOutput( cbcanalyser::AnalyseCBCOutput&& otherAnalyser ) = delete;
		AnalyseCBCOutput& operator=( const cbcanalyser::AnalyseCBCOutput& otherAnalyser ) = delete;
		AnalyseCBCOutput& operator=( cbcanalyser::AnalyseCBCOutput&& otherAnalyser ) = delete;

		~AnalyseCBCOutput();

		static void fillDescriptions( edm::ConfigurationDescriptions& descriptions );
	private:
		virtual void beginJob();
		virtual void analyze( const edm::Event& event, const edm::EventSetup& setup );
		virtual void endJob();

		virtual void beginRun( const edm::Run& run, const edm::EventSetup& setup );
		virtual void endRun( const edm::Run& run, const edm::EventSetup& setup );
		virtual void beginLuminosityBlock( const edm::LuminosityBlock& lumiBlock, const edm::EventSetup& setup );
		virtual void endLuminosityBlock( const edm::LuminosityBlock& lumiBlock, const edm::EventSetup& setup );

		/** @brief The handler that server_ will call when a HTTP request comes in. */
		virtual void handleRequest( const httpserver::HttpServer::Request& request, httpserver::HttpServer::Reply& reply );
	protected:
		/** @brief Method called by handleRequest to handle a request for "scurveFits". */
		void request_scurveFits( httpserver::HttpServer::Reply& reply );

		/** @brief Method called by handleRequest to handle a request for "createFakeData". */
		void request_createFakeData( httpserver::HttpServer::Reply& reply );

		/** @brief Method called by handleRequest to handle a request for "changeVar". */
		void request_changeVar( httpserver::HttpServer::Reply& reply, const std::vector< std::pair<std::string,std::string> >& parameters );

		/** @brief Save the current state to disk so that another AnalyseCBCOutput can be restored to the same state.
		 *
		 * I've been having a lot of problems with the DAQ runcontrol. The only way I can take multiple runs (as of
		 * 04/Sep/2013) is to kill all the processes and start again, so I need a way of restoring the analyser's
		 * state after a run.
		 */
		void saveState( const std::string& filename );
		/** @brief Restore to the state that was saved to a call to saveState.
		 *
		 * See the note on saveState for an explanation of why this is necassary.
		 */
		void restoreState( const std::string& filename );
		/** @brief Filename to save the state to. Optional - if empty the state is not restored or saved to disk. */
		std::string savedStateFilename_;

		DetectorSCurves detectorSCurves_;

		/** @brief Dumps the s-curves to the output stream for debugging */
		void dumpSCurveToStream( std::ostream& output );

		/** @brief Reads strip threshold offsets from the filename stored in I2CValuesFilename_ and store them
		 * in stripThresholdOffsets_.
		 *
		 * @post  stripThresholdOffsets_ is overwritten with any entries in the file specified in I2CValuesFilename_.
		 */
		void readI2CValues();
		std::string I2CValuesFilename_;
		std::vector<unsigned int> stripThresholdOffsets_;
		//std::atomic<float> globalComparatorThreshold_; /// @brief Atomic because the handleRequest method can change it from a different thread.
		float globalComparatorThreshold_; /// @brief Should be std::atomic but it won't link (compiles though). I think the compiler is too old.

		size_t eventsProcessed_;
		size_t runsProcessed_;

		class cbcanalyser::SCurveEntry* pSCurveEntryToMonitorForDQM_;
		httpserver::HttpServer server_;
		bool debug_; ///< @brief Whether or not to print lots of debug messages.
	};

} // end of namespace cbcanalyser

#endif
