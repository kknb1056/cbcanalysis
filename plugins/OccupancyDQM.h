#ifndef SLHCUpgradeTracker_CBCAnalysis_plugins_OccupancyDQM_h
#define SLHCUpgradeTracker_CBCAnalysis_plugins_OccupancyDQM_h

#include <mutex>
#include <atomic>
#include <FWCore/Framework/interface/Frameworkfwd.h>
#include <FWCore/Framework/interface/EDAnalyzer.h>
#include "SLHCUpgradeTracker/CBCAnalysis/interface/HttpServer.h"


namespace cbcanalyser
{
	/** @brief Analyser that records a rolling occupancy for the last <n> events and presents the results
	 * on a webpage.
	 *
	 * The hostname and port to try and run the server on is given in the config ParameterSet, as well
	 * as the number of events to calculate the occupancy for.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 09/Oct/2013
	 */
	class OccupancyDQM : public edm::EDAnalyzer, public httpserver::HttpServer::IRequestHandler
	{
	public:
		explicit OccupancyDQM( const edm::ParameterSet& config );
		~OccupancyDQM();

		OccupancyDQM( const cbcanalyser::OccupancyDQM& otherAnalyser ) = delete;
		OccupancyDQM( cbcanalyser::OccupancyDQM&& otherAnalyser ) = delete;
		OccupancyDQM& operator=( const cbcanalyser::OccupancyDQM& otherAnalyser ) = delete;
		OccupancyDQM& operator=( cbcanalyser::OccupancyDQM&& otherAnalyser ) = delete;
	private:
		virtual void analyze( const edm::Event& event, const edm::EventSetup& setup );

		/// @brief The handler that server_ will call when a HTTP request comes in. Required by the IRequestHandler interface.
		virtual void handleRequest( const httpserver::HttpServer::Request& request, httpserver::HttpServer::Reply& reply );
	protected:
		httpserver::HttpServer server_;
		std::mutex serverMutex_; ///< Mutex to stop the server reading while the analyze method is writing.
	private:
		// I've got a few utility classes that are only visible in the .cc file, so
		// I need to use a pImple.
		std::unique_ptr<class OccupancyDQMPrivateMembers> pImple;
		std::atomic<size_t> numberOfEvents_;
		std::string hostname_;
		std::string port_;
	};

} // end of namespace cbcanalyser

#endif
