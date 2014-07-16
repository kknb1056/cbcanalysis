#ifndef SLHCUpgradeTracker_CBCAnalysis_interface_SCurve_h
#define SLHCUpgradeTracker_CBCAnalysis_interface_SCurve_h

#include <vector>
#include <map>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <functional>

//
// Forward declarations
//
class TEfficiency;
class TF1;
class TDirectory;

namespace cbcanalyser
{
	/** @brief Given a sorted list of histogram centres, this function will calculate the bin lower edges.
	 *
	 * Useful for creating the array that should be passed to root histogram constructors for variably
	 * binned histograms. For example:
	 * @code
	 * std::vector<double> binCentres;
	 * // binCentres will be filled somehow here
	 * std::vector<double> binLowerEdges; // results will be stored in here
	 * calculateBinning( binLowerEdges, binCentres );
	 * TH1* pNewHistogram=new TH1D( "histogramName", "Histogram title", binLowerEdges.size()-1, &binLowerEdges[0] );
	 * @endcode
	 *
	 * Maps and other non-simple containers can also by used by supplying a retrieval function as the
	 * third argument. This takes an iterator for the input container and returns the axis value. For
	 * example using a lambda function:
	 * @code
	 * std::map<float,SomeValueClass> dataToHistogram; // the key value is the one that will be on the histogram x-axis
	 * // fill dataToHistogram
	 * std::vector<double> binLowerEdges; // results will be stored in here
	 * calculateBinning( binLowerEdges, dataToHistogram, [std::map<float,SomeValueClass>::const_iterator& iFloatClassPair]{return iFloatClassPair->first;} );
	 * TH1* pNewHistogram=new TH1D( "histogramName", "Histogram title", binLowerEdges.size()-1, &binLowerEdges[0] );
	 * @endcode
	 *
	 * If the input is empty then a std::runtime_error is thrown. If there is only one entry then
	 * binning is created for one bin of width 1 centred around the entry. If the input is such that
	 * putting bin edges equally between successive centres would result in bins not centred at the
	 * required point (i.e. distance from the requested centre on the left is not the same as on the
	 * right), then a "dummy" bin is added to fill the gap.
	 *
	 * @param outputBinLowerEdges  A container where the bin lower edges plus the global upper edge will
	 *                             be stored. This will generally be a std::vector<double> but doesn't
	 *                             have to be, as long as it has a push_back method.
	 * @param inputBinCentres      A container containing the points where each bin needs to be centred.
	 *                             This is assumed to be already sorted in increasing value.
	 * @param retriever            Optional. A std::function that will return the bin centre value given
	 *                             an iterator from inputBinCentres. Only needs to be supplied if
	 *                             inputBinCentres is not a simple container.
	 * @post                       outputBinLowerEdges will be filled with the lower edges of each bin,
	 *                             followed by the upper edge of the entire histogram. Hence the number
	 *                             of bins will be outputBinLowerEdges.size()-1. Note that this could be
	 *                             larger than the number of bin centres supplied, since some extra bins
	 *                             might need to added to ensure the bins are centred correctly.
	 *
	 * Random aside: I wanted this function as a static method in SCurve but it causes an internal
	 * compiler error on gcc 4.6.2, but not as a global function.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 29/Oct/2013
	 */
	template<class T_outputContainer, class T_inputContainer, class T_retriever>
	static void calculateBinning( T_outputContainer& outputBinLowerEdges, const T_inputContainer& inputBinCentres, T_retriever retriever );

	/** @brief Version of calculateBinning that provides a default T_retriever for when T_inputContainer is a vector
	 *
	 * See the notes for the other overload of calculateBinning.
	 */
	template<class T_outputContainer, class T_inputContainer>
	static void calculateBinning( T_outputContainer& outputBinLowerEdges, const T_inputContainer& inputBinCentres );


	/** @brief Class to record data for a single bin in an s-curve.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Sep/2013
	 */
	class SCurveEntry
	{
	public:
		SCurveEntry();
		bool operator==( const SCurveEntry& otherSCurveEntry ) const;
		bool operator!=( const SCurveEntry& otherSCurveEntry ) const;

