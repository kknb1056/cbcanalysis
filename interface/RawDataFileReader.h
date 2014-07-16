#ifndef SLHCUpgradeTracker_CBCAnalysis_interface_RawDataFileReader_h
#define SLHCUpgradeTracker_CBCAnalysis_interface_RawDataFileReader_h

#include <vector>
#include <iosfwd>
#include <memory>

namespace cbcanalyser
{
	/** @brief Interface for the data on a single CBC for a single event.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 06/Jan/2014
	 */
	class RawCBCEvent
	{
	public:
		virtual ~RawCBCEvent() {}
		virtual const std::vector<bool>& errorBits() = 0;
		virtual unsigned char status() = 0;
		virtual const std::vector<bool>& channelData() = 0;
		virtual unsigned char stubData() = 0;
	};

	/** @brief Interface for events held in the raw data format.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 06/Jan/2014
	 */
	class RawDataEvent
	{
	public:
		virtual ~RawDataEvent() {}
		virtual size_t bunchCounter() = 0;
		virtual size_t orbitCounter() = 0;
		virtual size_t lumisection() = 0;
		virtual size_t l1aCounter() = 0;
		virtual size_t cbcCounter() = 0;

		/** @brief Provides the data for each CBC chip.
		 *
		 * @param cbcNumber The identifier for the CBC you want data for, i.e. a number between 0 and 3.
		 *                  The CBCs on FMC 1 are numbered 0 and 1, and the two on FMC 2 are numbered 2
		 *                  and 3. Note that if an FMC is not present the GlibStreamer still puts in data
		 *                  which appears as all channels always on.
		 * @return          A RawCBCEvent structure representing the data from the requested CBC for the
		 *                  current event. */
		virtual RawCBCEvent& cbc( size_t cbcNumber ) = 0;
	};

	/** @brief Class that will read the raw dumps from GlibStreamer and present the data with a simple interface.
	 *
	 * Note that this is for the old file format, not for format described as "New DAQ format" by GlibStreamer.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 05/Jan/2014
	 */
	class RawDataFileReader
	{
	public:
		RawDataFileReader( std::istream& inputFile );
		/** @brief Returns an objecting representing the next event. When there are no more events returns nullptr. */
		std::unique_ptr<RawDataEvent> nextEvent();
	protected:
		std::istream& inputFile_;
	};

}

#endif
