#ifndef SLHCUpgradeTracker_CBCAnalysis_interface_CBC2ChannelUnpacker_h
#define SLHCUpgradeTracker_CBCAnalysis_interface_CBC2ChannelUnpacker_h

#include <vector>

//
// Forward declarations
//
class FEDRawData;
namespace sistrip
{
	class FEDChannel;
}


namespace cbcanalyser
{
	/** @brief Simple utility class to unpack the bits from the CBC2 and present the result as a 254 bool vector.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 28/May/2013
	 */
	class CBC2ChannelUnpacker
	{
	public:
		CBC2ChannelUnpacker( const FEDRawData& rawData );
		CBC2ChannelUnpacker( const sistrip::FEDChannel& fedChannel );
		const std::vector<bool>& hits() const;
		bool hasData() const;
	private:
		bool hasData_;
		std::vector<bool> hits_;
	};
}

#endif