		size_t& eventsOn();
		const size_t& eventsOn() const;
		size_t& eventsOff();
		const size_t& eventsOff() const;
		/** @brief Returns the fraction of events where the channel was on. */
		float fraction() const;
		/** @brief Returns simple error assuming poisson error on the number of events on, and no error on the total. */
		float fractionError() const;

		void dumpToStream( std::ostream& outputStream ) const;
		void restoreFromStream( std::istream& inputStream );
	protected:
		size_t eventsOn_;
		size_t eventsOff_;
	};

	/** @brief Class to record data to calculate an s-curve.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Sep/2013
	 */
	class SCurve
	{
	public:
		SCurve();
		bool operator==( const SCurve& otherSCurve ) const;
		bool operator!=( const SCurve& otherSCurve ) const;

		SCurveEntry& getEntry( float threshold );
		const SCurveEntry& getEntry( float threshold ) const;
		/** @brief Returns a vector of the thresholds that this SCurve has data for */
		const std::vector<float> getValidThresholds() const;
		/** @brief Returns the number of bins in the s-curve. */
		size_t size() const;

		/** @brief Creates a new root histogram and fills it with the s-curve data.
		 *
		 * The parent directory will be set to nullptr, so unless you call SetDirectory
		 * on the result the histogram will be held in memory only.
		 */
		std::unique_ptr<TEfficiency> createHistogram( const std::string& name ) const;

		/** @brief Restores from the TEfficiencies found in the directory supplied. */
		void restoreFromEfficiency( const TEfficiency* pEfficiency );

		/** @brief Fits the s-curve with an error function and returns the TF1.
		 */
		std::unique_ptr<TF1> fit() const;

		/** @brief Returns the fit parameters for the s-curve.
		 *
		 * The returned std::tuple contains the three fit parameters. These are: </br>
		 * tuple entry 0 - The chi2 of the fit
		 * tuple entry 1 - The number of degrees of freedom
		 * tuple entry 2 - The maximum efficiency
		 * tuple entry 3 - The standard deviation
		 * tuple entry 4 - The mean
		 */
		std::tuple<float,float,float,float,float> fitParameters() const;


		void dumpToStream( std::ostream& outputStream ) const;
		void restoreFromStream( std::istream& inputStream );

		/// @brief Returns the number of entries possible. I.e. any call to getEntry should be in the range 0 to this value-1
		size_t maxiumumEntries();

		static std::unique_ptr<TF1> fitHistogram( const std::unique_ptr<TEfficiency>& pHistogram );
	protected:
		std::map<float,SCurveEntry> entries_;

		/** @brief Stores parameters of function passed to it
		 *
		 * Expects function with exactly 3 parameters.
		 */
		void storeFitParameters ( const TF1& fittedFunction );

		// Fit results
		float fit_chi2_;
		float fit_ndf_;
		float fit_maxEfficiency_;
		float fit_standardDeviation_;
		float fit_mean_;


	};

//	/** @brief Class to fit SCurve
//	 *
//         * @author Emyr Clement (mark.grimes@bristol.ac.uk)
//         * @date 17/Oct/2013
//	 */
//	class FitSCurve
//	{
//	public:
//	    FitSCurve( TEfficiency & sCurve, const std::string& name );
//
//            /** @brief Performs a fit of the fitFunction_ to sCurveToFit_
//             *
//             */
//            std::unique_ptr<TF1> performFit() const;
//
//	protected:
//            // The sCurve to fit
//            TEfficiency & sCurveToFit_;
//            // The function to fit
//            TF1 *fitFunction_;
//	};

	/** @brief Convenience class to store all the s-curves for a FED channel.
	 *
	 * Could have just used a raw std::map but this way makes the intention in the code a bit
	 * clearer.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Sep/2013
	 */
	class FedChannelSCurves
	{
	public:
		SCurve& getStripSCurve( size_t stripNumber );
		const SCurve& getStripSCurve( size_t stripNumber ) const;
		/** @brief Returns a vector of the strip indices that have data recorded for them. */
		std::vector<size_t> getValidStripIndices() const;

