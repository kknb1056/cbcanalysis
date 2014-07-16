#ifndef SLHCUpgradeTracker_CBCAnalysis_interface_CBCChannelUnpacker_h
#define SLHCUpgradeTracker_CBCAnalysis_interface_CBCChannelUnpacker_h

#include <vector>

//
// Forward declarations
//
namespace sistrip
{
	class FEDChannel;
}


namespace cbcanalyser
{
	/** @brief Simple utility class to unpack the bits from the CBC1 and present the result as a 128 bool vector.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 28/May/2013
	 */
	class CBCChannelUnpacker
	{
	public:
		CBCChannelUnpacker( const sistrip::FEDChannel& fedChannel );
		const std::vector<bool>& hits() const;
		bool hasData() const;
	private:
		bool hasData_;
		std::vector<bool> hits_;
	};
}

#endif