		/** @brief Creates a series of sub-directories for histograms for all of the s-curves. */
		void createHistograms( TDirectory* pParentDirectory ) const;

		/** @brief Restores from the TEfficiencies found in the directory supplied. */
		void restoreFromDirectory( TDirectory* pParentDirectory );

		void dumpToStream( std::ostream& outputStream ) const;
		void restoreFromStream( std::istream& inputStream );
	protected:
		std::map<size_t,SCurve> stripSCurves_;
	};

	/** @brief Convenience class to store all the s-curves for a single FED.
	 *
	 * Could have just used a series of raw std::map but this way makes the intention in the
	 * code a bit clearer.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Sep/2013
	 */
	class FedSCurves
	{
	public:
		FedChannelSCurves& getFedChannelSCurves( size_t fedChannelNumber );
		const FedChannelSCurves& getFedChannelSCurves( size_t fedChannelNumber ) const;
		SCurve& getStripSCurve( size_t fedChannelNumber, size_t stripNumber );
		/** @brief Returns a vector of the channel indices that have data recorded for them. */
		std::vector<size_t> getValidChannelIndices() const;

		/** @brief Creates a series of sub-directories for histograms for all of the s-curves. */
		void createHistograms( TDirectory* pParentDirectory ) const;

		/** @brief Restores from the TEfficiencies found in the directory supplied. */
		void restoreFromDirectory( TDirectory* pParentDirectory );

		void dumpToStream( std::ostream& outputStream ) const;
		void restoreFromStream( std::istream& inputStream );
	protected:
		std::map<size_t,FedChannelSCurves> fedChannelSCurves_;
	};

	/** @brief Convenience class to store all the s-curves for all of the FEDs.
	 *
	 * Could have just used a series of raw std::map but this way makes the intention in the
	 * code a bit clearer.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 04/Sep/2013
	 */
	class DetectorSCurves
	{
	public:
		FedSCurves& getFedSCurves( size_t fedNumber );
		FedChannelSCurves& getFedChannelSCurves( size_t fedNumber, size_t fedChannelNumber );
		SCurve& getStripSCurve( size_t fedNumber, size_t fedChannelNumber, size_t stripNumber );
		/** @brief Returns a vector of the FED indices that have data recorded for them. */
		std::vector<size_t> getValidFedIndices() const;

		/** @brief Creates a series of sub-directories for histograms for all of the s-curves. */
		void createHistograms( TDirectory* pParentDirectory ) const;

		void dumpToStream( std::ostream& outputStream ) const;
		void restoreFromStream( std::istream& inputStream );
	protected:
		std::map<size_t,FedSCurves> fedSCurves_;
	};

} // end of namespace cbcanalyser


//
// Template definitions that can't go in the source file
//
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <list>
template<class T_outputContainer, class T_inputContainer, class T_retriever>
void cbcanalyser::calculateBinning( T_outputContainer& outputBinLowerEdges, const T_inputContainer& inputBinCentres, T_retriever retriever )
{
	// Create a typedef for the type of the axis values to make life easier
	typedef typename T_outputContainer::value_type value_type;

	if( inputBinCentres.empty() ) throw std::runtime_error( "cbcanalyser::calculateBinning called with empty input" );
	if( inputBinCentres.size()==1 ) // This edge case messes up the logic, so handle it separately
	{
		outputBinLowerEdges.clear();
		value_type binCentre=retriever( inputBinCentres.begin() );
		outputBinLowerEdges.push_back( binCentre-0.5 ); // Arbitrary bin width of 1
		outputBinLowerEdges.push_back( binCentre+0.5 );
		return;
	}

	// Create a function to test float equality by checking they're within a certain percentage of each other
	std::function<bool(value_type,value_type)> floatsAreEqual=[](value_type value1,value_type value2){ return std::fabs( 1-value1/value2 )<std::pow( 10, -4 ); };

	// Life is much easier if everything is in a vector because I need random access. I also want
	// somewhere to record the width of each bin. I might as well keep them in the same place so
	// I'll store both as a std::pair. "first" will be the bin centre, and "second" will be the
	// bin width. I'll use -1 to indicate an undefined bin width.
	std::vector< std::pair<value_type,value_type> > binCentresAndWidths;
	for( auto inputIterator=inputBinCentres.begin(); inputIterator!=inputBinCentres.end(); ++inputIterator )
	{
		// None of the bin widths are defined yet so put in a -1
		binCentresAndWidths.push_back( std::make_pair( retriever(inputIterator), -1 ) );
	}
	// Sort the container so that everything is in increasing order. I want to sort by "first" of the
	// pair so use a lambda function as the comparator.
	const auto compareFirstOfPair=[](std::pair<value_type,value_type> value1,std::pair<value_type,value_type> value2){ return value1.first<value2.first; };
	std::sort( binCentresAndWidths.begin(), binCentresAndWidths.end(), compareFirstOfPair );

	// I need to make up the bins in order of increasing bin width. To do this I need
	// to know what the difference between each bin centre is, so I'll work that out and
	// put the result as well as the array indices of the relevant bins in a list. I want
	// to use a list because as soon as I've set the bin size for those bins I want to
	// remove the entry.
	// The entries in this tuple will be [the distance between bins, left bin index, right bin index].
	// Storing both bin indices might seem like duplication (since they have to be next to each other)
	// but I later want to add constraints to affect only one bin. When this happens I want to put in
	// -1 for the index of the other bin to represent this.
	//typedef std::tuple<value_type,int,int> diff_tuple;
	typedef std::pair<value_type,int> diff_tuple;
	std::list<diff_tuple> binConstraints;
	for( size_t index=1; index<binCentresAndWidths.size(); ++index ) // Start at one because I need to check against the previous bin
	{
		// What I'll actually store is half the difference between the bins. This is because
		// once I've set a bin width, I might want to add an entry to constrain further binning
		// from overlapping the bins I've just set.
		value_type leftBinCentre=binCentresAndWidths[index-1].first;
		value_type rightBinCentre=binCentresAndWidths[index].first;
		//binConstraints.push_back( diff_tuple( (rightBinCentre-leftBinCentre)/2.0, index-1, index ) );
		binConstraints.push_back( std::make_pair( (rightBinCentre-leftBinCentre)/2.0, index-1 ) );
	}

	bool needToReSort=true;
	while( !binConstraints.empty() )
	{
		// I now need to sort the list so that I first work on the smallest distance between bins first
		if( needToReSort )
		{
			binConstraints.sort( [](diff_tuple& leftValue,diff_tuple& rightValue){ return leftValue.first<rightValue.first; } );
			needToReSort=false;
		}

		auto iSmallestDifference=binConstraints.begin(); // Get an interator because I want to remove it once I'm done
		value_type binHalfWidth=iSmallestDifference->first;
		int leftBinIndex=iSmallestDifference->second;
		int rightBinIndex=iSmallestDifference->second+1;

		//
		// First look at the bin on the left of this constraint
		//
		if( binCentresAndWidths[leftBinIndex].second==-1 ) // "if left bin width has not been set"
		{
			// Set the bin width
			binCentresAndWidths[leftBinIndex].second=binHalfWidth;
			// Now that's set, if there is a still a constraint on the bin to the
			// left of this then I need to change that.
			if( leftBinIndex>0 )
			{
				// There is a bin to the left, see if I can find a constraint for it.
				auto iFindResult=std::find_if( binConstraints.begin(), binConstraints.end(), [&leftBinIndex](diff_tuple& value)->bool{ if( value.second==leftBinIndex-1 ) return true; else return false; } );
				if( iFindResult!=binConstraints.end() )
				{
					// If the width for this bin has already been set then I can remove this constraint
					// completely because the bins on either side have been set.
					if( binCentresAndWidths[leftBinIndex-1].second!=-1 ) binConstraints.erase( iFindResult );
					else
					{
						// I can allow this bin to be bigger than the midpoint between the two, since the
						// bin on the right is smaller (or could potentially be the same).
						// This expression says (consider that leftBinIndex is now the right bin and leftBinIndex-1
						// is now the left bin) leftBinPotentialHalfWidth=rightBinCentre-rightBinHalfWidth-leftBinCentre
						iFindResult->first=binCentresAndWidths[leftBinIndex].first-binHalfWidth-binCentresAndWidths[leftBinIndex-1].first;
						// I might have changed the ordering so I need to resort
						needToReSort=true;
					}
				} // end of "if a constraint was found for the bin to the left of these two"
			} // end of "if there is another bin to the left"
		} // end of "if left bin width has not been set"

		//
		// Now look at the bin on the right of this constraint
		//
		if( binCentresAndWidths[rightBinIndex].second==-1 )
		{
			// Set the bin width
			binCentresAndWidths[rightBinIndex].second=binHalfWidth;
			// Now that's set, if there is a still a constraint on the bin to the
			// right of this then I need to change that.
			if( rightBinIndex<static_cast<int>(binCentresAndWidths.size()-1) )
			{
				// There is a bin to the right, see if I can find a constraint for it
				auto iFindResult=std::find_if( binConstraints.begin(), binConstraints.end(), [&rightBinIndex](diff_tuple& value)->bool{ if( value.second==rightBinIndex ) return true; else return false; } );
				if( iFindResult!=binConstraints.end() )
				{
					// If the width for this bin has already been set then I can remove this constraint
					// completely because the bins on either side have been set.
					if( binCentresAndWidths[rightBinIndex+1].second!=-1 ) binConstraints.erase( iFindResult );
					else
					{
						// I can allow this bin to be bigger than the midpoint between the two, since the
						// bin on the left is smaller (or could potentially be the same).
						// This expression says (consider that rightBinIndex is now the left bin and rightBinIndex+1
						// is now the right bin) rightBinPotentialHalfWidth=rightBinCentre-leftBinCentre-leftBinHalfWidth
						iFindResult->first=binCentresAndWidths[rightBinIndex+1].first-binCentresAndWidths[rightBinIndex].first-binHalfWidth;
						// I might have changed the ordering of smallest to largest so I need to resort
						needToReSort=true;
					}
				} // end of "if a constraint was found for the bin on the right of these two"
			} // end of "if there is another bin to the right"
		} // end of "if right bin width has not been set"

		// I've set the widths of the bins on either side of this constraint, so I can now remove it
		binConstraints.erase( iSmallestDifference );
	}

	//
	// I should now have all of the bin widths set, so I can convert these into
	// absolute values on the axis and store them in the return value.
	//
	outputBinLowerEdges.clear();
	// Logic in the for loop assumes an entry, so manually push the first one
	outputBinLowerEdges.push_back( binCentresAndWidths[0].first-binCentresAndWidths[0].second );
	for( const auto& centreAndWidthPair : binCentresAndWidths )
	{
		value_type& lastEntry=outputBinLowerEdges.back();
		value_type newBinLeftEdge=centreAndWidthPair.first-centreAndWidthPair.second;
		value_type newBinRightEdge=centreAndWidthPair.first+centreAndWidthPair.second;
		// The high edge of the previous bin might not actually touch this bin. If it does then
		// I only need to add one of the edges. If it doesn't then I'll push both; this has the
		// effect of creating a "dummy" bin as a spacer.
		// These types could be floats, so I'll check they're within an arbitrary percentage of
		// each other rather than using the equality operator.
		if( std::fabs( 1-lastEntry/newBinLeftEdge )>std::pow( 10, -4 ) ) outputBinLowerEdges.push_back( newBinLeftEdge );
		outputBinLowerEdges.push_back( newBinRightEdge );
	}

}

template<class T_outputContainer, class T_inputContainer>
void cbcanalyser::calculateBinning( T_outputContainer& outputBinLowerEdges, const T_inputContainer& inputBinCentres )
{
	calculateBinning( outputBinLowerEdges, inputBinCentres, [](typename T_inputContainer::const_iterator iValue){return *iValue;} );
}

#endif
